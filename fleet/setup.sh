#!/bin/bash
# GoBall Fleet Monitor — Server Setup Wizard
# Step-by-step: checks prerequisites, configures server, generates passwords,
# sets up SSH keys, and starts the stack.
set -e

cd "$(dirname "$0")"

echo ""
echo "  ╔══════════════════════════════════════════╗"
echo "  ║     GoBall Fleet Monitor — Setup         ║"
echo "  ╚══════════════════════════════════════════╝"
echo ""

# --- Helper: update a key in server.conf ---
update_conf() {
    local key="$1" val="$2"
    if grep -q "^${key}=" server.conf 2>/dev/null; then
        sed -i "s|^${key}=.*|${key}=${val}|" server.conf
    else
        echo "${key}=${val}" >> server.conf
    fi
}

# --- Helper: read value from server.conf ---
get_conf() {
    grep "^${1}=" server.conf 2>/dev/null | cut -d= -f2
}

# --- Helper: generate random password ---
rand_pass() {
    openssl rand -base64 12 2>/dev/null | tr -d '/+=' | head -c 12 || head -c 12 /dev/urandom | base64 | tr -d '/+=' | head -c 12
}

# ============================================================
# STEP 1: Check prerequisites
# ============================================================
echo "Step 1/5 — Checking prerequisites"
echo "─────────────────────────────────"
echo ""

# Docker
if command -v docker &>/dev/null; then
    DOCKER_VER=$(docker --version 2>/dev/null | grep -oP '\d+\.\d+\.\d+' | head -1)
    echo "  [OK] Docker v${DOCKER_VER}"
else
    echo "  [!] Docker not found."
    read -p "  Install Docker now? [Y/n] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Nn]$ ]]; then
        echo "  Docker is required. Install it manually and re-run this script."
        exit 1
    fi
    if [ "$(id -u)" -ne 0 ]; then
        echo "  ERROR: Docker installation requires root. Run: sudo ./setup.sh"
        exit 1
    fi
    echo "  Installing Docker..."
    curl -fsSL https://get.docker.com | sh
    systemctl enable --now docker
    echo "  [OK] Docker installed"
fi

# Docker Compose
if docker compose version &>/dev/null; then
    COMPOSE_VER=$(docker compose version --short 2>/dev/null)
    echo "  [OK] Docker Compose v${COMPOSE_VER}"
else
    echo "  [X] Docker Compose v2 plugin not found."
    echo "      Install it with: sudo apt install docker-compose-plugin"
    exit 1
fi

# Docker running
if ! docker info &>/dev/null; then
    echo "  [X] Docker daemon is not running."
    echo "      Start it with: sudo systemctl start docker"
    exit 1
fi

# Docker permissions
if ! docker ps &>/dev/null; then
    echo "  [!] Your user cannot access Docker."
    if [ "$(id -u)" -ne 0 ]; then
        echo "      Run: sudo usermod -aG docker \$USER && newgrp docker"
        exit 1
    fi
    usermod -aG docker "$SUDO_USER" 2>/dev/null || true
    echo "  [OK] Added to docker group. You may need to log out and back in."
fi

echo ""
echo "  All prerequisites met."
echo ""

# ============================================================
# STEP 2: Configure server
# ============================================================
echo "Step 2/5 — Server configuration"
echo "────────────────────────────────"
echo ""

# Create server.conf if missing
if [ ! -f server.conf ]; then
    echo "  Creating server.conf..."
    cat > server.conf << 'CONFEOF'
# GoBall Fleet Monitor — Server Configuration
MQTT_BROKER_PORT=1883
MQTT_USERNAME=backend
MQTT_PASSWORD=changeme
DASHBOARD_PORT=8000
SQLITE_DB_PATH=/data/fleet.db
ALERT_OFFLINE_SECONDS=300
ALERT_CPU_TEMP_C=80
ALERT_DISK_USED_PCT=90
EVENT_RETENTION_DAYS=7
DEVICE_USERNAME=goball
DEVICE_PASSWORD=changeme
SSH_KEY_PATH=~/.ssh/id_ed25519
SSH_USERNAME=root
CONFEOF
    echo ""
fi

echo "  Configure each setting below."
echo "  Press Enter to keep the value shown in [brackets]."
echo "  Values marked [auto] will be randomly generated."
echo ""

# --- Connection ---
echo "  ── Connection ──"
echo ""

current=$(get_conf DASHBOARD_PORT)
read -p "  Dashboard port [${current}]: " input
[ -n "$input" ] && update_conf "DASHBOARD_PORT" "$input"

current=$(get_conf MQTT_BROKER_PORT)
read -p "  MQTT broker port [${current}]: " input
[ -n "$input" ] && update_conf "MQTT_BROKER_PORT" "$input"

echo ""

# --- Passwords ---
echo "  ── Passwords ──"
echo ""

current=$(get_conf MQTT_PASSWORD)
if [ "$current" = "changeme" ]; then
    suggested=$(rand_pass)
    read -p "  Backend password [auto: ${suggested}]: " input
    update_conf "MQTT_PASSWORD" "${input:-$suggested}"
else
    read -p "  Backend password [keep current]: " input
    [ -n "$input" ] && update_conf "MQTT_PASSWORD" "$input"
fi

current=$(get_conf DEVICE_PASSWORD)
if [ "$current" = "changeme" ]; then
    suggested=$(rand_pass)
    read -p "  Device password (all Pis use this) [auto: ${suggested}]: " input
    update_conf "DEVICE_PASSWORD" "${input:-$suggested}"
else
    read -p "  Device password (all Pis use this) [keep current]: " input
    [ -n "$input" ] && update_conf "DEVICE_PASSWORD" "$input"
fi

echo ""

# --- Alert thresholds ---
echo "  ── Alert Thresholds ──"
echo ""

current=$(get_conf ALERT_OFFLINE_SECONDS)
read -p "  Offline timeout seconds [${current}]: " input
[ -n "$input" ] && update_conf "ALERT_OFFLINE_SECONDS" "$input"

current=$(get_conf ALERT_CPU_TEMP_C)
read -p "  CPU temp warning °C [${current}]: " input
[ -n "$input" ] && update_conf "ALERT_CPU_TEMP_C" "$input"

current=$(get_conf ALERT_DISK_USED_PCT)
read -p "  Disk usage warning % [${current}]: " input
[ -n "$input" ] && update_conf "ALERT_DISK_USED_PCT" "$input"

current=$(get_conf EVENT_RETENTION_DAYS)
read -p "  Event retention days [${current}]: " input
[ -n "$input" ] && update_conf "EVENT_RETENTION_DAYS" "$input"

echo ""
echo "  [OK] server.conf saved"
echo ""

# ============================================================
# STEP 3: SSH key for web terminal
# ============================================================
echo "Step 3/5 — SSH key setup (for web terminal)"
echo "─────────────────────────────────────────────"
echo ""

mkdir -p keys

if [ -f keys/id_ed25519 ]; then
    echo "  [OK] SSH key already exists in keys/"
else
    # Check if user has an existing key
    if [ -f "$HOME/.ssh/id_ed25519" ]; then
        echo "  Found existing key at ~/.ssh/id_ed25519"
        read -p "  Use this key? [Y/n] " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Nn]$ ]]; then
            cp "$HOME/.ssh/id_ed25519" keys/id_ed25519
            chmod 600 keys/id_ed25519
            echo "  [OK] Key copied to keys/"
        fi
    fi

    # Generate new key if still missing
    if [ ! -f keys/id_ed25519 ]; then
        echo "  Generating new SSH key pair..."
        ssh-keygen -t ed25519 -f keys/id_ed25519 -N "" -q
        echo "  [OK] SSH key generated"
    fi
fi

# Show public key
if [ -f keys/id_ed25519.pub ]; then
    PUBKEY=$(cat keys/id_ed25519.pub)
elif [ -f keys/id_ed25519 ]; then
    PUBKEY=$(ssh-keygen -y -f keys/id_ed25519 2>/dev/null)
fi

echo ""
echo "  ┌─────────────────────────────────────────────────────────┐"
echo "  │  PUBLIC KEY (must be authorized on each Pi)             │"
echo "  └─────────────────────────────────────────────────────────┘"
echo ""
echo "  $PUBKEY"
echo ""
echo "  This key is automatically added when you run install.sh"
echo "  on each Pi. Or add it manually to /root/.ssh/authorized_keys"
echo ""

# Save public key to agent folder so install.sh can use it
cp keys/id_ed25519.pub agent/server_key.pub 2>/dev/null || \
    ssh-keygen -y -f keys/id_ed25519 > agent/server_key.pub 2>/dev/null || true

# Update SSH_USERNAME default
current=$(get_conf SSH_USERNAME)
read -p "  SSH username on Pi devices [${current}]: " input
[ -n "$input" ] && update_conf "SSH_USERNAME" "$input"

echo ""

# ============================================================
# STEP 4: Generate broker passwords
# ============================================================
echo "Step 4/5 — Generating broker credentials"
echo "─────────────────────────────────────────"
echo ""

BACKEND_PASS=$(get_conf MQTT_PASSWORD)
DEVICE_USER=$(get_conf DEVICE_USERNAME)
DEVICE_PASS=$(get_conf DEVICE_PASSWORD)

mkdir -p broker
echo "backend:${BACKEND_PASS}" > broker/passwd
echo "${DEVICE_USER}:${DEVICE_PASS}" >> broker/passwd
chmod 600 broker/passwd

docker run --rm -v "$(pwd)/broker/passwd:/passwd:z" eclipse-mosquitto:2 \
    sh -c "chmod 600 /passwd && mosquitto_passwd -U /passwd" 2>/dev/null

echo "  [OK] Broker password file created"
echo ""

# ============================================================
# STEP 5: Start the stack
# ============================================================
echo "Step 5/5 — Starting services"
echo "────────────────────────────"
echo ""

read -p "  Start GoBall Fleet Monitor now? [Y/n] " -n 1 -r
echo
if [[ $REPLY =~ ^[Nn]$ ]]; then
    echo ""
    echo "  To start later, run:"
    echo "    docker compose --env-file server.conf up -d --build"
    echo ""
    exit 0
fi

echo ""
echo "  Building and starting containers..."
docker compose --env-file server.conf up -d --build

DASHBOARD_PORT=$(get_conf DASHBOARD_PORT)
DEVICE_PASS=$(get_conf DEVICE_PASSWORD)
SERVER_IP=$(hostname -I 2>/dev/null | awk '{print $1}' || echo 'localhost')

echo ""
echo "  ╔══════════════════════════════════════════╗"
echo "  ║         Setup Complete!                  ║"
echo "  ╚══════════════════════════════════════════╝"
echo ""
echo "  Dashboard:      http://${SERVER_IP}:${DASHBOARD_PORT}"
echo "  MQTT Broker:    ${SERVER_IP}:$(get_conf MQTT_BROKER_PORT)"
echo ""
echo "  Device password: ${DEVICE_PASS}"
echo "  (Pi agents need this to connect)"
echo ""
echo "  ── Commands ──"
echo "  View logs:    docker compose --env-file server.conf logs -f"
echo "  Restart:      docker compose --env-file server.conf restart"
echo "  Stop:         docker compose --env-file server.conf down"
echo "  Reconfigure:  ./setup.sh"
echo ""
echo "  ── Next Steps ──"
echo "  1. Copy agent/ folder to each Raspberry Pi"
echo "  2. Run: sudo ./install.sh"
echo "  3. The wizard will configure everything"
echo ""
echo "  See notes.txt for full instructions."
