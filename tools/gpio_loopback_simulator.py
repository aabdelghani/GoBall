#!/usr/bin/env python3
"""
GPIO Loopback Simulator for GoBall Testing (libgpiod v2.x)

WIRING REQUIRED:
    OUTPUT Pin  --->  INPUT Pin (Sensor)
    GPIO 5      --->  GPIO 17 (Sensor 1 - 5 points)
    GPIO 6      --->  GPIO 26 (Sensor 2 - 4 points)
    GPIO 13     --->  GPIO 27 (Sensor 3 - 3 points)
    GPIO 19     --->  GPIO 24 (Sensor 4 - 0 points)

Usage:
    sudo python3 gpio_loopback_simulator.py
"""

import sys
import time
import select
import termios
import tty
from datetime import datetime


def debug(msg):
    """Print debug message with timestamp."""
    ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"[{ts}] [DEBUG] {msg}")


def info(msg):
    """Print info message with timestamp."""
    ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"[{ts}] [INFO] {msg}")


def error(msg):
    """Print error message with timestamp."""
    ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"[{ts}] [ERROR] {msg}")


print("=" * 60)
debug("Starting GPIO Loopback Simulator")
debug(f"Python version: {sys.version}")
print("=" * 60)

# Try to import gpiod
debug("Attempting to import gpiod module...")
try:
    import gpiod
    debug("SUCCESS: gpiod module imported")
    debug(f"gpiod attributes: {dir(gpiod)}")

    if hasattr(gpiod, '__version__'):
        debug(f"gpiod version: {gpiod.__version__}")
    if hasattr(gpiod, 'api_version'):
        debug(f"gpiod api_version: {gpiod.api_version}")

    # Try to import v2.x specific modules
    debug("Attempting to import gpiod.line...")
    from gpiod.line import Direction, Value
    debug("SUCCESS: Direction and Value imported from gpiod.line")
    debug(f"Direction.OUTPUT = {Direction.OUTPUT}")
    debug(f"Value.ACTIVE = {Value.ACTIVE}")
    debug(f"Value.INACTIVE = {Value.INACTIVE}")

    GPIOD_AVAILABLE = True

except ImportError as e:
    error(f"gpiod import error: {e}")
    debug("Install with: sudo apt install python3-libgpiod")
    GPIOD_AVAILABLE = False
except Exception as e:
    error(f"Unexpected error: {e}")
    GPIOD_AVAILABLE = False

# Configuration
LOOPBACK_PINS = {
    '1': {'out': 5,  'in': 17, 'points': 5, 'desc': 'Sensor 1 (5 pts)'},
    '2': {'out': 6,  'in': 26, 'points': 4, 'desc': 'Sensor 2 (4 pts)'},
    '3': {'out': 13, 'in': 27, 'points': 3, 'desc': 'Sensor 3 (3 pts)'},
    '4': {'out': 19, 'in': 24, 'points': 0, 'desc': 'Sensor 4 (0 pts)'},
}

CHIP_PATH = "/dev/gpiochip0"

debug("Configuration:")
debug(f"  CHIP_PATH: {CHIP_PATH}")
debug(f"  LOOPBACK_PINS: {LOOPBACK_PINS}")


class KeyboardInput:
    def __init__(self):
        self.old_settings = None

    def __enter__(self):
        self.old_settings = termios.tcgetattr(sys.stdin)
        tty.setcbreak(sys.stdin.fileno())
        return self

    def __exit__(self, *args):
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, self.old_settings)

    def get_key(self):
        if select.select([sys.stdin], [], [], 0.1)[0]:
            return sys.stdin.read(1)
        return None


class LoopbackSimulator:
    def __init__(self, chip_path=CHIP_PATH):
        self.chip_path = chip_path
        self.request = None
        self.offsets = []
        debug(f"LoopbackSimulator.__init__(chip_path={chip_path})")

    def init(self):
        debug("LoopbackSimulator.init() called")
        debug(f"  GPIOD_AVAILABLE: {GPIOD_AVAILABLE}")

        if not GPIOD_AVAILABLE:
            debug("Running in DRY RUN MODE (no GPIO control)")
            return True

        try:
            # Check if chip exists
            import os
            debug(f"Checking if {self.chip_path} exists...")
            if os.path.exists(self.chip_path):
                debug(f"YES - {self.chip_path} exists")
            else:
                error(f"{self.chip_path} does NOT exist!")
                debug("Available gpio chips:")
                for f in os.listdir('/dev'):
                    if 'gpio' in f:
                        debug(f"  /dev/{f}")
                return False

            # Get all output pin offsets
            self.offsets = [cfg['out'] for cfg in LOOPBACK_PINS.values()]
            debug(f"Output pin offsets: {self.offsets}")

            # Configure lines as outputs with initial HIGH value
            debug("Creating LineSettings for each pin...")
            config = {}
            for offset in self.offsets:
                debug(f"  Creating LineSettings for GPIO {offset}")
                settings = gpiod.LineSettings(
                    direction=Direction.OUTPUT,
                    output_value=Value.ACTIVE
                )
                debug(f"    Settings created: {settings}")
                config[offset] = settings

            debug(f"Config dict: {config}")

            # Request all lines
            debug("Calling gpiod.request_lines()...")
            debug(f"  chip_path: {self.chip_path}")
            debug(f"  consumer: 'loopback_sim'")

            self.request = gpiod.request_lines(
                self.chip_path,
                consumer="loopback_sim",
                config=config
            )

            info("SUCCESS: Lines requested")
            debug(f"Request object: {self.request}")

            return True

        except PermissionError as e:
            error(f"PERMISSION ERROR: {e}")
            debug(f"Try running with: sudo python3 {sys.argv[0]}")
            return False
        except Exception as e:
            error(f"EXCEPTION: {type(e).__name__}: {e}")
            import traceback
            traceback.print_exc()
            self.request = None
            return False

    def trigger_sensor(self, sensor_key):
        if sensor_key not in LOOPBACK_PINS:
            error(f"Invalid sensor key: {sensor_key}")
            return

        cfg = LOOPBACK_PINS[sensor_key]
        out_pin = cfg['out']
        in_pin = cfg['in']

        info(f">>> TRIGGER: {cfg['desc']} <<<")
        debug(f"  sensor_key: {sensor_key}, out_pin: {out_pin}, in_pin: {in_pin}")

        if self.request is not None:
            try:
                debug(f"  Setting GPIO {out_pin} to INACTIVE (LOW)...")
                self.request.set_value(out_pin, Value.INACTIVE)
                debug(f"  Sleeping 50ms...")
                time.sleep(0.05)
                debug(f"  Setting GPIO {out_pin} to ACTIVE (HIGH)...")
                self.request.set_value(out_pin, Value.ACTIVE)
                debug(f"  DONE - Pulse complete")

            except Exception as e:
                error(f"GPIO error: {type(e).__name__}: {e}")
                import traceback
                traceback.print_exc()
        else:
            debug(f"  SIMULATED (no GPIO request)")

    def cleanup(self):
        debug("LoopbackSimulator.cleanup() called")
        if self.request is not None:
            try:
                debug("Releasing GPIO request...")
                self.request.release()
                debug("Released")
            except Exception as e:
                error(f"Error releasing: {e}")


def print_banner():
    print("\n" + "=" * 60)
    print("        GoBall Loopback GPIO Simulator (v2.x)")
    print("=" * 60)
    print("\nWIRING REQUIRED (connect with jumper wires):")
    print("    OUTPUT    INPUT (Sensor)")
    print("    ------    --------------")
    for key, cfg in LOOPBACK_PINS.items():
        print(f"    GPIO {cfg['out']:2}  ->  GPIO {cfg['in']:2}  ({cfg['desc']})")
    print("\nControls:")
    print("    1-4  Trigger individual sensors")
    print("    a    Auto sequence (all sensors)")
    print("    s    Score round (all 4 sensors)")
    print("    h    Complete a hole (9 triggers)")
    print("    l    LOOP FOREVER (press any key to stop)")
    print("    q    Quit")
    print("=" * 60 + "\n")


def main():
    print_banner()

    debug("Creating LoopbackSimulator instance...")
    sim = LoopbackSimulator()

    debug("Initializing simulator...")
    if not sim.init():
        error("Failed to initialize GPIO. Check debug output above.")
        error("Make sure you're running with sudo and wiring is correct.")
        input("Press Enter to exit...")
        return

    print("\n" + "=" * 60)
    info("READY! Press keys to trigger sensors (q to quit)")
    print("=" * 60 + "\n")

    try:
        with KeyboardInput() as kb:
            while True:
                key = kb.get_key()
                if key is None:
                    continue

                if key == 'q':
                    debug("Quit requested")
                    break

                elif key in '1234':
                    sim.trigger_sensor(key)

                elif key == 'a':
                    info("--- All sensors sequence ---")
                    for k in '1234':
                        sim.trigger_sensor(k)
                        time.sleep(0.5)

                elif key == 's':
                    info("--- Scoring round ---")
                    for k in '1234':
                        sim.trigger_sensor(k)
                        time.sleep(0.3)

                elif key == 'h':
                    info("--- Completing a hole (9 triggers) ---")
                    for i in range(9):
                        debug(f"Trigger {i+1}/9")
                        sim.trigger_sensor('1')
                        time.sleep(0.2)

                elif key == 'l':
                    print("\n" + "=" * 60)
                    info("LOOP MODE - Press any key to stop")
                    print("=" * 60)
                    loop_count = 0
                    while True:
                        loop_count += 1
                        info(f"--- Loop #{loop_count} ---")
                        for k in '1234':
                            sim.trigger_sensor(k)
                            time.sleep(1.0)
                            # Check for key press to stop
                            if select.select([sys.stdin], [], [], 0)[0]:
                                sys.stdin.read(1)  # consume the key
                                debug("Loop stopped by user")
                                break
                        else:
                            time.sleep(1.0)  # delay between loops
                            continue
                        break  # exit outer while loop
                    info("Loop mode ended")

    except KeyboardInterrupt:
        debug("Keyboard interrupt")
    finally:
        sim.cleanup()
        debug("Exiting")


if __name__ == "__main__":
    main()
