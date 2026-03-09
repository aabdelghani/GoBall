#!/bin/bash
# One-command installer for GoBall fleet agent on RPi
set -e

INSTALL_DIR="/opt/goball-agent"
BROKER_HOST="${1:-fleet.example.com}"
BROKER_PORT="${2:-1883}"

echo "Installing GoBall fleet agent..."

# Install paho-mqtt
pip3 install --break-system-packages paho-mqtt==2.1.0 2>/dev/null || pip3 install paho-mqtt==2.1.0

# Copy agent
mkdir -p "$INSTALL_DIR"
cp goball_agent.py "$INSTALL_DIR/"

# Install systemd service
cp goball-agent.service /etc/systemd/system/
sed -i "s|fleet.example.com|${BROKER_HOST}|g" /etc/systemd/system/goball-agent.service
sed -i "s|MQTT_BROKER_PORT=1883|MQTT_BROKER_PORT=${BROKER_PORT}|g" /etc/systemd/system/goball-agent.service

systemctl daemon-reload
systemctl enable goball-agent.service
systemctl start goball-agent.service

echo "Agent installed and running. Serial: $(cat /sys/firmware/devicetree/base/serial-number 2>/dev/null || hostname)"
echo "Check status: systemctl status goball-agent"
