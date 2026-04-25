# GoBall - Mini Golf Scoring System

A Raspberry Pi 5-based mini golf scoring system with an LVGL touchscreen UI, IR sensor detection, LED strip feedback, and voice announcements. Built with SDL2 backend for cross-compilation from a Linux host to aarch64 Raspberry Pi targets.

## Features

### Game Modes
- **Stroke Play** - 1 to 4 players, 9 or 18 holes
- **Match Play 1v1** - Head-to-head, 9 or 18 holes with early victory detection
- **Match Play 2v2** - Team-based, 9 or 18 holes with early victory detection
- **Quota Points** - 1 to 4 players, 9 or 18 holes with preset par targets counting down to zero
- **Vegas Quota Points** - 1 to 4 players, 9 or 18 holes with bonus scoring after completing par categories; category locks when 2 players close it

### Scoring
- IR sensors detect ball entry into 4 scoring holes: 3 points, 4 points, 5 points, and 0 points
- Automatic score tracking and cumulative scorecard display
- Match Play uses "Up & Down" format (e.g., "3 & 2") with early victory when lead exceeds remaining holes
- Quota Points: preset par targets (3pt, 4pt, 5pt) count down to zero — first to complete wins
- Vegas Quota Points: same countdown, but bonus points earned when hitting a completed category; category dies (no more bonus) once 2 players close it

### Audio
- Voice announcements for player turns, game mode selection, and winner declarations
- 5 randomized winner voice lines per player/team for variety
- Configurable sound delays for announcements and turn switches
- Mute/unmute support

### Visual Feedback
- WS2812 LED strip integration via PIO on Raspberry Pi 5
- Color-coded flash on scoring (green for points, white for zero)
- Player turn highlighting patterns

### Video Playback
- Tips > Visualize screen plays instructional video via external mpv subprocess
- Native LVGL controls: green-themed play/pause button + seekbar slider below video
- Play button shows pause icon during playback, play icon when paused, rewind icon when video finishes
- Seekbar updates in real-time (polled every 500ms via mpv IPC); drag to seek
- Video layout: 6:1 grid ratio — Row 1 = video, Row 2 = controls (play button + seekbar)
- Video stays on last frame when finished (`--keep-open=yes`); tap rewind to replay
- Window dragging disabled (`--no-window-dragging`) to prevent accidental repositioning
- Always-on-top enforced by `mpv-raise` systemd service using `wlr-foreign-toplevel-management` Wayland protocol
- Forced window positioning (`--force-window-position` + `--geometry=WxH+X+Y`)
- mpv IPC socket (`/tmp/mpv-ipc`) for pause/seek/position polling from LVGL
- Auto-detects video dimensions via ffprobe
- Child process auto-terminates when parent app exits (PR_SET_PDEATHSIG)

### Player Names
- Tap any player name label to edit via on-screen keyboard
- Custom names persist across all game screens and scorecards for the session
- Names reset to defaults ("Player 1"–"Player 4") when returning to main menu
- Dynamic turn display labels also use custom names

### WiFi Manager
- Tap the G hexagon logo on the home screen to open WiFi network selector
- Scans available networks via NetworkManager (`nmcli`) with signal strength and security info
- Scrollable network list sorted by signal strength with green scrollbar
- Connected network highlighted with green border and checkmark icon at top of list
- Password entry via on-screen LVGL keyboard with password masking
- Back button to return from password entry to network list
- Disconnect button (red) to drop current connection and switch networks
- WiFi status icon in bottom-right corner: green when connected, red when disconnected
- Auto-polling every 5 seconds to update connection status

### Fleet Monitoring
- Real-time web dashboard for 10,000+ devices worldwide (MQTT + FastAPI + Alpine.js)
- RPi agent: system metrics (CPU temp, memory, disk, WiFi, fan), game state via journalctl parsing
- MQTT broker (Mosquitto): LWT offline detection, per-device ACLs, retained messages
- Alerts: offline, high temp, disk full, app down, fan failure
- Remote commands: restart app, reboot, fan config via MQTT
- OTA firmware updates via SSH with auto-rollback
- Browser-based SSH terminal to any online device
- World map with device locations, game mode charts, activity feed
- RPi5 Active Cooler fan control with configurable PWM thresholds
- Self-hosted on customer server via Docker (zero cloud costs)

### UI
- Built with LVGL and SquareLine Studio
- Automatic screen transitions to scorecard on game completion
- Crown icon for Match Play leader and Quota Points winner
- Per-hole and cumulative score display
- Player turn highlighting for all modes (2P cycles every 8, 3P every 12, 4P every 24 detections)

## Hardware

- Raspberry Pi 5
- 7" touchscreen display (SDL2/DRM backend)
- 4x IR sensors (GPIO pins 17, 26, 27, 24)
- WS2812 LED strip (via PIO)
- Speaker/amplifier for audio output

## Project Structure

```
├── main.c                      # Application entry point
├── CMakeLists.txt              # Build configuration
├── modules/
│   ├── debug/                  # Configurable debug logging (ERROR/WARN/INFO/DEBUG/TRACE)
│   ├── game_modes/             # Game logic
│   │   ├── strokeplay.c/h      # Stroke play scoring
│   │   ├── matchplay.c/h       # Match play scoring with early victory
│   │   ├── quotaplay.c/h       # Quota and Vegas Quota scoring
│   │   ├── game_modes.h        # Game mode enums
│   │   └── player.h            # Player struct definition
│   ├── game_sounds/            # WAV audio assets
│   ├── game_videos/            # Video playback via mpv subprocess
│   ├── led_logic/              # WS2812 LED strip control
│   ├── logic/                  # GPIO event handling, debounce, game flow
│   ├── player_name/            # Editable player names with on-screen keyboard
│   ├── sound_logic/            # SDL2_mixer audio system
│   ├── wifi_manager/           # WiFi network scanning, connection, and status
│   └── ui_logic/               # UI event handlers
├── ui/                         # LVGL UI (exported from SquareLine Studio)
│   ├── screens/                # All game screens
│   ├── images/                 # Image assets (as C arrays)
│   ├── fonts/                  # Font assets
│   └── components/             # Reusable UI components
├── sdl2-dev-rpi64/             # Pre-built SDL2 for aarch64 cross-compilation
├── utils/
│   ├── piolib/                 # PIO library for WS2812 LED control
│   ├── autostart/              # Desktop files for kiosk mode
│   └── test/                   # Test utilities and sound tests
├── fleet/                         # Fleet monitoring system
│   ├── agent/                     # RPi agent (Python, systemd service)
│   ├── backend/                   # FastAPI server (MQTT sub, REST, WebSocket)
│   ├── frontend/                  # Web dashboard (Alpine.js, Tailwind, Leaflet)
│   ├── broker/                    # Mosquitto MQTT config
│   ├── tests/                     # Fake agent simulator
│   └── docker-compose.yml         # Mosquitto + backend containers
├── tools/
│   ├── goball_dashboard.py         # 5-tab development dashboard (deploy, GPIO, test, logs, config)
│   ├── gpio_loopback_simulator.py  # GPIO test simulator
│   ├── install_requirements.sh     # Install dashboard dependencies
│   └── mpv-raise/                  # Wayland tool to keep mpv always on top (wlr-foreign-toplevel)
├── scripts/                    # Build and dependency scripts
└── Dockerfile                  # Docker cross-compilation environment
```

## Building

### Prerequisites

- **Host**: Linux with `cmake` (>= 3.15), `make`, `pkg-config`
- **Cross-compiler**: `aarch64-linux-gnu-gcc` (from `gcc-aarch64-linux-gnu` package)
- **Sysroot**: A copy of the RPi5 filesystem at `rpi5-sysroot/` in the project root, containing at minimum:
  - `usr/include/` — kernel headers, gpiod.h, SDL2 headers
  - `usr/lib/` — libgpiod, libSDL2, libSDL2_mixer shared libraries and pkg-config files
- **Target RPi packages** (installed on the Pi): `libsdl2-2.0-0`, `libsdl2-mixer-2.0-0`, `libgpiod2`, `mpv`

### CMake Build

```bash
# Configure (uses toolchain-aarch64.cmake, auto-finds rpi5-sysroot/ in project root)
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
make -C build -j$(nproc)

# Custom sysroot location (if not in project root)
cmake -B build -DSYSROOT_PATH=/path/to/rpi5-sysroot
```

#### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | `Debug` | `Debug` (level 4), `Release` (level 2) |
| `ENABLE_DEBUG_TRACE` | `OFF` | Enable TRACE-level logging (level 5) |
| `ENABLE_DEBUG_COLORS` | `ON` | Colored terminal output |
| `ENABLE_DEBUG_TIMESTAMP` | `ON` | Timestamps in log output |
| `SYSROOT_PATH` | `rpi5-sysroot/` | Override sysroot location |
| `SOUND_DIR_PATH` | — | Override sound file directory (for Yocto) |
| `YOCTO_BUILD` | — | Skip local toolchain/SDL paths for Yocto |

### Script Build

```bash
# Native cross-compile
./build-cross.sh

# Or with Docker (no local toolchain needed)
./docker-build-and-run.sh
```

### VSCode Build

1. Open the project folder in VSCode
2. Install C/C++ Extension Pack and CMake Tools
3. Run **CMake: Configure** (Ctrl+Shift+P) — generates `compile_commands.json` for IntelliSense
4. Select the aarch64 toolchain from the bottom toolbar
5. Click Build

### Deploy to Raspberry Pi

```bash
scp build/goball root@<rpi-ip>:/usr/bin/goball
```

### GPIO Test Simulator

For testing without physical IR sensors, use the loopback simulator on the Pi itself:

```bash
python3 tools/gpio_loopback_simulator.py
```

### Development Dashboard

A 5-tab tkinter GUI for managing the full dev workflow from the host machine:

```bash
# Install dependencies (tkinter, SSH tools)
./tools/install_requirements.sh

# Launch dashboard
python3 tools/goball_dashboard.py
```

**Tabs:**
1. **Deploy & Run** — Build, SCP deploy, start/stop/kill/restart app on Pi, configurable Pi IP/user/display
2. **GPIO Simulator** — Trigger IR sensors remotely via SSH + gpiod
3. **Test Harness** — Automated game scenarios (P1-P4 wins, full rounds, custom sequences)
4. **Log Analyzer** — Live tail of app logs with module/level filtering and color-coded output
5. **Config Editor** — Edit sound delays, debug levels, view source files

## Configuration

### Sound Timing (modules/sound_logic/sound_logic_event.h)
| Define | Default | Description |
|--------|---------|-------------|
| `SOUND_DELAY_PLAYER_ANNOUNCE_MS` | 750ms | Player turn announcement delay |
| `SOUND_DELAY_TEAM_ANNOUNCE_MS` | 750ms | Team turn announcement delay |
| `SOUND_DELAY_PLAYER_WINS_MS` | 750ms | Winner announcement delay |
| `SOUND_DELAY_TEAM_WINS_MS` | 750ms | Team winner announcement delay |
| `SOUND_DELAY_TURN_SWITCH_MS` | 500ms | Turn switch announcement delay |

### Sensor Debounce (modules/logic/gpio_event.h)
| Define | Default | Description |
|--------|---------|-------------|
| `DEBOUNCE_TIME_MS` | 3000ms | Signal-based POSIX timer debounce |

### Debug Levels (modules/debug/debug.h)
Set `DEBUG_LEVEL` to control output verbosity: `ERROR(1)`, `WARN(2)`, `INFO(3)`, `DEBUG(4)`, `TRACE(5)`

**Debug modules:** `MAIN`, `LVGL`, `UI`, `GAME`, `SOUND`, `LED`, `GPIO`, `INPUT`, `ANIMATION`, `LOGIC`, `HAL`, `VIDEO`

## Changelog

### v1.9.5 — 04/25/2026
- **PGA Tour Radio in background**: live HLS audio (`https://video-distribution.pgatourhq.com/pgatour-radio/pgatour-radio_1.m3u8`) plays during gameplay; auto-restarts on stream drop via mpv `--cache=yes --loop=inf`
- **Tap-to-toggle radio icon**: speaker glyph on the home screen (just left of the WiFi icon) — green when on, grey when off; persists across reboot
- **`radio_manager` module**: spawns mpv as `--no-video` PulseAudio client at `--volume=60` so SDL2_mixer game effects stay dominant; cleaned up via `PR_SET_PDEATHSIG`
- **Persistence**: `/etc/goball-radio.conf` (`ENABLED`, `URL`, `REFERER`, `VOLUME`); shipped with `ENABLED=0` so the toggle is opt-in on first boot
- **FFmpeg/mpv HTTPS**: enabled `--enable-openssl` in FFmpeg via `ffmpeg_%.bbappend` so mpv can open `https://` streams (was failing with `No protocol handler found`)
- **Required HTTP header**: PGA CloudFront rejects fetches without `Referer: https://www.pgatour.com/` — passed via `--http-header-fields`; URL is also US-geo-locked

### v1.8.0 — 03/15/2026
- **Fleet monitoring dashboard**: real-time web UI for monitoring 10,000+ GoBall devices worldwide via MQTT
- **RPi agent**: Python systemd service publishing system metrics (CPU temp, memory, disk, WiFi, uptime) every 30s and game events via journalctl parsing
- **MQTT broker**: Mosquitto with password auth, per-device ACLs, Last Will Testament for automatic offline detection
- **Backend API**: FastAPI with 22 REST endpoints, WebSocket real-time updates, in-memory device state, SQLite event history
- **Dashboard features**: device grid with filters/search/sort, world map (Leaflet), game mode charts (Chart.js), live activity feed, alert panel (6 alert types)
- **Remote management**: restart app/agent/reboot via MQTT commands, OTA firmware deploy via SSH with auto-rollback, browser-based SSH terminal (xterm.js)
- **RPi5 fan control**: PWM fan control via sysfs with configurable temp thresholds, linear interpolation, fan failure alerts
- **Agent WiFi fix**: switched from nmcli to iw for WiFi info (no NetworkManager dependency)
- **Self-hosted deployment**: Docker Compose (Mosquitto + FastAPI), zero cloud costs, deployable to customer server

### v1.7.0 — 03/06/2026
- **WiFi network manager**: tap the G hexagon logo on the home screen to scan, select, and connect to WiFi networks via touchscreen UI
- **Network scanning**: uses `nmcli` to list available networks with SSID, signal strength percentage, and security type; deduplicates SSIDs keeping strongest signal
- **Password entry**: on-screen LVGL keyboard with password masking, dark-themed keys matching player name keyboard style
- **Connection status**: current connected network shown at top of list with green border and checkmark; auto-polls every 5 seconds
- **WiFi status icon**: bottom-right corner of home screen — green WiFi symbol when connected, red when disconnected
- **Disconnect support**: red disconnect button when connected, allowing users to switch networks
- **Back button**: 140px-wide back button on password dialog to return to network list without connecting
- **Overlay pattern**: reuses `lv_layer_top()` dark overlay from player name module; tap background or close button to dismiss

### v1.6 — 03/06/2026
- **Native LVGL video controls**: replaced mpv Lua OSC (`minimal-osc.lua`) with native LVGL play/pause button and seekbar slider, eliminating z-order and positioning issues with mpv's ASS overlay
- **Play/Pause/Rewind button**: shows pause icon during playback, play icon when paused, rewind icon (`LV_SYMBOL_REFRESH`) when video reaches EOF; tap to restart from beginning
- **Seekbar with IPC polling**: LVGL slider polls `percent-pos` every 500ms via mpv IPC socket; drag-to-seek with `absolute-percent` command
- **Grid layout**: 6:1 row ratio — video fills top 6/7 of panel, controls occupy bottom 1/7 with play button (col 1) and seekbar (col 2)
- **Forced window positioning**: added `--force-window-position` flag for mpv on Wayland/labwc
- **Panel position tuning**: runtime adjustment of panel Y offset (-22px) and padding removal for tighter video-to-controls fit
- **Removed redundant ontop IPC**: removed `set_property ontop true` from timer callback; `mpv-raise.service` is now the sole always-on-top enforcer
- **mpv IPC property queries**: added `mpv_ipc_get_property_number()` and `mpv_ipc_get_property_bool()` for non-blocking property polling (percent-pos, pause, eof-reached)

### 03/05/2026
- **Switched video player from ffplay to mpv**: replaced ffplay subprocess with mpv for video playback
- **Custom minimal OSC**: created `minimal-osc.lua` — green-themed (#00F46A) on-screen controller with play/pause button and seekbar; always visible (no auto-hide); uses ASS drawing on mpv OSD overlay
- **mpv OSC requires Lua**: mpv rebuilt in Yocto image with `-Dlua=enabled` to support the on-screen controller script
- **Keep-open on finish**: video stays on last frame (`--keep-open=yes`) instead of closing when playback ends
- **No window dragging**: added `--no-window-dragging` to prevent accidental repositioning via click-and-drag
- **Explicit window positioning**: geometry now includes position (`--geometry=WxH+X+Y`), centered within LVGL panel inner area
- **Removed LVGL video controls**: removed custom pause button, seek slider, progress timer, and control bar — custom Lua OSC handles all playback interaction
- **mpv IPC socket**: added `--input-ipc-server=/tmp/mpv-ipc` for programmatic control; ontop enforcer timer sends `set_property ontop true` every 500ms as fallback
- **mpv-raise systemd service**: created `tools/mpv-raise/mpv-raise.c` — a Wayland client that uses the `wlr-foreign-toplevel-management` protocol to force-activate the mpv window every 500ms, ensuring it stays on top of the maximized goball window on labwc compositor
- **Cross-compiled mpv-raise**: built `mpv-raise` for aarch64 using wayland-scanner-generated protocol bindings; deployed to `/usr/bin/mpv-raise` with systemd service at `/etc/systemd/system/mpv-raise.service`
- **labwc config updates**: added mpv window rule with `ToggleAlwaysOnTop`, `ignoreFocusRequest`, `fixedPosition`; set `followMouse=yes` and `raiseOnFocus=no` to prevent click-to-raise from hiding mpv

### 03/01/2026
- **Borderless window**: removed title bar from main application window via `SDL_SetWindowBordered`; window title set to "app"
- **Video path fix**: changed hardcoded `/home/q/...` video path to relative `modules/game_videos/` — works on any Pi regardless of username
- **Stroke Play 1P highlight fix**: added `case 1:` for single-player 9H and 18H — removes "Unsupported player count" warning
- **Player name 1P scorecard fix**: registered missing `ui_SP19HPScPText` label so edited names appear on 1P 9H scorecard
- **Video debug logging**: replaced raw `fprintf` with `DEBUG_*` macros via new `MODULE_VIDEO`; file existence and permission checks before playback; ffprobe/ffplay error reporting with `strerror`; exit status logging for ffplay process
- **Audio debug logging**: audio format info (freq/format/channels) logged on init; file access checks before `Mix_LoadWAV`; channel assignment logged on play; cleanup logging on shutdown
- **Yocto video path**: added `#ifdef YOCTO_BUILD` conditional in `game_videos.h` — uses `/opt/goball/videos/` on Yocto, relative path on desktop
- **YOCTO_BUILD define**: added `add_compile_definitions(YOCTO_BUILD=1)` in CMakeLists.txt so the preprocessor flag reaches C code
- **Yocto rpidistro-ffmpeg**: added `rpidistro-ffmpeg` as runtime dependency with SDL2 PACKAGECONFIG enabled (bbappend) — provides ffplay on RPi5
- **Yocto video install**: recipe now installs `.mp4` files from `modules/game_videos/` to `/opt/goball/videos/` on the image
- **Yocto branch switch**: goball recipe now builds from `ui-redesign` git branch instead of `master`
- **Yocto labwc compositor**: replaced Weston with labwc (wlroots-based compositor matching Raspi OS) — supports window positioning rules for ffplay video overlay; created wlroots 0.17.4 and labwc 0.7.4 Yocto recipes from scratch
- **Psplash loading bar**: custom progress bar positioned over the green rectangle in the splash image (x+564, y=344) with green theme colors (#00F46A)
- **Runtime video positioning**: video overlay now reads LVGL panel coordinates at runtime via `lv_obj_get_coords()` instead of hardcoded values — adapts to UI layout changes
- **LVGL 9.x API fix**: replaced non-existent `lv_area_get_x1()`/`lv_area_get_y1()` with direct `lv_area_t` struct field access (`.x1`, `.y1`)
- **Video debug enhancements**: added ffplay command logging, SDL window position logging, LVGL panel coord logging, and early-death detection (100ms check after spawn)

### 02/28/2026
- **Development Dashboard** (`tools/goball_dashboard.py`): 5-tab tkinter GUI — deploy & run, GPIO simulator, test harness, log analyzer, config editor
  - File browser for binary and extra modules to deploy
  - Configurable Pi IP, username, SSH key, deploy path, and Wayland display
  - Start/Stop/Kill/Restart/Reboot remote app controls
  - P1-P4 wins test scenarios with realistic scoring patterns
  - Live log tail with ANSI color stripping, module/level filtering
  - Fix: SSH timeout on app start resolved with `nohup setsid ... disown`
  - Fix: `pkill -f` instead of `pkill -x` for process names >15 chars
  - Fix: Thread-safe tkinter updates from background SSH threads
- **Player Name Editing**: tap any player name label to rename via on-screen keyboard overlay
  - Custom names update across all game screens, scorecards, and turn displays
  - Names reset to defaults on return to main menu
  - Montserrat 32 font with icon glyph support for keyboard
- Video playback on Tips > Visualize screen using external ffplay subprocess overlaid on the LVGL panel
- Seek slider for video scrubbing with auto-detected duration via ffprobe
- Pause/resume via SIGSTOP/SIGCONT signals to ffplay process
- Always-on-top video window via labwc window rule (`~/.config/labwc/rc.xml`)
- Auto-cleanup: ffplay child process killed on parent exit via `prctl(PR_SET_PDEATHSIG)`
- Back button stops video and navigates without re-triggering during screen fade-out animation

### 02/27/2026
- Vegas Quota Points: category locks when 2 players close it (3P/4P) — no more bonus scoring on dead categories
- Fix: Quota Points and Vegas Quota Points main menu button now plays "main menu" sound instead of "going back"
- Fix: Toolchain sysroot now auto-resolves to project-local `rpi5-sysroot/` (no more hardcoded path)
- CMake accepts `-DSYSROOT_PATH=...` to override sysroot location
- Updated README build instructions with CMake options and `compile_commands.json` for IntelliSense

### 02/15/2026
- Vegas Quota Points 4-player mode (9H and 18H) with highlighting, crown, turn announcements, ball counter, and bonus score display
- Vegas Quota Points 4P winner logic: first to complete wins; simultaneous finish compares scores (highest wins, tie shows crowns)
- Vegas Quota Points 3-player mode (9H and 18H) with highlighting, crown, turn announcements, ball counter, and bonus score display
- Vegas Quota Points 3P winner logic: first to complete wins; simultaneous finish compares scores (highest wins, tie shows both crowns)
- Vegas Quota Points 2-player mode (9H and 18H) with highlighting, crown, turn announcements, ball counter (2→1 cycle), and bonus score display
- Vegas Quota Points 1-player 18H mode
- Vegas Quota Points 1-player 9H mode with bonus scoring after completing par categories
- Crowns hidden at game start for Vegas Quota 2P, 3P, and 4P (shown only on winner)
- Fix: Vegas Quota home screen button now plays correct sound instead of "going back"
- Fix: Vegas Quota scoring only adds points after a category reaches 0 (not from start)
- Fix: Multiple Vegas Quota main menu buttons were deleting wrong screens

### 02/14/2026
- Quota Points 4-player mode (9H and 18H) with highlighting, crown, and turn announcements
- Quota Points 3-player mode (9H and 18H) with highlighting, crown, and turn announcements
- Quota Points 2-player mode with winner logic, highlighting, and sensor control
- Added score reset on quota back/main menu, initial P1 highlight and par label reset

### 02/13/2026
- Quota Points 1-player mode (9H and 18H) fully implemented with scoring and completion

### 02/03/2026
- Match Play (1v1 & 2v2, 9/18 holes) now automatically navigates to scorecard on game end
- Match Play announces winner from pool of 5 random voice lines with delay
- Stroke Play (1-4 players, 9/18 holes) auto-navigates to scorecard and announces winner
- Fix: End of Match Play no longer says hardcoded "Player 1 Turn"
- Fix: Debounce changed from time-based to interrupt-based using POSIX signals (3s debounce)
- All sound timing delays consolidated into configurable macros
- Added GPIO loopback test simulator

### 02/02/2026
- Match Play 2v2 (9 and 18 holes) fully implemented and working
- Match Play 1v1 (18 holes) implemented with proper reset
- Crown icon added for Match Play leader (1v1 and 2v2, 9 and 18 holes)
- Fix: Stops announcing points after game completion (crown displayed)
- Fix: Player name announced at each turn for 1v1 (9 and 18 holes)
- Scorecard for Match Play working with proper ball/hole reset

### 01/12/2026
- Match Play scorecard now functional

### 01/11/2026
- Fixed Docker build for both release and debug configurations

### 11/05/2025
- Match Play 1v1 (9 holes) corner case: player comeback calculated correctly
- Fix: Sounds no longer play after game completion
- Added configurable debug logging system (activate/deactivate per module)

### 11/04/2025
- Match Play 1v1 (9 holes) initial implementation with UI updates

### 10/20/2025
- Abstracted game modes into dedicated `game_modes/` directory (strokeplay, matchplay)

### 10/14/2025
- Added Quota Points, Vegas Quota Points, and Match Play mode structures

### Earlier
- Stroke Play 1-4 players (9/18 holes) with scoring, highlighting, and scorecard
- Sound integration: game sounds, player turn announcements, point announcements
- LED strip integration with color-coded scoring feedback
- Non-blocking audio using `play_sound_once` timer system
- Player turn voice only after completing all balls (not every sensor trigger)
- Fix: Sensor-triggered scoring no longer occurs on main menu screen (`sensors_enabled` flag)
- Fix: 18-hole 2-player mode now correctly records final score on scorecard
