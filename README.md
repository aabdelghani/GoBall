# GoBall - Mini Golf Scoring System

A Raspberry Pi 5-based mini golf scoring system with an LVGL touchscreen UI, IR sensor detection, LED strip feedback, and voice announcements. Built with SDL2 backend for cross-compilation from a Linux host to aarch64 Raspberry Pi targets.

## Features

### Game Modes
- **Stroke Play** - 1 to 4 players, 9 or 18 holes
- **Match Play 1v1** - Head-to-head, 9 or 18 holes with early victory detection
- **Match Play 2v2** - Team-based, 9 or 18 holes with early victory detection
- **Quota Points** - 1 to 4 players, 9 or 18 holes with preset par targets counting down to zero
- **Vegas Quota Points** - 1 to 4 players, 9 or 18 holes with bonus scoring after completing par categories

### Scoring
- IR sensors detect ball entry into 4 scoring holes: 3 points, 4 points, 5 points, and 0 points
- Automatic score tracking and cumulative scorecard display
- Match Play uses "Up & Down" format (e.g., "3 & 2") with early victory when lead exceeds remaining holes
- Quota Points: preset par targets (3pt, 4pt, 5pt) count down to zero — first to complete wins
- Vegas Quota Points: same countdown, but bonus points earned when hitting a completed category while opponent hasn't

### Audio
- Voice announcements for player turns, game mode selection, and winner declarations
- 5 randomized winner voice lines per player/team for variety
- Configurable sound delays for announcements and turn switches
- Mute/unmute support

### Visual Feedback
- WS2812 LED strip integration via PIO on Raspberry Pi 5
- Color-coded flash on scoring (green for points, white for zero)
- Player turn highlighting patterns

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
│   ├── led_logic/              # WS2812 LED strip control
│   ├── logic/                  # GPIO event handling, debounce, game flow
│   ├── sound_logic/            # SDL2_mixer audio system
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
├── tools/
│   └── gpio_loopback_simulator.py  # GPIO test simulator
├── scripts/                    # Build and dependency scripts
└── Dockerfile                  # Docker cross-compilation environment
```

## Building

### Prerequisites

- **Host**: Linux with `gcc`, `cmake`, `make`
- **Cross-compiler**: ARM GNU Toolchain (`aarch64-none-linux-gnu-gcc`)
  - [ARM GNU Toolchain 12.3](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) for RPi OS 12 (Bookworm)
  - [ARM GNU Toolchain 10.2](https://developer.arm.com/downloads/-/gnu-a) for RPi OS 11 (Bullseye)
- **Target RPi packages**: `libsdl2-2.0-0`, `libsdl2-mixer-2.0-0`

### Command-Line Build

```bash
# Native cross-compile
./build-cross.sh

# Or with Docker (no local toolchain needed)
./docker-build-and-run.sh
```

### VSCode Build

1. Open the project folder in VSCode
2. Install C/C++ Extension Pack and CMake Tools
3. Select the aarch64 toolchain from the bottom toolbar
4. Click Build

### Deploy to Raspberry Pi

```bash
scp build-cross/build/SquareLine_Project pi@<rpi-ip>:~
# Or use docker output:
scp docker-output/SquareLine_Project pi@<rpi-ip>:~
```

### GPIO Test Simulator

For testing without physical IR sensors, use the loopback simulator on the Pi itself:

```bash
python3 tools/gpio_loopback_simulator.py
```

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

## Changelog

### 02/15/2026
- Vegas Quota Points 3-player mode (9H and 18H) with highlighting, crown, turn announcements, ball counter, and bonus score display
- Vegas Quota Points 3P winner logic: first to complete wins; simultaneous finish compares scores (highest wins, tie shows both crowns)
- Vegas Quota Points 2-player mode (9H and 18H) with highlighting, crown, turn announcements, ball counter (2→1 cycle), and bonus score display
- Vegas Quota Points 1-player 18H mode
- Vegas Quota Points 1-player 9H mode with bonus scoring after completing par categories
- Crowns hidden at game start for Vegas Quota 2P and 3P (shown only on winner)
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
