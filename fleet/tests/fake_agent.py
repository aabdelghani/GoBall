#!/usr/bin/env python3
"""Simulate N GoBall devices publishing MQTT telemetry for testing."""

import argparse
import json
import random
import signal
import sys
import time

import paho.mqtt.client as mqtt

GAME_MODES = ["idle", "setup", "playing", "finished"]
WIFI_SSIDS = ["VenueLAN", "GolfClub-5G", "MinigolfNet", "PublicWiFi"]
HOSTNAMES = ["goball-venue-{}", "goball-club-{}", "goball-park-{}"]

# Worldwide mini golf venue locations
VENUES = [
    (40.7128, -74.0060),   # New York
    (51.5074, -0.1278),    # London
    (48.8566, 2.3522),     # Paris
    (35.6762, 139.6503),   # Tokyo
    (25.2048, 55.2708),    # Dubai
    (-33.8688, 151.2093),  # Sydney
    (37.7749, -122.4194),  # San Francisco
    (52.5200, 13.4050),    # Berlin
    (1.3521, 103.8198),    # Singapore
    (55.7558, 37.6173),    # Moscow
    (30.0444, 31.2357),    # Cairo
    (-23.5505, -46.6333),  # Sao Paulo
    (41.0082, 28.9784),    # Istanbul
    (19.4326, -99.1332),   # Mexico City
    (59.3293, 18.0686),    # Stockholm
    (45.4215, -75.6972),   # Ottawa
    (28.6139, 77.2090),    # New Delhi
    (-1.2921, 36.8219),    # Nairobi
    (31.2304, 121.4737),   # Shanghai
    (43.6532, -79.3832),   # Toronto
]


def make_serial(index: int) -> str:
    return f"gb{index:04d}"


def system_payload(serial: str, index: int) -> dict:
    venue = VENUES[index % len(VENUES)]
    # Add slight jitter so multiple devices at same city don't stack
    lat = venue[0] + random.uniform(-0.05, 0.05)
    lng = venue[1] + random.uniform(-0.05, 0.05)
    return {
        "ts": time.time(),
        "hostname": random.choice(HOSTNAMES).format(index),
        "ip": f"192.168.{(index // 254) % 256}.{(index % 254) + 1}",
        "cpu_temp": round(random.gauss(52, 8), 1),
        "mem_used_mb": random.randint(200, 600),
        "mem_total_mb": 4096,
        "disk_used_pct": round(random.uniform(20, 70), 1),
        "uptime_s": random.randint(3600, 604800),
        "wifi_ssid": random.choice(WIFI_SSIDS),
        "wifi_signal_dbm": random.randint(-70, -30),
        "app_running": random.random() > 0.05,
        "app_pid": random.randint(1000, 9999),
        "lat": round(lat, 4),
        "lng": round(lng, 4),
    }


def game_state_payload() -> dict:
    mode = random.choice(GAME_MODES)
    pc = random.randint(1, 4) if mode in ("playing", "finished") else 0
    return {
        "mode": mode,
        "player_count": pc,
        "scores": [random.randint(0, 36) for _ in range(pc)],
        "hole": random.randint(1, 18) if mode == "playing" else 0,
    }


def game_event_payload() -> dict:
    return {
        "event": random.choice(["score", "sensor_trigger", "ball_return"]),
        "player": random.randint(1, 4),
        "value": random.randint(1, 6),
        "ts": time.time(),
    }


def error_payload() -> dict:
    errors = [
        "Sensor 3 not responding",
        "LED strip communication timeout",
        "Audio device disconnected",
        "WiFi signal lost briefly",
        "App crash: segfault in render loop",
    ]
    return {"message": random.choice(errors), "ts": time.time()}


def main():
    parser = argparse.ArgumentParser(description="Fake GoBall fleet agent")
    parser.add_argument("-n", "--num-devices", type=int, default=5, help="Number of devices to simulate")
    parser.add_argument("-H", "--host", default="localhost", help="MQTT broker host")
    parser.add_argument("-p", "--port", type=int, default=1883, help="MQTT broker port")
    parser.add_argument("-u", "--username", default="", help="MQTT username")
    parser.add_argument("-P", "--password", default="", help="MQTT password")
    parser.add_argument("-i", "--interval", type=float, default=5.0, help="Publish interval (seconds)")
    args = parser.parse_args()

    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id=f"fake-agent-{random.randint(1000,9999)}",
    )
    if args.username:
        client.username_pw_set(args.username, args.password)

    serials = [make_serial(i) for i in range(args.num_devices)]

    # Set LWT for all devices
    for serial in serials:
        client.will_set(f"goball/{serial}/status", "offline", qos=1, retain=True)

    def on_connect(client, userdata, flags, rc, properties=None):
        print(f"Connected to broker (rc={rc}), simulating {len(serials)} devices")
        # Publish online status for all
        for serial in serials:
            client.publish(f"goball/{serial}/status", "online", qos=1, retain=True)

    client.on_connect = on_connect
    client.connect(args.host, args.port)
    client.loop_start()

    running = True

    def stop(sig, frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)

    cycle = 0
    try:
        while running:
            for i, serial in enumerate(serials):
                prefix = f"goball/{serial}"

                # System metrics every cycle
                client.publish(f"{prefix}/system", json.dumps(system_payload(serial, i)))

                # Game state changes occasionally
                if random.random() < 0.3:
                    client.publish(f"{prefix}/game/state", json.dumps(game_state_payload()), qos=1)

                # Game events occasionally
                if random.random() < 0.2:
                    client.publish(f"{prefix}/game/event", json.dumps(game_event_payload()))

                # Hardware state rarely
                if random.random() < 0.1:
                    client.publish(
                        f"{prefix}/hardware",
                        json.dumps({"led_active": True, "audio_active": random.random() > 0.3, "sensor_ok": random.random() > 0.1}),
                        qos=1,
                    )

                # Errors rarely
                if random.random() < 0.05:
                    client.publish(f"{prefix}/errors", json.dumps(error_payload()), qos=1)

            cycle += 1
            if cycle % 10 == 0:
                print(f"Cycle {cycle}: published metrics for {len(serials)} devices")

            time.sleep(args.interval)
    finally:
        # Publish offline for all
        for serial in serials:
            client.publish(f"goball/{serial}/status", "offline", qos=1, retain=True)
        client.loop_stop()
        client.disconnect()
        print("Fake agent stopped")


if __name__ == "__main__":
    main()
