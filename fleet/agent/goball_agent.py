#!/usr/bin/env python3
"""GoBall fleet monitoring agent — runs on each Raspberry Pi.

Publishes system metrics and parses goball.service logs via MQTT.
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

# Configuration (env vars or defaults)
BROKER_HOST = os.environ.get("MQTT_BROKER_HOST", "localhost")
BROKER_PORT = int(os.environ.get("MQTT_BROKER_PORT", "1883"))
MQTT_USER = os.environ.get("MQTT_USERNAME", "")
MQTT_PASS = os.environ.get("MQTT_PASSWORD", "")
PUBLISH_INTERVAL = int(os.environ.get("PUBLISH_INTERVAL", "30"))


def get_serial() -> str:
    """Read RPi serial number."""
    try:
        with open("/sys/firmware/devicetree/base/serial-number", "r") as f:
            return f.read().strip().strip("\x00")
    except FileNotFoundError:
        # Fallback for non-RPi systems
        return f"dev-{socket.gethostname()}"


def get_cpu_temp() -> float:
    try:
        with open("/sys/class/thermal/thermal_zone0/temp", "r") as f:
            return int(f.read().strip()) / 1000.0
    except (FileNotFoundError, ValueError):
        return 0.0


def get_memory() -> tuple[int, int]:
    """Return (used_mb, total_mb)."""
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
    """Return (ssid, signal_dbm)."""
    try:
        out = subprocess.check_output(
            ["nmcli", "-t", "-f", "ACTIVE,SSID,SIGNAL", "dev", "wifi"],
            timeout=5, stderr=subprocess.DEVNULL,
        ).decode()
        for line in out.strip().split("\n"):
            parts = line.split(":")
            if len(parts) >= 3 and parts[0] == "yes":
                return parts[1], -100 + int(parts[2])
    except (subprocess.SubprocessError, FileNotFoundError, ValueError):
        pass
    return "", 0


def get_app_info() -> tuple[bool, int]:
    """Check if SquareLine_Project is running."""
    try:
        out = subprocess.check_output(
            ["pgrep", "-f", "SquareLine_Project"], timeout=5, stderr=subprocess.DEVNULL
        ).decode().strip()
        if out:
            pid = int(out.split("\n")[0])
            return True, pid
    except (subprocess.SubprocessError, ValueError):
        pass
    return False, 0


def get_ip() -> str:
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
    return {
        "ts": time.time(),
        "hostname": socket.gethostname(),
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
    }


# --- Log parsing patterns ---
RE_GAME_MODE = re.compile(r"\[GAME\]\s+Mode changed to (\w+)")
RE_PLAYER_COUNT = re.compile(r"\[GAME\]\s+Players?: (\d+)")
RE_SCORE = re.compile(r"\[GAME\]\s+Player (\d+) scored? (\d+)")
RE_ERROR = re.compile(r"\[ERROR\]\s+(.+)")
RE_SENSOR = re.compile(r"\[SENSOR\]\s+(\w+) triggered")


class LogParser:
    """Tail journalctl for goball.service and extract game events."""

    def __init__(self, client: mqtt.Client, serial: str):
        self.client = client
        self.serial = serial
        self.prefix = f"goball/{serial}"
        self.proc = None

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
            self._parse_line(line.strip())

    def stop(self):
        if self.proc:
            self.proc.terminate()
            self.proc.wait(timeout=5)

    def _parse_line(self, line: str):
        m = RE_GAME_MODE.search(line)
        if m:
            self.client.publish(
                f"{self.prefix}/game/state",
                json.dumps({"mode": m.group(1).lower(), "player_count": 0, "scores": [], "hole": 0}),
                qos=1,
            )
            return

        m = RE_SCORE.search(line)
        if m:
            self.client.publish(
                f"{self.prefix}/game/event",
                json.dumps({"event": "score", "player": int(m.group(1)), "value": int(m.group(2)), "ts": time.time()}),
            )
            return

        m = RE_ERROR.search(line)
        if m:
            self.client.publish(
                f"{self.prefix}/errors",
                json.dumps({"message": m.group(1), "ts": time.time()}),
                qos=1,
            )


def main():
    serial = get_serial()
    prefix = f"goball/{serial}"
    log.info("GoBall agent starting, serial=%s", serial)

    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id=f"goball-{serial}",
    )
    if MQTT_USER:
        client.username_pw_set(MQTT_USER, MQTT_PASS)

    # Last Will and Testament
    client.will_set(f"{prefix}/status", "offline", qos=1, retain=True)

    def on_connect(c, userdata, flags, rc, properties=None):
        log.info("Connected to MQTT broker (rc=%s)", rc)
        c.publish(f"{prefix}/status", "online", qos=1, retain=True)

    def on_disconnect(c, userdata, flags, rc, properties=None):
        log.warning("Disconnected from broker (rc=%s), will reconnect", rc)

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
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
