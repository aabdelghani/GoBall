#!/usr/bin/env bash
# =============================================================================
# GoBall Fleet — New Pi Setup Wizard
# =============================================================================
# Run on the fleet server: bash ~/fleet/setup-pi.sh
# Guides you through generating certs and configuring a new Pi device.
# =============================================================================

set -euo pipefail

FLEET_DIR="$HOME/fleet"
CERTS_DIR="$FLEET_DIR/certs"
AGENT_DIR="$FLEET_DIR/agent"
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

banner() {
    echo ""
    echo -e "${GREEN}╔══════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║${BOLD}     GoBall Fleet — New Pi Setup Wizard           ${NC}${GREEN}║${NC}"
    echo -e "${GREEN}╚══════════════════════════════════════════════════╝${NC}"
    echo ""
}

step() {
    echo ""
    echo -e "${CYAN}━━━ Step $1: $2 ━━━${NC}"
    echo ""
}

success() {
    echo -e "  ${GREEN}✓${NC} $1"
}

error() {
    echo -e "  ${RED}✗${NC} $1"
}

warn() {
    echo -e "  ${YELLOW}!${NC} $1"
}

prompt() {
    echo -ne "  ${BOLD}$1${NC}"
    read -r REPLY
}

# ── Preflight checks ──

banner

if [[ ! -f "$CERTS_DIR/ca.crt" ]]; then
    error "CA not found at $CERTS_DIR/ca.crt"
    echo "  Run: cd $CERTS_DIR && bash generate.sh"
    exit 1
fi
success "CA certificate found"

if [[ ! -f "$CERTS_DIR/generate.sh" ]]; then
    error "generate.sh not found"
    exit 1
fi
success "Certificate generator found"

# ── Step 1: Get Pi connection info ──

step 1 "Connect to the Pi"

echo "  How will you connect to the Pi?"
echo ""
echo "    1) Direct SSH (e.g., root@10.0.0.2)"
echo "    2) Via this server to a local network Pi (e.g., root@192.168.x.x)"
echo ""
prompt "Choose [1/2]: "
CONNECT_MODE="$REPLY"

if [[ "$CONNECT_MODE" == "2" ]]; then
    prompt "Pi IP address on local network: "
    PI_IP="$REPLY"
    prompt "Pi SSH password [123]: "
    PI_PASS="${REPLY:-123}"
    SSH_CMD="sshpass -p $PI_PASS ssh -o StrictHostKeyChecking=no root@$PI_IP"
    SCP_CMD="sshpass -p $PI_PASS scp -o StrictHostKeyChecking=no"
    SCP_TARGET="root@$PI_IP"

    # Check sshpass
    if ! command -v sshpass &>/dev/null; then
        error "sshpass not installed. Install with: sudo apt install sshpass"
        exit 1
    fi
else
    prompt "Pi SSH address (e.g., root@10.0.0.2): "
    PI_ADDR="$REPLY"
    PI_IP="${PI_ADDR#*@}"
    SSH_CMD="ssh -o StrictHostKeyChecking=no $PI_ADDR"
    SCP_CMD="scp -o StrictHostKeyChecking=no"
    SCP_TARGET="$PI_ADDR"
fi

echo ""
echo "  Testing connection..."
if $SSH_CMD "echo ok" &>/dev/null; then
    success "Connected to Pi at $PI_IP"
else
    error "Cannot connect to Pi. Check IP and credentials."
    exit 1
fi

# ── Step 2: Get serial number ──

step 2 "Read Pi serial number"

SERIAL=$($SSH_CMD "cat /sys/firmware/devicetree/base/serial-number" 2>/dev/null | tr -d '[:space:]')

if [[ -z "$SERIAL" ]]; then
    error "Could not read serial number"
    exit 1
fi

success "Serial: $SERIAL"

# ── Step 3: Generate certificate ──

step 3 "Generate client certificate"

if [[ -f "$CERTS_DIR/clients/${SERIAL}.crt" ]]; then
    warn "Certificate already exists for $SERIAL"
    prompt "Regenerate? [y/N]: "
    if [[ "$REPLY" =~ ^[Yy]$ ]]; then
        rm -f "$CERTS_DIR/clients/${SERIAL}.crt" "$CERTS_DIR/clients/${SERIAL}.key"
        cd "$CERTS_DIR" && bash generate.sh client "$SERIAL"
    else
        success "Using existing certificate"
    fi
else
    cd "$CERTS_DIR" && bash generate.sh client "$SERIAL"
fi

if [[ -f "$CERTS_DIR/clients/${SERIAL}.crt" ]]; then
    success "Certificate ready: clients/${SERIAL}.crt"
else
    error "Certificate generation failed"
    exit 1
fi

# ── Step 4: Deploy certs to Pi ──

step 4 "Deploy certificates to Pi"

$SSH_CMD "mkdir -p /etc/goball-agent" 2>/dev/null

$SCP_CMD "$CERTS_DIR/ca.crt" "${SCP_TARGET}:/etc/goball-agent/ca.crt"
$SCP_CMD "$CERTS_DIR/clients/${SERIAL}.crt" "${SCP_TARGET}:/etc/goball-agent/client.crt"
$SCP_CMD "$CERTS_DIR/clients/${SERIAL}.key" "${SCP_TARGET}:/etc/goball-agent/client.key"

success "ca.crt deployed"
success "client.crt deployed (CN=$SERIAL)"
success "client.key deployed"

# ── Step 5: Deploy agent (if not baked into image) ──

step 5 "Check agent installation"

AGENT_EXISTS=$($SSH_CMD "test -f /opt/goball-agent/goball_agent.py && echo yes || echo no" 2>/dev/null)

if [[ "$AGENT_EXISTS" == "yes" ]]; then
    success "Agent script already installed"
    prompt "Update to latest version? [y/N]: "
    if [[ "$REPLY" =~ ^[Yy]$ ]]; then
        $SCP_CMD "$AGENT_DIR/goball_agent.py" "${SCP_TARGET}:/opt/goball-agent/goball_agent.py"
        success "Agent updated"
    fi
else
    warn "Agent not found — installing..."
    $SSH_CMD "mkdir -p /opt/goball-agent" 2>/dev/null
    $SCP_CMD "$AGENT_DIR/goball_agent.py" "${SCP_TARGET}:/opt/goball-agent/goball_agent.py"
    success "Agent script installed"

    # Check paho-mqtt
    PAHO=$($SSH_CMD "python3 -c 'import paho.mqtt.client' 2>&1 && echo ok || echo missing" 2>/dev/null)
    if [[ "$PAHO" != *"ok"* ]]; then
        warn "paho-mqtt not installed on Pi"
        echo "  Install manually: pip3 install paho-mqtt"
    fi

    # Install systemd service
    $SSH_CMD 'cat > /etc/systemd/system/goball-agent.service << '\''SVCEOF'\''
[Unit]
Description=GoBall Fleet Monitoring Agent
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/usr/bin/python3 /opt/goball-agent/goball_agent.py
Restart=always
RestartSec=5
Environment=PYTHONUNBUFFERED=1

[Install]
WantedBy=multi-user.target
SVCEOF
systemctl daemon-reload && systemctl enable goball-agent' 2>/dev/null
    success "Systemd service installed and enabled"
fi

# ── Step 6: Configure device ──

step 6 "Configure device"

CONFIG_EXISTS=$($SSH_CMD "test -f /etc/goball-agent.conf && echo yes || echo no" 2>/dev/null)

if [[ "$CONFIG_EXISTS" == "yes" ]]; then
    success "Config file exists"
    # Check if it has the right broker
    CURRENT_HOST=$($SSH_CMD "grep MQTT_BROKER_HOST /etc/goball-agent.conf | cut -d= -f2" 2>/dev/null)
    echo "  Current broker: $CURRENT_HOST"
else
    warn "No config file — creating default..."
fi

prompt "Venue name [Ahmed's Pi]: "
VENUE="${REPLY:-Ahmed's Pi}"

prompt "Device label [Hole 1]: "
LABEL="${REPLY:-Hole 1}"

prompt "City/Location [Stockholm]: "
CITY="${REPLY:-Stockholm}"

# Look up approximate coordinates for common cities
case "${CITY,,}" in
    stockholm)    LAT=59.3293; LNG=18.0686 ;;
    sodertalje)   LAT=59.1955; LNG=17.6253 ;;
    englewood)    LAT=26.9620; LNG=-82.3554 ;;
    miami)        LAT=25.7617; LNG=-80.1918 ;;
    "new york"|nyc) LAT=40.7128; LNG=-74.0060 ;;
    london)       LAT=51.5074; LNG=-0.1278 ;;
    dubai)        LAT=25.2048; LNG=55.2708 ;;
    *)
        prompt "Latitude: "
        LAT="$REPLY"
        prompt "Longitude: "
        LNG="$REPLY"
        ;;
esac

SERVER_IP=$(hostname -I | awk '{print $1}')

$SSH_CMD "cat > /etc/goball-agent.conf << CONFEOF
MQTT_BROKER_HOST=${SERVER_IP}
MQTT_BROKER_PORT=8883
MQTT_USERNAME=goball
MQTT_PASSWORD=changeme

VENUE_NAME=${VENUE}
DEVICE_LABEL=${LABEL}
LATITUDE=${LAT}
LONGITUDE=${LNG}

FLEET_HTTP_PORT=8000
PUBLISH_INTERVAL=30

FAN_ENABLED=1
FAN_TEMP_OFF=40
FAN_TEMP_LOW=50
FAN_TEMP_HIGH=70
FAN_PWM_MIN=80
FAN_PWM_MAX=255

MQTT_TLS_CA=/etc/goball-agent/ca.crt
MQTT_TLS_CERT=/etc/goball-agent/client.crt
MQTT_TLS_KEY=/etc/goball-agent/client.key
CONFEOF" 2>/dev/null

success "Config written: $VENUE - $LABEL ($CITY)"

# ── Step 7: Start agent ──

step 7 "Start agent"

$SSH_CMD "systemctl restart goball-agent" 2>/dev/null
sleep 3

STATUS=$($SSH_CMD "journalctl -u goball-agent --no-pager -n 5 2>&1" 2>/dev/null)

if echo "$STATUS" | grep -q "Connected to MQTT broker"; then
    success "Agent connected to fleet server via mTLS!"
elif echo "$STATUS" | grep -q "mTLS enabled"; then
    warn "Agent started with mTLS but hasn't connected yet (may need a moment)"
else
    error "Agent may have issues. Check logs:"
    echo "  $SSH_CMD 'journalctl -u goball-agent -n 20 --no-pager'"
fi

# ── Done ──

echo ""
echo -e "${GREEN}╔══════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║${BOLD}     Setup Complete!                              ${NC}${GREEN}║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════════╝${NC}"
echo ""
echo "  Device:   $SERIAL"
echo "  Name:     $VENUE - $LABEL"
echo "  Location: $CITY ($LAT, $LNG)"
echo "  Broker:   ${SERVER_IP}:8883 (mTLS)"
echo ""
echo "  Dashboard: http://${SERVER_IP}:8000"
echo ""
echo "  Useful commands:"
echo "    Check agent:  $SSH_CMD 'journalctl -u goball-agent -f'"
echo "    Restart:      $SSH_CMD 'systemctl restart goball-agent'"
echo "    View config:  $SSH_CMD 'cat /etc/goball-agent.conf'"
echo ""
