#!/bin/bash
# GoBall Fleet Agent — Pi Setup Wizard
# Interactive installer: configures connection, venue info, SSH keys,
# installs the agent, and starts the service.
# Usage: sudo ./install.sh
set -e

cd "$(dirname "$0")"

echo ""
echo "  ╔══════════════════════════════════════════╗"
echo "  ║     GoBall Fleet Agent — Pi Setup        ║"
echo "  ╚══════════════════════════════════════════╝"
echo ""

# Check root
if [ "$(id -u)" -ne 0 ]; then
    echo "  ERROR: This script must be run as root."
    echo "  Run: sudo ./install.sh"
    exit 1
fi

INSTALL_DIR="/opt/goball-agent"
CONF_FILE="agent.conf"
CONF_DEST="/etc/goball-agent.conf"

# --- Helper: update a key in agent.conf ---
update_conf() {
    local key="$1" val="$2"
    if grep -q "^${key}=" "$CONF_FILE" 2>/dev/null; then
        sed -i "s|^${key}=.*|${key}=${val}|" "$CONF_FILE"
    else
        echo "${key}=${val}" >> "$CONF_FILE"
    fi
}

# --- Helper: read value from agent.conf ---
get_conf() {
    grep "^${1}=" "$CONF_FILE" 2>/dev/null | cut -d= -f2
}

# Get device serial
SERIAL=$(cat /sys/firmware/devicetree/base/serial-number 2>/dev/null | tr -d '\0' || hostname)
echo "  Device serial: ${SERIAL}"
echo ""

# ============================================================
# STEP 1: Install dependencies
# ============================================================
echo "Step 1/5 — Installing dependencies"
echo "───────────────────────────────────"
echo ""

# Check Python
if command -v python3 &>/dev/null; then
    PY_VER=$(python3 --version 2>&1)
    echo "  [OK] ${PY_VER}"
else
    echo "  [X] Python 3 not found. Install it first."
    exit 1
fi

# Install paho-mqtt
echo "  Installing paho-mqtt..."
if python3 -c "import paho.mqtt" 2>/dev/null; then
    echo "  [OK] paho-mqtt already installed"
else
    # Try multiple install methods (BusyBox vs full distro)
    if pip3 install --break-system-packages paho-mqtt==2.1.0 2>/dev/null; then
        echo "  [OK] paho-mqtt installed via pip3"
    elif python3 -m pip install paho-mqtt==2.1.0 2>/dev/null; then
        echo "  [OK] paho-mqtt installed via python3 -m pip"
    elif python3 -m ensurepip 2>/dev/null && python3 -m pip install paho-mqtt==2.1.0 2>/dev/null; then
        echo "  [OK] paho-mqtt installed (bootstrapped pip first)"
    else
        echo "  [X] Failed to install paho-mqtt. Install manually:"
        echo "      pip3 install paho-mqtt==2.1.0"
        exit 1
    fi
fi

echo ""

# ============================================================
# STEP 2: Configure connection
# ============================================================
echo "Step 2/5 — Server connection"
echo "────────────────────────────"
echo ""

# Create default config if missing
if [ ! -f "$CONF_FILE" ]; then
    cat > "$CONF_FILE" << 'CONFEOF'
MQTT_BROKER_HOST=fleet.example.com
MQTT_BROKER_PORT=1883
MQTT_USERNAME=goball
MQTT_PASSWORD=changeme
VENUE_NAME=My Venue
DEVICE_LABEL=Hole 1
LATITUDE=0
LONGITUDE=0
PUBLISH_INTERVAL=30
CONFEOF
fi

echo "  Configure each setting below."
echo "  Press Enter to keep the value shown in [brackets]."
echo ""

current=$(get_conf MQTT_BROKER_HOST)
read -p "  Server IP/hostname [${current}]: " input
[ -n "$input" ] && update_conf "MQTT_BROKER_HOST" "$input"

current=$(get_conf MQTT_BROKER_PORT)
read -p "  MQTT port [${current}]: " input
[ -n "$input" ] && update_conf "MQTT_BROKER_PORT" "$input"

current=$(get_conf MQTT_PASSWORD)
if [ "$current" = "changeme" ]; then
    read -p "  Device password (from server setup): " input
    [ -n "$input" ] && update_conf "MQTT_PASSWORD" "$input"
else
    read -p "  Device password [keep current]: " input
    [ -n "$input" ] && update_conf "MQTT_PASSWORD" "$input"
fi

echo ""

# ============================================================
# STEP 3: Venue info
# ============================================================
echo "Step 3/5 — Venue information"
echo "────────────────────────────"
echo ""

current=$(get_conf VENUE_NAME)
read -p "  Venue name [${current}]: " input
[ -n "$input" ] && update_conf "VENUE_NAME" "$input"

current=$(get_conf DEVICE_LABEL)
read -p "  Device label (e.g. Hole 1) [${current}]: " input
[ -n "$input" ] && update_conf "DEVICE_LABEL" "$input"

current=$(get_conf LATITUDE)
read -p "  Latitude [${current}]: " input
[ -n "$input" ] && update_conf "LATITUDE" "$input"

current=$(get_conf LONGITUDE)
read -p "  Longitude [${current}]: " input
[ -n "$input" ] && update_conf "LONGITUDE" "$input"

echo ""

# ============================================================
# STEP 4: SSH key authorization (for web terminal)
# ============================================================
echo "Step 4/5 — SSH key authorization (web terminal)"
echo "─────────────────────────────────────────────────"
echo ""

SSH_USER="root"
SSH_DIR="/${SSH_USER}/.ssh"
AUTH_KEYS="${SSH_DIR}/authorized_keys"

# Check if server_key.pub was bundled by setup.sh
if [ -f "server_key.pub" ]; then
    SERVER_PUBKEY=$(cat server_key.pub)
    echo "  Found server public key (bundled by setup.sh)"
    echo "  Key: ${SERVER_PUBKEY:0:50}..."
    echo ""

    # Check if already authorized
    if [ -f "$AUTH_KEYS" ] && grep -qF "$SERVER_PUBKEY" "$AUTH_KEYS" 2>/dev/null; then
        echo "  [OK] Server key already authorized"
    else
        read -p "  Authorize this key for web terminal? [Y/n] " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Nn]$ ]]; then
            mkdir -p "$SSH_DIR"
            echo "$SERVER_PUBKEY" >> "$AUTH_KEYS"
            chmod 700 "$SSH_DIR"
            chmod 600 "$AUTH_KEYS"
            echo "  [OK] Server key authorized for ${SSH_USER}"
        else
            echo "  [!] Skipped. Web terminal won't work until key is authorized."
        fi
    fi
else
    echo "  [!] No server_key.pub found."
    echo "      To enable web terminal, paste the server's public key below."
    echo "      (Find it on the server in fleet/keys/id_ed25519.pub)"
    echo "      Press Enter to skip."
    echo ""
    read -p "  Public key: " input
    if [ -n "$input" ]; then
        mkdir -p "$SSH_DIR"
        echo "$input" >> "$AUTH_KEYS"
        chmod 700 "$SSH_DIR"
        chmod 600 "$AUTH_KEYS"
        echo "  [OK] Key authorized for ${SSH_USER}"
    else
        echo "  [!] Skipped. Web terminal won't work until key is authorized."
    fi
fi

# Enable SSH root login if sshd_config exists
if [ -f /etc/ssh/sshd_config ]; then
    if grep -q "^PermitRootLogin" /etc/ssh/sshd_config; then
        sed -i 's/^PermitRootLogin.*/PermitRootLogin prohibit-password/' /etc/ssh/sshd_config
    elif grep -q "^#PermitRootLogin" /etc/ssh/sshd_config; then
        sed -i 's/^#PermitRootLogin.*/PermitRootLogin prohibit-password/' /etc/ssh/sshd_config
    else
        echo "PermitRootLogin prohibit-password" >> /etc/ssh/sshd_config
    fi
    # Restart SSH if systemctl is available
    systemctl restart sshd 2>/dev/null || systemctl restart ssh 2>/dev/null || true
fi

echo ""

# ============================================================
# STEP 5: Install and start agent
# ============================================================
echo "Step 5/5 — Installing agent"
echo "───────────────────────────"
echo ""

# Copy agent
echo "  Installing agent to ${INSTALL_DIR}..."
mkdir -p "$INSTALL_DIR"
cp goball_agent.py "$INSTALL_DIR/"
chmod +x "$INSTALL_DIR/goball_agent.py"

# Copy config
echo "  Installing configuration to ${CONF_DEST}..."
cp "$CONF_FILE" "$CONF_DEST"
chmod 600 "$CONF_DEST"

# Install systemd service
echo "  Installing systemd service..."
cp goball-agent.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable goball-agent.service
systemctl restart goball-agent.service

# Wait for connection
echo ""
echo "  Waiting for MQTT connection..."
sleep 3

# Check if connected
if journalctl -u goball-agent -n 10 --no-pager 2>/dev/null | grep -q "Connected to MQTT"; then
    echo "  [OK] Connected to broker!"
else
    echo "  [!] Not connected yet. Check logs: journalctl -u goball-agent -f"
fi

echo ""
echo "  ╔══════════════════════════════════════════╗"
echo "  ║         Pi Setup Complete!               ║"
echo "  ╚══════════════════════════════════════════╝"
echo ""
echo "  Serial:       ${SERIAL}"
echo "  Venue:        $(get_conf VENUE_NAME) — $(get_conf DEVICE_LABEL)"
echo "  Server:       $(get_conf MQTT_BROKER_HOST):$(get_conf MQTT_BROKER_PORT)"
echo "  Config:       ${CONF_DEST}"
echo "  Service:      goball-agent.service"
echo ""
echo "  ── Commands ──"
echo "  Status:   systemctl status goball-agent"
echo "  Logs:     journalctl -u goball-agent -f"
echo "  Restart:  systemctl restart goball-agent"
echo "  Config:   nano ${CONF_DEST}"
echo ""
echo "  This device should now appear on the dashboard."
