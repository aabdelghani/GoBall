# GoBall Fleet Monitoring System

Real-time monitoring dashboard for 10,000+ GoBall mini golf devices deployed worldwide. Self-hosted on customer infrastructure with zero recurring cloud costs.

## Architecture

```
     +-------+      +-------+              +-----------------+
     | RPi 1 |----->|       |              |                 |
     +-------+      |       |   subscribe  |    FastAPI      |    WebSocket
     +-------+      | MQTT  |------------->|    Backend      |--------------> Web Dashboard
     | RPi 2 |----->|Broker |              |  (state + DB)   |               (browser)
     +-------+      |       |   commands   |                 |
        ...         |       |<-------------|                 |
     +-------+      |       |              +-----------------+
     | RPi N |----->|       |
     +-------+      +-------+
```

Each Raspberry Pi runs a lightweight Python agent that connects **outbound** to a central MQTT broker. No port forwarding needed — devices work behind any firewall or NAT. If a device loses connection, the broker automatically marks it offline within seconds via Last Will Testament.

## Quick Start

```bash
# 1. Start the broker and backend
docker compose --env-file server.conf up -d

# 2. Open dashboard
open http://localhost:8000

# 3. Simulate devices for testing
pip install paho-mqtt
python tests/fake_agent.py -n 10 -u goball -P changeme

# 4. Deploy agent to a real Pi
scp -r agent/ root@<pi-ip>:/opt/goball-agent/
ssh root@<pi-ip> 'bash /opt/goball-agent/install.sh'
```

## Components

### RPi Agent (`agent/goball_agent.py`)

A Python systemd service running on each Pi that publishes telemetry and executes remote commands.

**System Metrics** (published every 30s):
- CPU temperature (`/sys/class/thermal/thermal_zone0/temp`)
- Memory usage (`/proc/meminfo`)
- Disk usage (`os.statvfs`)
- Uptime (`/proc/uptime`)
- WiFi SSID and signal strength (`iw dev wlan0 link`)
- Application process status (`pgrep`)
- Public/local IP address
- Firmware version (extracted from binary via `strings`)

**Fan Control** (RPi5 Active Cooler):
- Detects fan hardware via `/sys/class/hwmon/` (`pwmfan` device)
- Linear PWM interpolation between configurable temperature thresholds
- Reads RPM from tachometer for failure detection
- Graceful handling when no fan is connected
- Remote configuration via MQTT command

| Temp Range | Fan Behavior |
|-----------|-------------|
| Below 40°C | Off (PWM = 0) |
| 40°C – 50°C | Ramp from PWM 80 to 255 |
| Above 70°C | Full speed (PWM = 255) |

**Game State Parsing**:
- Tails `journalctl -u goball.service` in real-time
- Extracts game mode, player count, scores, hole events, sensor triggers, winners, and errors via regex
- Publishes state changes immediately (not on a timer)

**Remote Commands**:

| Command | Action |
|---------|--------|
| `restart_app` | `systemctl restart goball.service` |
| `restart_agent` | `systemctl restart goball-agent.service` |
| `reboot` | System reboot |
| `set_fan` | Update fan PWM thresholds dynamically |

### MQTT Broker (`broker/`)

Mosquitto with password authentication and topic ACLs.

| Setting | Value |
|---------|-------|
| Max connections | 15,000 |
| Max message size | 65,536 bytes |
| Persistence | Enabled |
| Authentication | Password file |

### Backend (`backend/`)

FastAPI server that subscribes to all device topics and serves the web dashboard.

- **In-memory device state**: ~4KB per device (~40MB for 10K devices)
- **SQLite**: event history and device registry (7-day retention, auto-pruned)
- **WebSocket**: broadcasts every device update to connected dashboards
- **SSH proxy**: browser-based terminal to any online device

### Frontend (`frontend/`)

Single-page web dashboard built with Alpine.js + Tailwind CSS (CDN). No build step, no Node.js.

**Libraries**: Chart.js (charts), Leaflet (maps), xterm.js (terminal)

## MQTT Topics

All topics follow: `goball/{serial}/{category}`

| Topic | Direction | Payload | QoS | Retained |
|-------|-----------|---------|-----|----------|
| `goball/{serial}/status` | Agent → Broker | `"online"` or `"offline"` (LWT) | 1 | Yes |
| `goball/{serial}/system` | Agent → Broker | System metrics JSON (17 fields) | 0 | No |
| `goball/{serial}/game/state` | Agent → Broker | Game mode, players, scores, hole | 1 | No |
| `goball/{serial}/game/event` | Agent → Broker | Score events, sensor triggers, winners | 0 | No |
| `goball/{serial}/hardware` | Agent → Broker | LED, audio, sensor state | 1 | No |
| `goball/{serial}/errors` | Agent → Broker | Error message with module + timestamp | 1 | No |
| `goball/{serial}/command` | Backend → Agent | Remote command (restart, reboot, fan) | 1 | No |
| `goball/{serial}/command/result` | Agent → Broker | Command execution result | 1 | No |

**Device ID** = RPi serial number from `/sys/firmware/devicetree/base/serial-number` (unique, immutable).

**System Metrics Payload:**
```json
{
  "ts": 1709913600.0,
  "hostname": "goball-venue-12",
  "ip": "192.168.1.42",
  "cpu_temp": 52.3,
  "mem_used_mb": 312,
  "mem_total_mb": 4096,
  "disk_used_pct": 34.2,
  "uptime_s": 86400,
  "wifi_ssid": "VenueLAN",
  "wifi_signal_dbm": -45,
  "app_running": true,
  "app_pid": 1234,
  "lat": 40.7128,
  "lng": -74.0060,
  "firmware_version": "1.8.0",
  "fan_pwm": 120,
  "fan_rpm": 3200
}
```

## REST API

### Device Management

| Method | Endpoint | Purpose |
|--------|----------|---------|
| GET | `/api/devices` | List all devices with summary |
| GET | `/api/devices/{serial}` | Full device state |
| GET | `/api/devices/{serial}/events?limit=50` | Event history |
| DELETE | `/api/devices/{serial}` | Remove device + clear MQTT retained msgs |
| DELETE | `/api/devices/{serial}/errors` | Clear in-memory errors |
| DELETE | `/api/devices/{serial}/alerts` | Clear device alerts |
| DELETE | `/api/devices/{serial}/logs` | Delete error logs from database |

### Monitoring

| Method | Endpoint | Purpose |
|--------|----------|---------|
| GET | `/api/alerts` | Active alerts (last 200) |
| GET | `/api/stats` | Fleet-wide statistics (13+ fields) |
| GET | `/api/logs` | All error logs (limit=200) |
| GET | `/api/logs/{serial}` | Error logs for specific device |

### Remote Commands

| Method | Endpoint | Purpose |
|--------|----------|---------|
| POST | `/api/devices/{serial}/command` | Send command to device via MQTT |

Request body:
```json
{
  "action": "restart_app|restart_agent|reboot|set_fan",
  "params": {}
}
```

### Firmware / OTA

| Method | Endpoint | Purpose |
|--------|----------|---------|
| POST | `/api/devices/{serial}/firmware` | Upload & deploy firmware binary (push) |
| POST | `/api/devices/{serial}/firmware/update` | Deploy server's latest firmware (push) |
| GET | `/api/devices/{serial}/firmware/check` | Compare device vs server firmware version |
| GET | `/api/firmware/latest` | Get latest firmware metadata (pull OTA) |
| GET | `/api/firmware/download` | Download firmware binary (pull OTA) |

### WebSocket

| Endpoint | Purpose |
|----------|---------|
| `/ws` | Real-time device updates stream |
| `/ws/terminal/{serial}` | SSH terminal proxy to device |

## OTA Firmware Updates

### Device-Pull Model (Recommended)

The device checks the fleet server for updates and downloads if available:

```
Device                          Server
  |                               |
  |-- GET /api/firmware/latest -->|
  |<-- {version, size, avail} ----|
  |                               |
  |-- GET /api/firmware/download->|
  |<-- binary stream ------------|
  |                               |
  [install locally, restart]      |
```

Stage firmware on the server:
```
fleet/firmware/
  goball          # ELF binary (aarch64)
  VERSION         # Version string (e.g., "1.8.0")
  CHANGELOG       # Optional changelog text
```

### Server-Push Model

Deploy from the dashboard to a specific device:

1. Server validates the binary (ELF header check, 1KB–100MB size)
2. Stops `goball.service` on the device via SSH
3. Uploads binary via SFTP to `/tmp/goball_update`
4. Backs up current binary: `cp /usr/bin/goball /usr/bin/goball.bak`
5. Installs: `mv /tmp/goball_update /usr/bin/goball && chmod +x`
6. Restarts `goball.service`
7. Verifies process started; **auto-rollback** from backup on failure

## Alert System

| Alert Type | Trigger | Default Threshold | Auto-Clear |
|------------|---------|-------------------|------------|
| `offline` | No status update | 300 seconds (5 min) | When device comes online |
| `high_temp` | CPU temperature | > 80°C | When temp drops below |
| `disk_full` | Disk usage | > 90% | When usage drops below |
| `app_down` | goball process not running | Immediate | When process restarts |
| `fan_failure` | PWM > 50 but RPM = 0 | Immediate | When RPM recovers |
| `error` | Any ERROR log from device | Real-time | Manual clear only |

Alerts are deduplicated: one active alert per `serial + alert_type`.

## Dashboard Features

### Home Tab
- Online/offline device counts with badges
- Game mode distribution (doughnut chart)
- CPU temperature distribution (bar chart)
- World map with device locations (Leaflet, green = online, red = offline)
- Live activity feed (resizable, up to 200 entries)

### Devices Tab
- Device grid with status cards showing: serial, hostname, CPU temp, game mode, player count, fan speed, firmware version, error count, uptime
- **Filters**: All, Online, Offline, Errors, Playing
- **Search**: by serial, hostname, or IP
- **Sort**: by status, temperature, serial, game mode
- **Zoom**: 1x, 1.2x, 1.5x, 1.7x font scaling

### Device Detail Panel
- Full system telemetry display
- Recent events (30 items)
- Remote commands (restart app, restart agent, reboot)
- Firmware check and deploy
- Browser-based SSH terminal (xterm.js)
- Clear errors/alerts/logs
- Remove device

### Alerts Page
- Filterable by alert type (offline, high_temp, disk_full, app_down, error, fan_failure)
- Searchable by serial or message
- Sortable columns (time, type, device)
- Color-coded severity icons

### Logs Page
- Error log viewer with device filter
- All-devices or per-device view
- Real-time updates via WebSocket

### SSH Terminal
- Full terminal emulation via xterm.js
- Connects to any online device via SSH proxy
- Supports resize, web links, 256 colors
- Dark theme matching dashboard

## Configuration

### Server (`server.conf`)

```ini
# MQTT
MQTT_BROKER_PORT=1883
MQTT_USERNAME=backend
MQTT_PASSWORD=changeme

# Dashboard
DASHBOARD_PORT=8000

# Database
SQLITE_DB_PATH=/data/fleet.db

# Alert Thresholds
ALERT_OFFLINE_SECONDS=300
ALERT_CPU_TEMP_C=80
ALERT_DISK_USED_PCT=90

# Data Retention
EVENT_RETENTION_DAYS=7

# Device Auth
DEVICE_USERNAME=goball
DEVICE_PASSWORD=changeme

# SSH Terminal
SSH_KEY_PATH=~/.ssh/id_ed25519
SSH_USERNAME=q

# TLS (optional)
# MQTT_TLS_PORT=8883
```

### Agent (`agent.conf`)

```ini
# Server connection
MQTT_BROKER_HOST=fleet.example.com
MQTT_BROKER_PORT=1883
MQTT_USERNAME=goball
MQTT_PASSWORD=changeme

# Location
VENUE_NAME=My Mini Golf Venue
DEVICE_LABEL=Hole 1
LATITUDE=40.7128
LONGITUDE=-74.0060

# Telemetry
PUBLISH_INTERVAL=30

# Fan Control
FAN_ENABLED=1
FAN_TEMP_OFF=40
FAN_TEMP_LOW=50
FAN_TEMP_HIGH=70
FAN_PWM_MIN=80
FAN_PWM_MAX=255

# TLS (optional — see mTLS Setup section)
# MQTT_TLS_CA=/etc/goball-agent/ca.crt
# MQTT_TLS_CERT=/etc/goball-agent/client.crt
# MQTT_TLS_KEY=/etc/goball-agent/client.key
```

All config keys can be overridden via environment variables.

## Deployment

### Server Setup

```bash
# 1. Copy fleet/ to server
scp -r fleet/ user@server:/opt/goball-fleet/

# 2. Configure
cd /opt/goball-fleet
nano server.conf   # Set passwords, SSH key path

# 3. Generate MQTT passwords
docker run -it --rm -v $(pwd)/broker:/mosquitto/config \
  eclipse-mosquitto mosquitto_passwd -b /mosquitto/config/passwd goball <password>
docker run -it --rm -v $(pwd)/broker:/mosquitto/config \
  eclipse-mosquitto mosquitto_passwd -b /mosquitto/config/passwd backend <password>

# 4. Start
docker compose --env-file server.conf up -d
```

### Pi Agent Setup

```bash
# Copy agent to Pi
scp -r fleet/agent/ root@<pi-ip>:/opt/goball-agent/

# Install (creates systemd service, installs paho-mqtt, enables on boot)
ssh root@<pi-ip> 'bash /opt/goball-agent/install.sh'
```

The agent auto-starts on boot and reconnects if the broker goes down.

## Testing

### Fake Agent Simulator

Simulate N devices with realistic telemetry for load testing:

```bash
python tests/fake_agent.py \
  -n 10 \           # Number of devices
  -H localhost \     # Broker host
  -u goball \        # MQTT username
  -P changeme \      # MQTT password
  -i 5               # Publish interval (seconds)
```

Simulates 20 worldwide venues (NYC, London, Paris, Tokyo, Dubai, Sydney, etc.) with randomized metrics, game events, and occasional errors.

## Scaling

| Metric | Value |
|--------|-------|
| Max concurrent devices | 10,000+ |
| Messages/second | ~450 (350 system + 100 game events) |
| Inbound bandwidth | ~330 KB/s |
| Backend memory | ~40 MB (10K devices) |
| Per-Pi agent RAM | ~15 MB |
| Per-Pi network | ~1 KB/30s |
| Event retention | 7 days (auto-pruned) |

## Project Structure

```
fleet/
├── docker-compose.yml           # Mosquitto + Backend containers
├── server.conf                  # Server configuration
├── .env.example                 # Config template
├── agent/
│   ├── goball_agent.py          # Metrics, log parser, fan control, commands
│   ├── agent.conf               # Agent config template
│   ├── goball-agent.service     # systemd unit file
│   └── install.sh               # One-command Pi installer
├── broker/
│   ├── mosquitto.conf           # Auth, persistence, connection limits
│   └── acl.conf                 # Topic ACL rules
├── certs/
│   └── generate.sh              # CA + server + client cert generator
├── backend/
│   ├── app.py                   # FastAPI: REST + WebSocket + static files
│   ├── device_manager.py        # In-memory state + SQLite persistence
│   ├── models.py                # Pydantic data models
│   ├── mqtt_client.py           # MQTT subscriber + message router
│   ├── websocket_manager.py     # WebSocket broadcasting
│   ├── firmware_deploy.py       # SSH firmware deployment + rollback
│   ├── ssh_terminal.py          # WebSocket-to-SSH proxy
│   ├── requirements.txt         # Python dependencies
│   └── Dockerfile               # Container image
├── frontend/
│   ├── index.html               # Main dashboard
│   ├── app.js                   # Dashboard logic + WebSocket
│   ├── alerts.html              # Alerts page
│   └── logs.html                # Error logs page
├── firmware/
│   ├── goball                   # Latest firmware binary (for OTA)
│   ├── VERSION                  # Version string
│   └── CHANGELOG                # Optional changelog
└── tests/
    └── fake_agent.py            # Device simulator for testing
```

## mTLS (Mutual TLS) — Secure Pi-to-Server Communication

### What is mTLS?

Standard TLS (like HTTPS) only verifies the server's identity — the client checks that the server has a valid certificate. **Mutual TLS (mTLS)** goes both ways: the server also verifies the client's identity via a client certificate.

In the GoBall fleet:
- The **server** (Mosquitto broker) proves its identity to each Pi using a server certificate
- Each **Pi** proves its identity to the server using a unique client certificate
- The client certificate's Common Name (CN) is the Pi's serial number
- Mosquitto uses `use_identity_as_username true` so the CN automatically becomes the MQTT username
- No shared passwords needed — each device is cryptographically authenticated

**Why mTLS?**
- Prevents man-in-the-middle attacks
- Eliminates shared passwords (no `goball/changeme` credentials to leak)
- Per-device identity: each Pi has its own cert, and ACLs restrict it to its own topics (`goball/<serial>/#`)
- Revoke a single device by removing its cert — no password rotation needed across all devices

### How It Works

```
        Pi (Client)                           Server (Broker)
        ──────────                           ────────────────
        Has: ca.crt                          Has: ca.crt
             client.crt (CN=serial)               server.crt
             client.key                           server.key

        1. Pi connects to server:8883
        2. Server sends server.crt ──────>   Pi verifies with ca.crt ✓
        3. Server requests client cert
        4. Pi sends client.crt ──────────>   Server verifies with ca.crt ✓
        5. Server reads CN from cert ────>   CN = "09cbb6816c00f3dd" = MQTT username
        6. ACL check: can this CN access goball/09cbb6816c00f3dd/# ? ✓
        7. Connection established, fully encrypted
```

### First-Time Server Setup

**1. Generate the Certificate Authority and server cert:**

```bash
cd fleet/certs

# Generate CA + server cert (only once, ever)
BROKER_HOSTNAME=your-server-hostname.com bash generate.sh
```

This creates:
- `ca.crt` + `ca.key` — the Certificate Authority (valid 10 years)
- `server.crt` + `server.key` — the broker's certificate (valid ~2 years)

The `BROKER_HOSTNAME` must match the hostname/IP that Pis will connect to. You can include multiple IPs by editing `generate.sh`.

**2. Enable the mTLS listener in `broker/mosquitto.conf`:**

Uncomment the mTLS block:

```
listener 8883 0.0.0.0
cafile /mosquitto/config/certs/ca.crt
certfile /mosquitto/config/certs/server.crt
keyfile /mosquitto/config/certs/server.key
require_certificate true
use_identity_as_username true
tls_version tlsv1.2
```

**3. Ensure cert file permissions allow the mosquitto container to read them:**

```bash
chmod 644 certs/server.key certs/ca.key
```

**4. Restart the fleet:**

```bash
docker compose --env-file server.conf down && docker compose --env-file server.conf up -d
```

Verify mosquitto starts without errors:

```bash
docker logs fleet-mosquitto-1
```

You should see no `Error: Unable to load` messages. Both listeners (1883 and 8883) should be active.

### Adding a New Pi Device

**1. Get the Pi's serial number:**

```bash
ssh root@<pi-ip> 'cat /sys/firmware/devicetree/base/serial-number'
# Example output: 09cbb6816c00f3dd
```

**2. Generate a client certificate for this Pi (on the server):**

```bash
cd fleet/certs
bash generate.sh client 09cbb6816c00f3dd
# Creates: clients/09cbb6816c00f3dd.crt and clients/09cbb6816c00f3dd.key
```

You can batch-generate for multiple devices:

```bash
bash generate.sh client serial1 serial2 serial3
```

**3. Copy the certs to the Pi:**

```bash
ssh root@<pi-ip> 'mkdir -p /etc/goball-agent'
scp fleet/certs/ca.crt root@<pi-ip>:/etc/goball-agent/ca.crt
scp fleet/certs/clients/<serial>.crt root@<pi-ip>:/etc/goball-agent/client.crt
scp fleet/certs/clients/<serial>.key root@<pi-ip>:/etc/goball-agent/client.key
```

**4. Copy the agent script:**

```bash
ssh root@<pi-ip> 'mkdir -p /opt/goball-agent'
scp fleet/agent/goball_agent.py root@<pi-ip>:/opt/goball-agent/goball_agent.py
```

**5. Create the config file on the Pi (`/etc/goball-agent.conf`):**

```ini
MQTT_BROKER_HOST=<your-server-ip-or-hostname>
MQTT_BROKER_PORT=8883
MQTT_USERNAME=goball
MQTT_PASSWORD=changeme

VENUE_NAME=<venue name>
DEVICE_LABEL=<hole name>
LATITUDE=<decimal latitude>
LONGITUDE=<decimal longitude>

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
```

When `MQTT_TLS_CERT` and `MQTT_TLS_KEY` are set, the agent uses cert auth and ignores `MQTT_USERNAME`/`MQTT_PASSWORD`. The port auto-switches to 8883 if set to 1883.

**6. Install paho-mqtt** (if not baked into the Yocto image):

```bash
# From a machine with internet, download the wheel:
pip3 download paho-mqtt==2.1.0 --platform manylinux2014_aarch64 --python-version 312 --only-binary=:all: -d /tmp/wheels

# Copy to Pi and install:
scp /tmp/wheels/paho_mqtt-2.1.0-*.whl root@<pi-ip>:/tmp/
ssh root@<pi-ip> 'pip3 install /tmp/paho_mqtt-2.1.0-*.whl'
```

**7. Install and start the systemd service:**

```bash
ssh root@<pi-ip> 'cat > /etc/systemd/system/goball-agent.service << '\''EOF'\''
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
EOF
systemctl daemon-reload && systemctl enable goball-agent && systemctl start goball-agent'
```

**8. Verify the connection:**

```bash
ssh root@<pi-ip> 'journalctl -u goball-agent --no-pager | tail -5'
```

You should see:

```
mTLS enabled (CA: /etc/goball-agent/ca.crt, cert: /etc/goball-agent/client.crt)
Connected to MQTT broker (rc=Success)
Subscribed to goball/<serial>/command
```

The device should appear on the dashboard within 30 seconds.

### Architecture

| Port | Auth Method | Used By | Network |
|------|------------|---------|---------|
| 1883 | Password (`backend`/`goball` users) | Backend ↔ Broker | Docker internal only |
| 8883 | mTLS (client certificate) | Pi agents → Broker | External (internet) |

The backend connects to the broker on port 1883 inside the Docker network using password auth. Pi agents connect from the outside on port 8883 using mTLS. The two listeners are independent — disabling one doesn't affect the other.

### Troubleshooting

| Problem | Cause | Fix |
|---------|-------|-----|
| `certificate is not yet valid` | Clock skew between Pi and server | Sync time: `ntpdate pool.ntp.org` on Pi |
| `IP address mismatch` | Server cert SAN doesn't include the IP the Pi connects to | Regenerate server cert with correct `BROKER_HOSTNAME` or add IP to SAN in `generate.sh` |
| `certificate verify failed` | Wrong CA cert on Pi, or cert expired | Check `ca.crt` matches server's, check cert dates with `openssl x509 -in cert.crt -noout -dates` |
| Agent connects but device doesn't appear | ACL blocking — CN doesn't match topic pattern | Check broker logs: `docker logs fleet-mosquitto-1`, verify cert CN matches expected serial |
| `Permission denied` on broker startup | Mosquitto can't read cert key files | `chmod 644 certs/server.key certs/ca.key` |

## Security

- **mTLS**: mutual certificate authentication for Pi-to-broker connections (port 8883)
- **Per-device isolation**: cert CN = serial, ACL `goball/%u/#` restricts each device to its own topics
- MQTT password authentication for internal backend-to-broker communication
- Topic ACLs restrict device publishing scope
- SSH key-based authentication for firmware deploy and terminal proxy
- ELF header validation prevents uploading invalid firmware
- Firmware rollback on failed deployment
- No credentials stored in frontend
- Docker isolation for broker and backend services

## License

MIT
