#!/usr/bin/env python3
"""GoBall fleet monitoring agent — runs on each Raspberry Pi.

Publishes system metrics and parses goball.service logs via MQTT.
Reads configuration from /etc/goball-agent.conf or agent.conf in the same directory.
"""

import json
import logging
import os
import re
import signal
import socket
import subprocess
import time

import paho.mqtt.client as mqtt

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(levelname)s %(message)s")
log = logging.getLogger("goball-agent")

# --- Configuration ---

CONF_PATHS = [
    "/etc/goball-agent.conf",
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "agent.conf"),
]


def load_config() -> dict:
    """Load config from file, then overlay env vars."""
    cfg = {
        "MQTT_BROKER_HOST": "localhost",
        "MQTT_BROKER_PORT": "1883",
        "MQTT_USERNAME": "",
        "MQTT_PASSWORD": "",
        "VENUE_NAME": "",
        "DEVICE_LABEL": "",
        "LATITUDE": "0",
        "LONGITUDE": "0",
        "PUBLISH_INTERVAL": "30",
        "MQTT_TLS_CA": "",
        "MQTT_TLS_CERT": "",
        "MQTT_TLS_KEY": "",
        "FAN_ENABLED": "1",
        "FAN_TEMP_OFF": "40",
        "FAN_TEMP_LOW": "50",
        "FAN_TEMP_HIGH": "70",
        "FAN_PWM_MIN": "80",
        "FAN_PWM_MAX": "255",
    }
    # Read from first config file found
    for path in CONF_PATHS:
        if os.path.isfile(path):
            log.info("Loading config from %s", path)
            with open(path) as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith("#"):
                        continue
                    if "=" in line:
                        key, val = line.split("=", 1)
                        cfg[key.strip()] = val.strip()
            break
    else:
        log.warning("No config file found, using defaults + env vars")

    # Env vars override config file
    for key in cfg:
        env_val = os.environ.get(key)
        if env_val is not None:
            cfg[key] = env_val

    return cfg


CFG = load_config()

BROKER_HOST = CFG["MQTT_BROKER_HOST"]
BROKER_PORT = int(CFG["MQTT_BROKER_PORT"])
MQTT_USER = CFG["MQTT_USERNAME"]
MQTT_PASS = CFG["MQTT_PASSWORD"]
PUBLISH_INTERVAL = int(CFG["PUBLISH_INTERVAL"])
LATITUDE = float(CFG["LATITUDE"])
LONGITUDE = float(CFG["LONGITUDE"])
VENUE_NAME = CFG["VENUE_NAME"]
DEVICE_LABEL = CFG["DEVICE_LABEL"]
TLS_CA = CFG["MQTT_TLS_CA"]
TLS_CERT = CFG["MQTT_TLS_CERT"]
TLS_KEY = CFG["MQTT_TLS_KEY"]
FAN_ENABLED = CFG["FAN_ENABLED"] == "1"
FAN_TEMP_OFF = float(CFG["FAN_TEMP_OFF"])
FAN_TEMP_LOW = float(CFG["FAN_TEMP_LOW"])
FAN_TEMP_HIGH = float(CFG["FAN_TEMP_HIGH"])
FAN_PWM_MIN = int(CFG["FAN_PWM_MIN"])
FAN_PWM_MAX = int(CFG["FAN_PWM_MAX"])


# --- Fan control ---

_fan_hwmon_path: str | None = None
_fan_hwmon_searched = False
_fan_no_hw_warned = False


def find_fan_hwmon() -> str | None:
    """Scan /sys/class/hwmon for RPi5 PWM fan. Caches result."""
    global _fan_hwmon_path, _fan_hwmon_searched
    if _fan_hwmon_searched:
        return _fan_hwmon_path
    _fan_hwmon_searched = True
    try:
        for entry in os.listdir("/sys/class/hwmon"):
            name_path = f"/sys/class/hwmon/{entry}/name"
            try:
                with open(name_path) as f:
                    if f.read().strip() == "pwmfan":
                        _fan_hwmon_path = f"/sys/class/hwmon/{entry}"
                        log.info("Found fan hwmon at %s", _fan_hwmon_path)
                        return _fan_hwmon_path
            except OSError:
                continue
    except OSError:
        pass
    return None


def get_fan_pwm(hwmon_path: str | None) -> int:
    """Read current fan PWM duty (0-255). Returns -1 if unavailable."""
    if not hwmon_path:
        return -1
    try:
        with open(f"{hwmon_path}/pwm1") as f:
            return int(f.read().strip())
    except (OSError, ValueError):
        return -1


def get_fan_rpm(hwmon_path: str | None) -> int:
    """Read fan RPM from tachometer. Returns -1 if unavailable."""
    if not hwmon_path:
        return -1
    try:
        with open(f"{hwmon_path}/fan1_input") as f:
            return int(f.read().strip())
    except (OSError, ValueError):
        return -1


def set_fan_pwm(hwmon_path: str | None, value: int):
    """Write PWM duty cycle (0-255). Enables manual mode first."""
    if not hwmon_path:
        return
    value = max(0, min(255, value))
    try:
        with open(f"{hwmon_path}/pwm1_enable", "w") as f:
            f.write("1")
        with open(f"{hwmon_path}/pwm1", "w") as f:
            f.write(str(value))
    except OSError as e:
        log.debug("Failed to set fan PWM: %s", e)


def compute_fan_pwm(temp: float) -> int:
    """Linear interpolation: off below TEMP_OFF, ramp MIN→MAX between LOW→HIGH, max above HIGH."""
    if temp < FAN_TEMP_OFF:
        return 0
    if temp < FAN_TEMP_LOW:
        return FAN_PWM_MIN
    if temp >= FAN_TEMP_HIGH:
        return FAN_PWM_MAX
    # Linear ramp between LOW and HIGH
    ratio = (temp - FAN_TEMP_LOW) / (FAN_TEMP_HIGH - FAN_TEMP_LOW)
    return int(FAN_PWM_MIN + ratio * (FAN_PWM_MAX - FAN_PWM_MIN))


# --- System metrics collection ---

def get_serial() -> str:
    """Read RPi serial number."""
    try:
        with open("/sys/firmware/devicetree/base/serial-number", "r") as f:
            return f.read().strip().strip("\x00")
    except FileNotFoundError:
        return f"dev-{socket.gethostname()}"


def get_cpu_temp() -> float:
    try:
        with open("/sys/class/thermal/thermal_zone0/temp", "r") as f:
            return int(f.read().strip()) / 1000.0
    except (FileNotFoundError, ValueError):
        return 0.0


def get_memory() -> tuple[int, int]:
    info = {}
    try:
        with open("/proc/meminfo", "r") as f:
            for line in f:
                parts = line.split()
                if len(parts) >= 2:
                    info[parts[0].rstrip(":")] = int(parts[1])
    except FileNotFoundError:
        return 0, 0
    total = info.get("MemTotal", 0) // 1024
    available = info.get("MemAvailable", 0) // 1024
    return total - available, total


def get_disk_usage() -> float:
    try:
        st = os.statvfs("/")
        used = (st.f_blocks - st.f_bfree) * st.f_frsize
        total = st.f_blocks * st.f_frsize
        return round(used / total * 100, 1) if total else 0.0
    except OSError:
        return 0.0


def get_uptime() -> int:
    try:
        with open("/proc/uptime", "r") as f:
            return int(float(f.read().split()[0]))
    except (FileNotFoundError, ValueError):
        return 0


def get_wifi_info() -> tuple[str, int]:
    """Get WiFi SSID and signal using iw (no NetworkManager dependency)."""
    try:
        out = subprocess.check_output(
            ["iw", "dev", "wlan0", "link"],
            timeout=5, stderr=subprocess.DEVNULL,
        ).decode()
        ssid = ""
        signal = 0
        for line in out.strip().split("\n"):
            line = line.strip()
            if line.startswith("SSID:"):
                ssid = line.split(":", 1)[1].strip()
            elif line.startswith("signal:"):
                # "signal: -52 dBm"
                signal = int(line.split(":")[1].strip().split()[0])
        return ssid, signal
    except (subprocess.SubprocessError, FileNotFoundError, ValueError):
        return "", 0


def get_app_info() -> tuple[bool, int]:
    for name in ["goball", "SquareLine_Project"]:
        try:
            out = subprocess.check_output(
                ["pgrep", "-f", name], timeout=5, stderr=subprocess.DEVNULL
            ).decode().strip()
            if out:
                # Filter out our own agent process
                for line in out.split("\n"):
                    pid = int(line)
                    try:
                        cmdline = open(f"/proc/{pid}/cmdline").read()
                        if "goball_agent" not in cmdline:
                            return True, pid
                    except (OSError, ValueError):
                        continue
        except (subprocess.SubprocessError, ValueError):
            pass
    return False, 0


def get_public_ip() -> str:
    """Get public IP via external service (cached for 5 minutes)."""
    try:
        import urllib.request
        resp = urllib.request.urlopen("https://api.ipify.org", timeout=5)
        return resp.read().decode().strip()
    except Exception:
        return ""


_public_ip_cache = {"ip": "", "ts": 0}


def get_ip() -> str:
    now = time.time()
    # Refresh public IP every 5 minutes
    if now - _public_ip_cache["ts"] > 300:
        pub = get_public_ip()
        if pub:
            _public_ip_cache["ip"] = pub
            _public_ip_cache["ts"] = now
    if _public_ip_cache["ip"]:
        return _public_ip_cache["ip"]
    # Fallback to local IP
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except OSError:
        return ""


def collect_system_metrics(serial: str) -> dict:
    mem_used, mem_total = get_memory()
    wifi_ssid, wifi_signal = get_wifi_info()
    app_running, app_pid = get_app_info()
    firmware_version = ""
    try:
        result = subprocess.run(
            ["strings", "/usr/bin/goball"],
            capture_output=True, text=True, timeout=5
        )
        for line in result.stdout.splitlines():
            # GOBALL_VERSION is compiled as a bare "X.Y.Z" string
            if line and all(c.isdigit() or c == '.' for c in line) and line.count('.') == 2:
                parts = line.split('.')
                if all(p.isdigit() and len(p) <= 3 for p in parts):
                    firmware_version = line
                    break
    except Exception:
        pass
    fan_hw = find_fan_hwmon()
    fan_pwm = get_fan_pwm(fan_hw)
    fan_rpm = get_fan_rpm(fan_hw)
    hostname = VENUE_NAME or socket.gethostname()
    if DEVICE_LABEL:
        hostname = f"{hostname} - {DEVICE_LABEL}"
    return {
        "ts": time.time(),
        "hostname": hostname,
        "ip": get_ip(),
        "cpu_temp": get_cpu_temp(),
        "mem_used_mb": mem_used,
        "mem_total_mb": mem_total,
        "disk_used_pct": get_disk_usage(),
        "uptime_s": get_uptime(),
        "wifi_ssid": wifi_ssid,
        "wifi_signal_dbm": wifi_signal,
        "app_running": app_running,
        "app_pid": app_pid,
        "lat": LATITUDE,
        "lng": LONGITUDE,
        "firmware_version": firmware_version,
        "fan_pwm": fan_pwm,
        "fan_rpm": fan_rpm,
    }


# --- Log parsing ---
# Matches the GoBall C debug output format:
#   [HH:MM:SS] LEVEL [MODULE] [file:line] message
# ANSI color codes are stripped before matching.

ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;]*m")

# Game mode selection:  "Game mode set to: 0 (Stroke Play)"
RE_GAME_MODE = re.compile(r"\[UI\].*Game mode set to: \d+ \(([^)]+)\)")

# Player count:  "Number of players set to: 2"
RE_PLAYER_COUNT = re.compile(r"\[UI\].*Number of players set to: (\d+)")

# Score update:  "Player 1 new score: 5"
RE_SCORE = re.compile(r"\[GAME\].*Player (\d+) (?:new score|current score): (\d+)")

# Hole event:  "3-POINT HOLE" / "4-POINT HOLE" / "5-POINT HOLE"
RE_HOLE_EVENT = re.compile(r"\[GAME\].*(\d)-POINT HOLE")

# Sensor trigger:  "Debounce timer expired for sensor X"
RE_SENSOR = re.compile(r"\[GPIO\].*(?:timer expired|event) for sensor (\d+)")

# Any ERROR level line:  "[HH:MM:SS] ERROR [MODULE] ..."
RE_ERROR = re.compile(r"ERROR\s+\[(\w+)\]\s+\[[^\]]+\]\s+(.+)")

# Any WARN level line:  "[HH:MM:SS] WARN [MODULE] ..."
RE_WARN = re.compile(r"WARN\s+\[(\w+)\]\s+\[[^\]]+\]\s+(.+)")

# WiFi events
RE_WIFI = re.compile(r"\[WIFI\]\s+(.+)")

# Game init:  "Game initialized with N players"
RE_GAME_INIT = re.compile(r"\[LOGIC\].*Game initialized with (\d+) players")

# Game winner:  "Player 1 is the WINNER with a score of 5!"
RE_WINNER = re.compile(r"\[GAME\].*Player (\d+) is the WINNER with a score of (\d+)")

# Game tie:  "It's a tie!"
RE_TIE = re.compile(r"\[GAME\].*It's a tie")

# Hole completion:  "All players completed hole 9. Advancing to next round"
RE_HOLE_COMPLETE = re.compile(r"\[GAME\].*All players completed hole (\d+)")

GAME_MODE_MAP = {
    "Stroke Play": "playing",
    "Match Play": "playing",
    "Quota": "playing",
    "Vegas": "playing",
}


class LogParser:
    """Tail journalctl for goball.service and extract game events."""

    def __init__(self, client: mqtt.Client, serial: str):
        self.client = client
        self.serial = serial
        self.prefix = f"goball/{serial}"
        self.proc = None
        # Track state to avoid duplicate publishes
        self.current_mode = "idle"
        self.player_count = 0
        self.scores = {}

    def start(self):
        try:
            self.proc = subprocess.Popen(
                ["journalctl", "-u", "goball.service", "-f", "-n", "0", "--no-pager", "-o", "cat"],
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                text=True,
            )
            log.info("Started journalctl log parser")
        except FileNotFoundError:
            log.warning("journalctl not available, log parsing disabled")

    def poll(self):
        if not self.proc or not self.proc.stdout:
            return
        import select
        while select.select([self.proc.stdout], [], [], 0)[0]:
            line = self.proc.stdout.readline()
            if not line:
                break
            # Strip ANSI color codes
            clean = ANSI_ESCAPE.sub("", line.strip())
            self._parse_line(clean)

    def stop(self):
        if self.proc:
            self.proc.terminate()
            self.proc.wait(timeout=5)

    def _publish_game_state(self):
        self.client.publish(
            f"{self.prefix}/game/state",
            json.dumps({
                "mode": self.current_mode,
                "player_count": self.player_count,
                "scores": [self.scores.get(i, 0) for i in range(1, self.player_count + 1)],
                "hole": 0,
            }),
            qos=1,
        )

    def _parse_line(self, line: str):
        # Game mode selection
        m = RE_GAME_MODE.search(line)
        if m:
            mode_name = m.group(1)
            self.current_mode = GAME_MODE_MAP.get(mode_name, "playing")
            self.scores = {}
            self._publish_game_state()
            return

        # Player count
        m = RE_PLAYER_COUNT.search(line)
        if m:
            self.player_count = int(m.group(1))
            self._publish_game_state()
            return

        # Game init (also sets player count)
        m = RE_GAME_INIT.search(line)
        if m:
            self.player_count = int(m.group(1))
            self.current_mode = "setup"
            self.scores = {}
            self._publish_game_state()
            return

        # Score update
        m = RE_SCORE.search(line)
        if m:
            player = int(m.group(1))
            score = int(m.group(2))
            self.scores[player] = score
            self.client.publish(
                f"{self.prefix}/game/event",
                json.dumps({"event": "score", "player": player, "value": score, "ts": time.time()}),
            )
            self._publish_game_state()
            return

        # Hole scoring event
        m = RE_HOLE_EVENT.search(line)
        if m:
            points = int(m.group(1))
            self.client.publish(
                f"{self.prefix}/game/event",
                json.dumps({"event": "hole_score", "points": points, "ts": time.time()}),
            )
            return

        # Game winner
        m = RE_WINNER.search(line)
        if m:
            player = int(m.group(1))
            score = int(m.group(2))
            self.current_mode = "finished"
            self.client.publish(
                f"{self.prefix}/game/event",
                json.dumps({"event": "game_over", "winner": player, "score": score, "ts": time.time()}),
                qos=1,
            )
            self._publish_game_state()
            return

        # Game tie
        m = RE_TIE.search(line)
        if m:
            self.current_mode = "finished"
            self.client.publish(
                f"{self.prefix}/game/event",
                json.dumps({"event": "game_over", "winner": 0, "tie": True, "ts": time.time()}),
                qos=1,
            )
            self._publish_game_state()
            return

        # Hole completion
        m = RE_HOLE_COMPLETE.search(line)
        if m:
            hole = int(m.group(1))
            self.client.publish(
                f"{self.prefix}/game/event",
                json.dumps({"event": "hole_complete", "hole": hole, "ts": time.time()}),
            )
            return

        # Sensor trigger
        m = RE_SENSOR.search(line)
        if m:
            sensor = int(m.group(1))
            self.client.publish(
                f"{self.prefix}/game/event",
                json.dumps({"event": "sensor_trigger", "sensor": sensor, "ts": time.time()}),
            )
            return

        # Errors
        m = RE_ERROR.search(line)
        if m:
            module = m.group(1)
            message = m.group(2).strip()
            self.client.publish(
                f"{self.prefix}/errors",
                json.dumps({"module": module, "message": message, "ts": time.time()}),
                qos=1,
            )
            return

        # Warnings (publish as hardware state issues)
        m = RE_WARN.search(line)
        if m:
            module = m.group(1)
            message = m.group(2).strip()
            # Hardware-related warnings
            if module in ("LED", "GPIO", "SOUND"):
                self.client.publish(
                    f"{self.prefix}/hardware",
                    json.dumps({
                        "led_active": module != "LED",
                        "audio_active": module != "SOUND",
                        "sensor_ok": module != "GPIO",
                    }),
                    qos=1,
                )


# --- Remote commands ---

import threading

_SUDO = [] if os.geteuid() == 0 else ["sudo"]

ALLOWED_COMMANDS = {
    "restart_app": [*_SUDO, "systemctl", "restart", "goball.service"],
    "restart_agent": [*_SUDO, "systemctl", "restart", "goball-agent.service"],
    "reboot": [*_SUDO, "reboot"],
}


def handle_set_fan(client, prefix, payload):
    """Handle remote fan config update."""
    cmd_id = payload.get("id", "")
    result = {"id": cmd_id, "action": "set_fan", "ts": time.time()}
    config = payload.get("config", {})
    if not config:
        result["status"] = "error"
        result["message"] = "No config provided"
        client.publish(f"{prefix}/command/result", json.dumps(result), qos=1)
        return
    global FAN_ENABLED, FAN_TEMP_OFF, FAN_TEMP_LOW, FAN_TEMP_HIGH, FAN_PWM_MIN, FAN_PWM_MAX
    fan_keys = {
        "FAN_ENABLED": lambda v: v == "1" or v is True,
        "FAN_TEMP_OFF": float, "FAN_TEMP_LOW": float, "FAN_TEMP_HIGH": float,
        "FAN_PWM_MIN": int, "FAN_PWM_MAX": int,
    }
    for key, conv in fan_keys.items():
        if key in config:
            val = conv(config[key])
            CFG[key] = str(config[key])
            if key == "FAN_ENABLED": FAN_ENABLED = val
            elif key == "FAN_TEMP_OFF": FAN_TEMP_OFF = val
            elif key == "FAN_TEMP_LOW": FAN_TEMP_LOW = val
            elif key == "FAN_TEMP_HIGH": FAN_TEMP_HIGH = val
            elif key == "FAN_PWM_MIN": FAN_PWM_MIN = val
            elif key == "FAN_PWM_MAX": FAN_PWM_MAX = val
    result["status"] = "ok"
    result["message"] = f"Fan config updated: {config}"
    client.publish(f"{prefix}/command/result", json.dumps(result), qos=1)
    log.info("Fan config updated via MQTT: %s", config)


def handle_command(client, prefix, payload):
    """Execute a remote command and publish the result."""
    cmd_id = payload.get("id", "")
    action = payload.get("action", "")
    result = {"id": cmd_id, "action": action, "ts": time.time()}

    if action == "set_fan":
        handle_set_fan(client, prefix, payload)
        return

    if action not in ALLOWED_COMMANDS:
        result["status"] = "error"
        result["message"] = f"Unknown action: {action}"
        client.publish(f"{prefix}/command/result", json.dumps(result), qos=1)
        return

    try:
        # For restart_agent, publish result first since we'll be killed
        if action == "restart_agent":
            result["status"] = "ok"
            result["message"] = "Agent restarting..."
            client.publish(f"{prefix}/command/result", json.dumps(result), qos=1)
            time.sleep(0.5)

        cmd = ALLOWED_COMMANDS[action]
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        result["status"] = "ok" if proc.returncode == 0 else "error"
        result["message"] = (proc.stdout or proc.stderr or "Done").strip()[-500:]
    except subprocess.TimeoutExpired:
        result["status"] = "error"
        result["message"] = "Command timed out"
    except Exception as e:
        result["status"] = "error"
        result["message"] = str(e)

    client.publish(f"{prefix}/command/result", json.dumps(result), qos=1)


# --- Main ---

def main():
    global _fan_no_hw_warned, BROKER_PORT
    serial = get_serial()
    prefix = f"goball/{serial}"
    log.info("GoBall agent starting, serial=%s, broker=%s:%d", serial, BROKER_HOST, BROKER_PORT)

    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id=f"goball-{serial}",
    )
    # mTLS: if client cert is provided, use cert auth (no password needed)
    tls_client_cert = TLS_CERT if TLS_CERT and os.path.isfile(TLS_CERT) else None
    tls_client_key = TLS_KEY if TLS_KEY and os.path.isfile(TLS_KEY) else None

    if TLS_CA and os.path.isfile(TLS_CA):
        import ssl
        client.tls_set(
            ca_certs=TLS_CA,
            certfile=tls_client_cert,
            keyfile=tls_client_key,
            tls_version=ssl.PROTOCOL_TLSv1_2,
        )
        if tls_client_cert:
            log.info("mTLS enabled (CA: %s, cert: %s)", TLS_CA, TLS_CERT)
            # Auto-switch to 8883 if port was left at default 1883
            if BROKER_PORT == 1883:
                BROKER_PORT = 8883
                log.info("Auto-switched to port 8883 for mTLS")
        else:
            log.info("TLS enabled with CA: %s (no client cert)", TLS_CA)
            # Use password auth when no client cert
            if MQTT_USER:
                client.username_pw_set(MQTT_USER, MQTT_PASS)
    elif MQTT_USER:
        client.username_pw_set(MQTT_USER, MQTT_PASS)

    # Last Will and Testament
    client.will_set(f"{prefix}/status", "offline", qos=1, retain=True)

    def on_connect(c, userdata, flags, rc, properties=None):
        log.info("Connected to MQTT broker (rc=%s)", rc)
        c.publish(f"{prefix}/status", "online", qos=1, retain=True)
        c.subscribe(f"{prefix}/command", qos=1)
        log.info("Subscribed to %s/command", prefix)

    def on_disconnect(c, userdata, flags, rc, properties=None):
        log.warning("Disconnected from broker (rc=%s), will reconnect", rc)

    def on_message(c, userdata, msg):
        if msg.topic == f"{prefix}/command":
            try:
                payload = json.loads(msg.payload.decode())
                log.info("Received command: %s", payload.get("action"))
                threading.Thread(
                    target=handle_command, args=(client, prefix, payload), daemon=True
                ).start()
            except (json.JSONDecodeError, UnicodeDecodeError):
                log.warning("Bad command payload")

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    client.reconnect_delay_set(min_delay=1, max_delay=60)
    client.connect_async(BROKER_HOST, BROKER_PORT)
    client.loop_start()

    log_parser = LogParser(client, serial)
    log_parser.start()

    running = True

    def stop(sig, frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)

    try:
        while running:
            metrics = collect_system_metrics(serial)
            client.publish(f"{prefix}/system", json.dumps(metrics))

            # Fan control
            if FAN_ENABLED:
                fan_hw = find_fan_hwmon()
                if fan_hw:
                    target_pwm = compute_fan_pwm(metrics["cpu_temp"])
                    set_fan_pwm(fan_hw, target_pwm)
                elif not _fan_no_hw_warned:
                    log.warning("Fan control enabled but no fan hardware found (pwmfan hwmon)")
                    _fan_no_hw_warned = True

            log_parser.poll()
            time.sleep(PUBLISH_INTERVAL)
    finally:
        client.publish(f"{prefix}/status", "offline", qos=1, retain=True)
        time.sleep(0.5)
        log_parser.stop()
        client.loop_stop()
        client.disconnect()
        log.info("Agent stopped")


if __name__ == "__main__":
    main()
