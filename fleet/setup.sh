#!/bin/bash
# Initial setup: generate Mosquitto password file
set -e

cd "$(dirname "$0")"

MQTT_PASSWORD="${MQTT_PASSWORD:-changeme}"

echo "Generating Mosquitto password file..."
# Create temp password file, then hash it with mosquitto container
echo "backend:${MQTT_PASSWORD}" > broker/passwd
echo "goball:${MQTT_PASSWORD}" >> broker/passwd

# Use mosquitto container to hash the passwords
docker run --rm -v "$(pwd)/broker/passwd:/passwd" eclipse-mosquitto:2 \
    mosquitto_passwd -U /passwd

echo "Password file created at broker/passwd"
echo ""
echo "Start the stack with:  docker-compose up -d"
echo "Test with fake agent:  python3 tests/fake_agent.py -n 5 -u goball -P ${MQTT_PASSWORD}"
echo "Dashboard at:          http://localhost:8000"
