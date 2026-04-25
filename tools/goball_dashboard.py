#!/usr/bin/env python3
"""
GoBall Development Dashboard
=============================
5-tab tkinter application for deploying, testing, and managing the GoBall project.

Tabs:
  1. Deploy & Run   — Build, SCP deploy, start/stop/restart app on Pi
  2. GPIO Simulator  — Trigger IR sensors via SSH + gpiod on Pi
  3. Test Harness    — Automated game scenario testing
  4. Log Analyzer    — Live tail & filtered view of app logs
  5. Config Editor   — Edit sound delays, debug levels, view source files

Usage:
    python3 tools/goball_dashboard.py
"""

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox, filedialog
import subprocess
import threading
import os
import re
import time
from datetime import datetime

# ============================================================================
# Configuration
# ============================================================================

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_DIR = "build-master"

# Default connection values (editable in the Deploy tab)
DEFAULT_PI_USER = "q"
DEFAULT_PI_IP   = "192.168.50.93"
DEFAULT_SSH_KEY = os.path.expanduser("~/.ssh/id_ed25519")
DEFAULT_DEPLOY_PATH = "/home/q/Desktop/SquareLine_Project/"
DEFAULT_BINARY  = os.path.join(PROJECT_ROOT, BUILD_DIR, "SquareLine_Project")
DEFAULT_WAYLAND_DISPLAY = "wayland-0"

# GPIO loopback wiring: output pin pulses LOW to trigger the input sensor
GPIO_SENSORS = {
    1: {"out": 5,  "in": 17, "points": 5, "desc": "Sensor 1 (5 pts)"},
    2: {"out": 6,  "in": 26, "points": 4, "desc": "Sensor 2 (4 pts)"},
    3: {"out": 13, "in": 27, "points": 3, "desc": "Sensor 3 (3 pts)"},
    4: {"out": 19, "in": 24, "points": 0, "desc": "Sensor 4 (0 pts)"},
}

DEBUG_MODULES = [
    "MAIN", "LVGL", "UI", "GAME", "SOUND",
    "LED", "GPIO", "INPUT", "ANIMATION", "LOGIC", "HAL",
]
DEBUG_LEVELS = ["NONE", "ERROR", "WARN", "INFO", "DEBUG", "TRACE"]

SOUND_DELAYS = {
    "SOUND_DELAY_PLAYER_ANNOUNCE_MS": {"default": 750, "desc": "Player announcement delay"},
    "SOUND_DELAY_TEAM_ANNOUNCE_MS":   {"default": 750, "desc": "Team announcement delay"},
    "SOUND_DELAY_PLAYER_WINS_MS":     {"default": 750, "desc": "Player wins announcement delay"},
    "SOUND_DELAY_TEAM_WINS_MS":       {"default": 750, "desc": "Team wins announcement delay"},
    "SOUND_DELAY_TURN_SWITCH_MS":     {"default": 500, "desc": "Turn switch delay"},
}


# ============================================================================
# Shared connection config (updated by Deploy tab, read by all tabs)
# ============================================================================

class PiConfig:
    """Shared Pi connection settings, updated from the Deploy tab UI."""
    def __init__(self):
        self.user = DEFAULT_PI_USER
        self.ip   = DEFAULT_PI_IP
        self.ssh_key = DEFAULT_SSH_KEY
        self.deploy_path = DEFAULT_DEPLOY_PATH
        self.binary = DEFAULT_BINARY
        self.display = DEFAULT_WAYLAND_DISPLAY
        self.extra_files = []  # list of local paths to also deploy

    @property
    def host(self):
        return f"{self.user}@{self.ip}"

    @property
    def app_name(self):
        return os.path.basename(self.binary)

pi_cfg = PiConfig()


# ============================================================================
# SSH helpers (use pi_cfg for connection details)
# ============================================================================

def ssh_cmd(cmd, timeout=30):
    """Run a command on the Pi via SSH and return CompletedProcess."""
    full = [
        "ssh", "-i", pi_cfg.ssh_key,
        "-o", "StrictHostKeyChecking=no",
        "-o", "ConnectTimeout=5",
        pi_cfg.host, cmd,
    ]
    return subprocess.run(full, capture_output=True, text=True, timeout=timeout)


def scp_to_pi(local_path, remote_path):
    """SCP a file or directory to the Pi. Uses -r for directories."""
    cmd = ["scp", "-i", pi_cfg.ssh_key]
    if os.path.isdir(local_path):
        cmd.append("-r")
    cmd += [local_path, f"{pi_cfg.host}:{remote_path}"]
    return subprocess.run(cmd, capture_output=True, text=True, timeout=120)


def ts():
    """Short timestamp for log lines."""
    return datetime.now().strftime("%H:%M:%S")


# ============================================================================
# Tab 1 — Deploy & Run Dashboard
# ============================================================================

class DeployTab(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self._build_ui()

    def _build_ui(self):
        # --- Pi Connection ---
        cf = ttk.LabelFrame(self, text="Pi Connection", padding=8)
        cf.pack(fill=tk.X, padx=10, pady=4)

        r1 = ttk.Frame(cf); r1.pack(fill=tk.X, pady=2)
        ttk.Label(r1, text="Pi Name/User:").pack(side=tk.LEFT, padx=5)
        self.user_var = tk.StringVar(value=DEFAULT_PI_USER)
        ttk.Entry(r1, textvariable=self.user_var, width=12).pack(side=tk.LEFT, padx=2)

        ttk.Label(r1, text="IP:").pack(side=tk.LEFT, padx=(15, 5))
        self.ip_var = tk.StringVar(value=DEFAULT_PI_IP)
        ttk.Entry(r1, textvariable=self.ip_var, width=16).pack(side=tk.LEFT, padx=2)

        ttk.Label(r1, text="SSH Key:").pack(side=tk.LEFT, padx=(15, 5))
        self.key_var = tk.StringVar(value=DEFAULT_SSH_KEY)
        ttk.Entry(r1, textvariable=self.key_var, width=30).pack(side=tk.LEFT, padx=2)
        ttk.Button(r1, text="...", command=self._browse_key, width=3).pack(side=tk.LEFT, padx=2)

        r2 = ttk.Frame(cf); r2.pack(fill=tk.X, pady=2)
        ttk.Label(r2, text="Deploy Path:").pack(side=tk.LEFT, padx=5)
        self.deploy_path_var = tk.StringVar(value=DEFAULT_DEPLOY_PATH)
        ttk.Entry(r2, textvariable=self.deploy_path_var, width=40).pack(side=tk.LEFT, padx=2)

        ttk.Label(r2, text="Display:").pack(side=tk.LEFT, padx=(15, 5))
        self.display_var = tk.StringVar(value=DEFAULT_WAYLAND_DISPLAY)
        display_opts = ["wayland-0", "wayland-1", "wayland-2", ":0", ":1"]
        ttk.Combobox(r2, textvariable=self.display_var, values=display_opts, width=12).pack(side=tk.LEFT, padx=2)

        ttk.Button(r2, text="Apply", command=self._apply_config, width=8).pack(side=tk.RIGHT, padx=5)

        # --- Binary selection ---
        bf = ttk.LabelFrame(self, text="Binary to Deploy", padding=8)
        bf.pack(fill=tk.X, padx=10, pady=4)

        br = ttk.Frame(bf); br.pack(fill=tk.X)
        self.binary_var = tk.StringVar(value=DEFAULT_BINARY)
        ttk.Entry(br, textvariable=self.binary_var, width=70).pack(side=tk.LEFT, padx=5, fill=tk.X, expand=True)
        ttk.Button(br, text="Browse", command=self._browse_binary, width=8).pack(side=tk.LEFT, padx=2)

        # --- Extra files/modules to deploy ---
        ef = ttk.LabelFrame(self, text="Extra Files / Modules to Deploy", padding=8)
        ef.pack(fill=tk.X, padx=10, pady=4)

        er = ttk.Frame(ef); er.pack(fill=tk.X)
        self.extras_listbox = tk.Listbox(er, height=4, font=("Monospace", 9),
                                         bg="#1e1e1e", fg="#d4d4d4", selectmode=tk.EXTENDED)
        self.extras_listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=5)

        ebtn = ttk.Frame(er); ebtn.pack(side=tk.LEFT, padx=5)
        ttk.Button(ebtn, text="Add File",   command=self._add_file,   width=10).pack(pady=2)
        ttk.Button(ebtn, text="Add Folder", command=self._add_folder, width=10).pack(pady=2)
        ttk.Button(ebtn, text="Remove",     command=self._remove_extra, width=10).pack(pady=2)
        ttk.Button(ebtn, text="Clear All",  command=self._clear_extras, width=10).pack(pady=2)

        # --- Status bar ---
        sf = ttk.LabelFrame(self, text="Status", padding=6)
        sf.pack(fill=tk.X, padx=10, pady=4)

        self.pi_status  = tk.StringVar(value="Unknown")
        self.app_status = tk.StringVar(value="Unknown")
        self.build_status = tk.StringVar(value="Ready")

        srow = ttk.Frame(sf)
        srow.pack(fill=tk.X)
        ttk.Label(srow, text="Pi:").pack(side=tk.LEFT, padx=(5, 2))
        self.pi_lbl = ttk.Label(srow, textvariable=self.pi_status, foreground="gray")
        self.pi_lbl.pack(side=tk.LEFT, padx=(0, 15))
        ttk.Label(srow, text="App:").pack(side=tk.LEFT, padx=(0, 2))
        self.app_lbl = ttk.Label(srow, textvariable=self.app_status, foreground="gray")
        self.app_lbl.pack(side=tk.LEFT, padx=(0, 15))
        ttk.Label(srow, text="Build:").pack(side=tk.LEFT, padx=(0, 2))
        ttk.Label(srow, textvariable=self.build_status).pack(side=tk.LEFT)
        ttk.Button(srow, text="Refresh", command=self._refresh_status).pack(side=tk.RIGHT, padx=5)

        # --- Action buttons ---
        af = ttk.LabelFrame(self, text="Actions", padding=8)
        af.pack(fill=tk.X, padx=10, pady=4)

        a1 = ttk.Frame(af); a1.pack(fill=tk.X)
        for txt, cmd in [
            ("Build",        self._build),
            ("Deploy",       self._deploy),
            ("Build+Deploy", self._build_deploy),
            ("Start App",    self._start_app),
            ("Stop App",     self._stop_app),
            ("Kill App",     self._kill_app),
            ("Restart App",  self._restart_app),
        ]:
            ttk.Button(a1, text=txt, command=cmd, width=13).pack(side=tk.LEFT, padx=3, pady=2)

        a2 = ttk.Frame(af); a2.pack(fill=tk.X, pady=(4, 0))
        ttk.Button(a2, text="Build + Deploy + Run", command=self._full_pipeline, width=22).pack(side=tk.LEFT, padx=4)
        ttk.Button(a2, text="Reboot Pi", command=self._reboot_pi, width=12).pack(side=tk.RIGHT, padx=4)

        # --- Output console ---
        of = ttk.LabelFrame(self, text="Output", padding=5)
        of.pack(fill=tk.BOTH, expand=True, padx=10, pady=4)
        self.out = scrolledtext.ScrolledText(
            of, height=12, font=("Monospace", 9),
            bg="#1e1e1e", fg="#d4d4d4", insertbackground="white",
        )
        self.out.pack(fill=tk.BOTH, expand=True)
        ttk.Button(of, text="Clear", command=lambda: self.out.delete(1.0, tk.END)).pack(anchor=tk.E, pady=2)

        # Apply config on init
        self._apply_config()

    # -- UI callbacks for connection / file selection --

    def _apply_config(self):
        """Push UI values into the shared pi_cfg."""
        pi_cfg.user = self.user_var.get().strip()
        pi_cfg.ip   = self.ip_var.get().strip()
        pi_cfg.ssh_key = self.key_var.get().strip()
        pi_cfg.deploy_path = self.deploy_path_var.get().strip()
        pi_cfg.binary = self.binary_var.get().strip()
        pi_cfg.display = self.display_var.get().strip()
        pi_cfg.extra_files = list(self.extras_listbox.get(0, tk.END))
        self._log(f"Config applied: {pi_cfg.host} -> {pi_cfg.deploy_path}")

    def _browse_key(self):
        path = filedialog.askopenfilename(
            title="Select SSH Private Key",
            initialdir=os.path.expanduser("~/.ssh"),
        )
        if path:
            self.key_var.set(path)

    def _browse_binary(self):
        path = filedialog.askopenfilename(
            title="Select Binary to Deploy",
            initialdir=os.path.join(PROJECT_ROOT, BUILD_DIR),
        )
        if path:
            self.binary_var.set(path)

    def _add_file(self):
        paths = filedialog.askopenfilenames(
            title="Select File(s) to Deploy",
            initialdir=PROJECT_ROOT,
        )
        for p in paths:
            if p and p not in self.extras_listbox.get(0, tk.END):
                self.extras_listbox.insert(tk.END, p)

    def _add_folder(self):
        path = filedialog.askdirectory(
            title="Select Folder to Deploy",
            initialdir=os.path.join(PROJECT_ROOT, "modules"),
        )
        if path and path not in self.extras_listbox.get(0, tk.END):
            self.extras_listbox.insert(tk.END, path)

    def _remove_extra(self):
        for idx in reversed(self.extras_listbox.curselection()):
            self.extras_listbox.delete(idx)

    def _clear_extras(self):
        self.extras_listbox.delete(0, tk.END)

    # -- helpers --
    def _log(self, msg):
        def do_log():
            self.out.insert(tk.END, f"[{ts()}] {msg}\n")
            self.out.see(tk.END)
        self.after(0, do_log)

    def _bg(self, fn):
        threading.Thread(target=fn, daemon=True).start()

    # -- actions --
    def _refresh_status(self):
        self._apply_config()
        def do():
            try:
                r = ssh_cmd("echo ok", timeout=5)
                ok = r.returncode == 0
            except Exception:
                ok = False

            def update_pi():
                self.pi_status.set("Connected" if ok else "Disconnected")
                self.pi_lbl.config(foreground="green" if ok else "red")
            self.after(0, update_pi)

            if ok:
                try:
                    r = ssh_cmd(f"pgrep -f {pi_cfg.app_name}", timeout=5)
                    running = r.returncode == 0
                except Exception:
                    running = False
                def update_app():
                    self.app_status.set("Running" if running else "Stopped")
                    self.app_lbl.config(foreground="green" if running else "orange")
                self.after(0, update_app)
            else:
                self.after(0, lambda: self.app_status.set("Unknown"))
                self.after(0, lambda: self.app_lbl.config(foreground="gray"))
        self._bg(do)

    def _do_build(self):
        """Build locally. Returns True on success."""
        self.build_status.set("Building...")
        self._log("Starting build...")
        nproc = os.cpu_count() or 4
        try:
            r = subprocess.run(
                ["make", "-C", os.path.join(PROJECT_ROOT, BUILD_DIR), f"-j{nproc}"],
                capture_output=True, text=True, timeout=300,
            )
            if r.stdout:
                for line in r.stdout.strip().split("\n")[-15:]:
                    self._log(line)
            if r.returncode == 0:
                self.build_status.set("Success")
                self._log("Build successful!")
                return True
            else:
                self.build_status.set("Failed")
                self._log("Build FAILED!")
                if r.stderr:
                    for line in r.stderr.strip().split("\n")[-20:]:
                        self._log(f"  {line}")
                return False
        except Exception as e:
            self.build_status.set("Error")
            self._log(f"Build error: {e}")
            return False

    def _do_deploy(self):
        """Kill remote app, SCP binary + extras. Returns True on success."""
        self._apply_config()
        self._log("Stopping app on Pi...")
        try:
            ssh_cmd(f"pkill -f {pi_cfg.app_name} 2>/dev/null; sleep 1", timeout=10)
        except Exception:
            pass

        # Deploy main binary
        self._log(f"Deploying binary: {os.path.basename(pi_cfg.binary)}")
        try:
            r = scp_to_pi(pi_cfg.binary, pi_cfg.deploy_path)
            if r.returncode != 0:
                self._log(f"Deploy FAILED: {r.stderr.strip()}")
                return False
            self._log("Binary deployed OK.")
        except Exception as e:
            self._log(f"Deploy error: {e}")
            return False

        # Deploy extra files/folders
        for extra in pi_cfg.extra_files:
            name = os.path.basename(extra.rstrip("/"))
            is_dir = os.path.isdir(extra)
            self._log(f"Deploying {'folder' if is_dir else 'file'}: {name}")
            try:
                r = scp_to_pi(extra, pi_cfg.deploy_path)
                if r.returncode != 0:
                    self._log(f"  FAILED: {r.stderr.strip()}")
                else:
                    self._log(f"  {name} deployed OK.")
            except Exception as e:
                self._log(f"  ERROR: {e}")

        self._log("Deploy complete!")
        return True

    def _do_start(self):
        """Start app on Pi with Wayland display env vars."""
        self._apply_config()
        self._log("Starting app on Pi...")
        # Set display env vars based on Wayland vs X11
        disp = pi_cfg.display
        if disp.startswith(":"):
            env = f"export DISPLAY={disp}"
        else:
            env = f"export XDG_RUNTIME_DIR=/run/user/$(id -u) WAYLAND_DISPLAY={disp}"
        # Wrap in bash -c with disown so SSH shell exits immediately
        remote = (
            f"{env} && "
            f"cd {pi_cfg.deploy_path} && "
            f"nohup setsid ./{pi_cfg.app_name} > /tmp/goball.log 2>&1 </dev/null &"
            f" disown; exit 0"
        )
        try:
            ssh_cmd(f"bash -c '{remote}'", timeout=5)
        except subprocess.TimeoutExpired:
            self._log("(SSH slow to disconnect — app may still be starting)")
        time.sleep(2)
        self._refresh_status()
        self._log("Start command sent.")

    def _build(self):
        self._bg(self._do_build)

    def _deploy(self):
        self._bg(self._do_deploy)

    def _build_deploy(self):
        def do():
            if self._do_build():
                self._do_deploy()
        self._bg(do)

    def _start_app(self):
        self._bg(self._do_start)

    def _stop_app(self):
        def do():
            self._apply_config()
            self._log("Stopping app (SIGTERM)...")
            ssh_cmd(f"pkill -f {pi_cfg.app_name}")
            time.sleep(1)
            self._refresh_status()
            self._log("Stop command sent.")
        self._bg(do)

    def _kill_app(self):
        def do():
            self._apply_config()
            self._log("Killing app (SIGKILL)...")
            ssh_cmd(f"pkill -9 -f {pi_cfg.app_name}")
            time.sleep(1)
            self._refresh_status()
            self._log("Kill command sent.")
        self._bg(do)

    def _restart_app(self):
        def do():
            self._log("Restarting app...")
            self._apply_config()
            ssh_cmd(f"pkill -f {pi_cfg.app_name}")
            time.sleep(2)
            self._do_start()
        self._bg(do)

    def _full_pipeline(self):
        def do():
            self._apply_config()
            self._log("=== FULL PIPELINE ===")
            self._log("Step 1/3: Build")
            if not self._do_build():
                return
            self._log("Step 2/3: Deploy")
            if not self._do_deploy():
                return
            self._log("Step 3/3: Run")
            self._do_start()
            self._log("=== PIPELINE COMPLETE ===")
        self._bg(do)

    def _reboot_pi(self):
        if messagebox.askyesno("Confirm", "Reboot the Raspberry Pi?"):
            def do():
                self._apply_config()
                self._log("Rebooting Pi...")
                ssh_cmd("sudo reboot")
                self._log("Reboot command sent. Wait ~30s.")
            self._bg(do)


# ============================================================================
# Tab 2 — GPIO Simulator
# ============================================================================

def _gpio_trigger_cmd(sensor_id):
    """Build a python one-liner that pulses a GPIO output pin via gpiod v2."""
    cfg = GPIO_SENSORS[sensor_id]
    out = cfg["out"]
    return (
        f"sudo python3 -c \""
        f"import gpiod, time; "
        f"from gpiod.line import Direction, Value; "
        f"req = gpiod.request_lines('/dev/gpiochip0', consumer='dash', "
        f"config={{{out}: gpiod.LineSettings(direction=Direction.OUTPUT, output_value=Value.ACTIVE)}}); "
        f"req.set_value({out}, Value.INACTIVE); time.sleep(0.05); "
        f"req.set_value({out}, Value.ACTIVE); req.release()\""
    )


class GPIOTab(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.running = False
        self._build_ui()

    def _build_ui(self):
        # --- Sensor buttons ---
        sf = ttk.LabelFrame(self, text="Trigger Sensors", padding=10)
        sf.pack(fill=tk.X, padx=10, pady=5)

        bf = ttk.Frame(sf); bf.pack()
        colors = {5: "#4CAF50", 4: "#2196F3", 3: "#FF9800", 0: "#f44336"}

        for sid, cfg in GPIO_SENSORS.items():
            btn = tk.Button(
                bf,
                text=f"Sensor {sid}\n{cfg['points']} pts\nGPIO {cfg['out']}\u2192{cfg['in']}",
                width=16, height=4,
                font=("Helvetica", 11, "bold"),
                bg=colors[cfg["points"]], fg="white",
                activebackground=colors[cfg["points"]],
                command=lambda s=sid: self._trigger(s),
            )
            btn.pack(side=tk.LEFT, padx=8, pady=5)

        # --- Sequences ---
        qf = ttk.LabelFrame(self, text="Sequences", padding=10)
        qf.pack(fill=tk.X, padx=10, pady=5)

        r1 = ttk.Frame(qf); r1.pack(fill=tk.X)
        ttk.Button(r1, text="All Sensors",      command=lambda: self._seq([1,2,3,4], 0.5), width=14).pack(side=tk.LEFT, padx=4)
        ttk.Button(r1, text="Score Round",       command=lambda: self._seq([1,2,3,4], 0.3), width=14).pack(side=tk.LEFT, padx=4)
        ttk.Button(r1, text="Complete Hole (9)", command=lambda: self._seq([1]*9, 0.2),     width=16).pack(side=tk.LEFT, padx=4)
        ttk.Button(r1, text="Full Game 18Hx4P",  command=self._full_game,                   width=16).pack(side=tk.LEFT, padx=4)
        self.stop_btn = ttk.Button(r1, text="Stop", command=self._stop, width=8, state=tk.DISABLED)
        self.stop_btn.pack(side=tk.RIGHT, padx=4)

        r2 = ttk.Frame(qf); r2.pack(fill=tk.X, pady=(5, 0))
        ttk.Label(r2, text="Delay (ms):").pack(side=tk.LEFT, padx=5)
        self.delay_var = tk.StringVar(value="500")
        ttk.Entry(r2, textvariable=self.delay_var, width=8).pack(side=tk.LEFT, padx=5)

        # --- Custom sequence ---
        cf = ttk.LabelFrame(self, text="Custom Sequence (space-separated sensor #s)", padding=10)
        cf.pack(fill=tk.X, padx=10, pady=5)

        cr = ttk.Frame(cf); cr.pack(fill=tk.X)
        self.custom_var = tk.StringVar(value="1 2 3 4")
        ttk.Entry(cr, textvariable=self.custom_var, width=40).pack(side=tk.LEFT, padx=5)
        ttk.Button(cr, text="Run", command=self._run_custom).pack(side=tk.LEFT, padx=5)

        # --- Log ---
        lf = ttk.LabelFrame(self, text="GPIO Log", padding=5)
        lf.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        self.out = scrolledtext.ScrolledText(
            lf, height=12, font=("Monospace", 9), bg="#1e1e1e", fg="#d4d4d4",
        )
        self.out.pack(fill=tk.BOTH, expand=True)
        ttk.Button(lf, text="Clear", command=lambda: self.out.delete(1.0, tk.END)).pack(anchor=tk.E, pady=2)

    def _log(self, msg):
        self.out.insert(tk.END, f"[{ts()}] {msg}\n")
        self.out.see(tk.END)

    def _trigger(self, sid):
        """Trigger a single sensor (threaded)."""
        def do():
            cfg = GPIO_SENSORS[sid]
            self._log(f"Trigger: {cfg['desc']}")
            try:
                r = ssh_cmd(_gpio_trigger_cmd(sid), timeout=10)
                if r.returncode == 0:
                    self._log(f"  -> OK")
                else:
                    self._log(f"  -> ERROR: {r.stderr.strip()}")
            except Exception as e:
                self._log(f"  -> ERROR: {e}")
        threading.Thread(target=do, daemon=True).start()

    def _trigger_sync(self, sid):
        """Trigger a sensor synchronously (for use inside sequence threads)."""
        cfg = GPIO_SENSORS[sid]
        self.out.after(0, self._log, f"  Trigger: {cfg['desc']}")
        try:
            r = ssh_cmd(_gpio_trigger_cmd(sid), timeout=10)
            if r.returncode != 0:
                self.out.after(0, self._log, f"    ERROR: {r.stderr.strip()}")
        except Exception as e:
            self.out.after(0, self._log, f"    ERROR: {e}")

    def _seq(self, sensors, delay):
        def do():
            self.running = True
            self.stop_btn.config(state=tk.NORMAL)
            self.out.after(0, self._log, f"Sequence: {sensors} delay={delay}s")
            for i, s in enumerate(sensors):
                if not self.running:
                    self.out.after(0, self._log, "Sequence stopped.")
                    break
                self._trigger_sync(s)
                if i < len(sensors) - 1:
                    time.sleep(delay)
            self.running = False
            self.stop_btn.config(state=tk.DISABLED)
        threading.Thread(target=do, daemon=True).start()

    def _stop(self):
        self.running = False

    def _run_custom(self):
        try:
            sensors = [int(x) for x in self.custom_var.get().split() if x.strip()]
            if not all(1 <= s <= 4 for s in sensors):
                raise ValueError
            delay = int(self.delay_var.get()) / 1000.0
            self._seq(sensors, delay)
        except ValueError:
            messagebox.showerror("Error", "Enter space-separated sensor numbers 1-4.")

    def _full_game(self):
        def do():
            self.running = True
            self.stop_btn.config(state=tk.NORMAL)
            delay = int(self.delay_var.get()) / 1000.0
            self.out.after(0, self._log, "=== Full Game: 18 holes x 4 players ===")
            for hole in range(1, 19):
                if not self.running:
                    break
                self.out.after(0, self._log, f"--- Hole {hole}/18 ---")
                for player in range(4):
                    if not self.running:
                        break
                    sensor = (hole + player) % 4 + 1
                    self._trigger_sync(sensor)
                    time.sleep(delay)
            self.running = False
            self.stop_btn.config(state=tk.DISABLED)
            self.out.after(0, self._log, "Full game complete.")
        threading.Thread(target=do, daemon=True).start()


# ============================================================================
# Tab 3 — Test Harness
# ============================================================================

class TestHarnessTab(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.running = False
        self._build_ui()

    def _build_ui(self):
        # --- Config ---
        cf = ttk.LabelFrame(self, text="Game Configuration", padding=10)
        cf.pack(fill=tk.X, padx=10, pady=5)

        r1 = ttk.Frame(cf); r1.pack(fill=tk.X, pady=2)
        ttk.Label(r1, text="Mode:").pack(side=tk.LEFT, padx=5)
        self.mode_var = tk.StringVar(value="Stroke Play")
        modes = ["Stroke Play", "Match Play 1v1", "Match Play 2v2", "Quota Points", "Vegas Quota"]
        ttk.Combobox(r1, textvariable=self.mode_var, values=modes, width=20, state="readonly").pack(side=tk.LEFT, padx=5)

        ttk.Label(r1, text="Players:").pack(side=tk.LEFT, padx=5)
        self.players_var = tk.StringVar(value="4")
        ttk.Combobox(r1, textvariable=self.players_var, values=["1","2","3","4"], width=5, state="readonly").pack(side=tk.LEFT, padx=5)

        ttk.Label(r1, text="Holes:").pack(side=tk.LEFT, padx=5)
        self.holes_var = tk.StringVar(value="9")
        ttk.Combobox(r1, textvariable=self.holes_var, values=["9","18"], width=5, state="readonly").pack(side=tk.LEFT, padx=5)

        ttk.Label(r1, text="Delay (ms):").pack(side=tk.LEFT, padx=5)
        self.delay_var = tk.StringVar(value="500")
        ttk.Entry(r1, textvariable=self.delay_var, width=8).pack(side=tk.LEFT, padx=5)

        # --- Player names (informational) ---
        nf = ttk.LabelFrame(self, text="Player Names (info only — set in-app)", padding=10)
        nf.pack(fill=tk.X, padx=10, pady=5)

        self.name_vars = []
        nr = ttk.Frame(nf); nr.pack(fill=tk.X)
        for i in range(4):
            ttk.Label(nr, text=f"P{i+1}:").pack(side=tk.LEFT, padx=(10, 2))
            var = tk.StringVar(value=f"Player {i+1}")
            self.name_vars.append(var)
            ttk.Entry(nr, textvariable=var, width=14).pack(side=tk.LEFT, padx=(0, 5))

        # --- Scenarios ---
        sf = ttk.LabelFrame(self, text="Test Scenarios", padding=10)
        sf.pack(fill=tk.X, padx=10, pady=5)

        sr = ttk.Frame(sf); sr.pack(fill=tk.X)
        scenarios = [
            ("Quick Round\n(1 hole)",       self._quick_round),
            ("Half Game",                   self._half_game),
            ("Full Game",                   self._full_game),
            ("Stress Test\n(rapid fire)",   self._stress_test),
            ("All Zeros\n(edge case)",      self._all_zeros),
            ("All Fives\n(best case)",      self._all_fives),
        ]
        for txt, cmd in scenarios:
            ttk.Button(sr, text=txt, command=cmd, width=14).pack(side=tk.LEFT, padx=4, pady=2)

        self.stop_btn = ttk.Button(sr, text="Stop", command=self._stop, width=8, state=tk.DISABLED)
        self.stop_btn.pack(side=tk.RIGHT, padx=4)

        # --- Win scenarios ---
        sr2 = ttk.Frame(sf); sr2.pack(fill=tk.X, pady=(4, 0))
        for p in range(4):
            ttk.Button(sr2, text=f"P{p+1} Wins", command=lambda w=p: self._player_wins(w), width=10).pack(side=tk.LEFT, padx=4, pady=2)

        # --- Output ---
        of = ttk.LabelFrame(self, text="Test Output", padding=5)
        of.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        self.out = scrolledtext.ScrolledText(
            of, height=15, font=("Monospace", 9), bg="#1e1e1e", fg="#d4d4d4",
        )
        self.out.pack(fill=tk.BOTH, expand=True)
        ttk.Button(of, text="Clear", command=lambda: self.out.delete(1.0, tk.END)).pack(anchor=tk.E, pady=2)

    def _log(self, msg):
        self.out.insert(tk.END, f"[{ts()}] {msg}\n")
        self.out.see(tk.END)

    def _trigger(self, sid):
        cfg = GPIO_SENSORS[sid]
        try:
            r = ssh_cmd(_gpio_trigger_cmd(sid), timeout=10)
            return r.returncode == 0
        except Exception:
            return False

    def _stop(self):
        self.running = False

    def _run_scenario(self, name, hole_count, sensor_fn):
        """Generic scenario runner. sensor_fn(hole, player) -> sensor_id."""
        def do():
            self.running = True
            self.stop_btn.config(state=tk.NORMAL)
            players = int(self.players_var.get())
            delay = int(self.delay_var.get()) / 1000.0

            self.out.after(0, self._log, f"=== {name}: {players}P x {hole_count}H ===")

            for h in range(1, hole_count + 1):
                if not self.running:
                    break
                self.out.after(0, self._log, f"-- Hole {h}/{hole_count} --")
                for p in range(players):
                    if not self.running:
                        break
                    sid = sensor_fn(h, p)
                    pname = self.name_vars[p].get() if p < 4 else f"Player {p+1}"
                    ok = self._trigger(sid)
                    status = "OK" if ok else "FAIL"
                    pts = GPIO_SENSORS[sid]["points"]
                    self.out.after(0, self._log, f"  {pname}: sensor {sid} ({pts}pts) [{status}]")
                    time.sleep(delay)

            self.running = False
            self.stop_btn.config(state=tk.DISABLED)
            self.out.after(0, self._log, f"=== {name} complete ===")

        threading.Thread(target=do, daemon=True).start()

    def _quick_round(self):
        self._run_scenario("Quick Round", 1, lambda h, p: (p % 4) + 1)

    def _half_game(self):
        total = int(self.holes_var.get()) // 2
        self._run_scenario("Half Game", max(total, 1), lambda h, p: ((h + p) % 4) + 1)

    def _full_game(self):
        total = int(self.holes_var.get())
        self._run_scenario("Full Game", total, lambda h, p: ((h + p) % 4) + 1)

    def _stress_test(self):
        def do():
            self.running = True
            self.stop_btn.config(state=tk.NORMAL)
            self.out.after(0, self._log, "=== Stress Test: 20 rapid triggers ===")
            for i in range(20):
                if not self.running:
                    break
                sid = (i % 4) + 1
                ok = self._trigger(sid)
                self.out.after(0, self._log, f"  #{i+1}: sensor {sid} {'OK' if ok else 'FAIL'}")
                time.sleep(0.15)
            self.running = False
            self.stop_btn.config(state=tk.DISABLED)
            self.out.after(0, self._log, "=== Stress test complete ===")
        threading.Thread(target=do, daemon=True).start()

    def _all_zeros(self):
        total = int(self.holes_var.get())
        self._run_scenario("All Zeros", total, lambda h, p: 4)  # sensor 4 = 0pts

    def _all_fives(self):
        total = int(self.holes_var.get())
        self._run_scenario("All Fives", total, lambda h, p: 1)  # sensor 1 = 5pts

    def _player_wins(self, winner):
        """Winner gets 4-5pt shots, others get 0-3pt shots."""
        total = int(self.holes_var.get())
        # Sensor mapping: 1=5pts, 2=4pts, 3=3pts, 4=0pts
        # Winner alternates between sensor 1 (5pts) and sensor 2 (4pts)
        # Losers alternate between sensor 4 (0pts) and sensor 3 (3pts)
        def pick(h, p):
            if p == winner:
                return 1 if h % 2 == 1 else 2  # 5pts / 4pts
            else:
                return 4 if h % 2 == 1 else 3  # 0pts / 3pts
        pname = self.name_vars[winner].get() if winner < 4 else f"Player {winner+1}"
        self._run_scenario(f"{pname} Wins", total, pick)


# ============================================================================
# Tab 4 — Log Analyzer
# ============================================================================

class LogTab(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.tail_proc = None
        self._build_ui()

    def _build_ui(self):
        # --- Controls ---
        cf = ttk.LabelFrame(self, text="Controls", padding=10)
        cf.pack(fill=tk.X, padx=10, pady=5)

        r1 = ttk.Frame(cf); r1.pack(fill=tk.X)
        ttk.Button(r1, text="Start Live Tail", command=self._start_tail, width=14).pack(side=tk.LEFT, padx=4)
        ttk.Button(r1, text="Stop Tail",       command=self._stop_tail,  width=10).pack(side=tk.LEFT, padx=4)
        ttk.Button(r1, text="Fetch Last 200",  command=self._fetch,      width=14).pack(side=tk.LEFT, padx=4)
        ttk.Button(r1, text="Fetch Last 500",  command=lambda: self._fetch(500), width=14).pack(side=tk.LEFT, padx=4)
        ttk.Button(r1, text="Clear Remote Log", command=self._clear_remote, width=14).pack(side=tk.RIGHT, padx=4)

        # --- Filters ---
        ff = ttk.LabelFrame(self, text="Filters", padding=10)
        ff.pack(fill=tk.X, padx=10, pady=5)

        r2 = ttk.Frame(ff); r2.pack(fill=tk.X)
        ttk.Label(r2, text="Module:").pack(side=tk.LEFT, padx=5)
        self.mod_var = tk.StringVar(value="ALL")
        ttk.Combobox(r2, textvariable=self.mod_var, values=["ALL"] + DEBUG_MODULES,
                     width=12, state="readonly").pack(side=tk.LEFT, padx=5)

        ttk.Label(r2, text="Level:").pack(side=tk.LEFT, padx=5)
        self.lvl_var = tk.StringVar(value="ALL")
        ttk.Combobox(r2, textvariable=self.lvl_var, values=["ALL"] + DEBUG_LEVELS,
                     width=10, state="readonly").pack(side=tk.LEFT, padx=5)

        ttk.Label(r2, text="Search:").pack(side=tk.LEFT, padx=5)
        self.search_var = tk.StringVar()
        ttk.Entry(r2, textvariable=self.search_var, width=25).pack(side=tk.LEFT, padx=5)
        ttk.Button(r2, text="Apply", command=lambda: self._fetch(200)).pack(side=tk.LEFT, padx=5)

        # --- Log display ---
        lf = ttk.LabelFrame(self, text="Log Output", padding=5)
        lf.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        self.out = scrolledtext.ScrolledText(
            lf, height=25, font=("Monospace", 9),
            bg="#1e1e1e", fg="#d4d4d4", wrap=tk.NONE,
        )
        self.out.pack(fill=tk.BOTH, expand=True)

        # Color tags
        self.out.tag_config("error", foreground="#f44336")
        self.out.tag_config("warn",  foreground="#FF9800")
        self.out.tag_config("info",  foreground="#4CAF50")
        self.out.tag_config("debug", foreground="#64B5F6")
        self.out.tag_config("trace", foreground="#9E9E9E")

        br = ttk.Frame(lf); br.pack(fill=tk.X, pady=2)
        self.line_count = tk.StringVar(value="0 lines")
        ttk.Label(br, textvariable=self.line_count).pack(side=tk.LEFT, padx=5)
        ttk.Button(br, text="Clear", command=lambda: self.out.delete(1.0, tk.END)).pack(side=tk.RIGHT, padx=5)

    def _tag_for(self, line):
        if "ERROR" in line: return "error"
        if "WARN"  in line: return "warn"
        if "INFO"  in line: return "info"
        if "DEBUG" in line: return "debug"
        if "TRACE" in line: return "trace"
        return None

    def _append(self, line):
        # Strip ANSI escape codes (colors/bold/reset) before display
        clean = re.sub(r'\x1b\[[0-9;]*m', '', line)
        tag = self._tag_for(clean)
        if tag:
            self.out.insert(tk.END, clean + "\n", tag)
        else:
            self.out.insert(tk.END, clean + "\n")
        self.out.see(tk.END)

    def _passes(self, line):
        mod = self.mod_var.get()
        lvl = self.lvl_var.get()
        srch = self.search_var.get()
        if mod != "ALL" and mod not in line:
            return False
        if lvl != "ALL" and lvl not in line:
            return False
        if srch and srch.lower() not in line.lower():
            return False
        return True

    def _start_tail(self):
        self._stop_tail()
        def do():
            try:
                self.tail_proc = subprocess.Popen(
                    ["ssh", "-i", pi_cfg.ssh_key, "-o", "StrictHostKeyChecking=no",
                     pi_cfg.host, "tail -f /tmp/goball.log"],
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                )
                self.out.after(0, self._append, f"[{ts()}] Live tail started...")
                for line in iter(self.tail_proc.stdout.readline, ""):
                    if self.tail_proc is None:
                        break
                    line = line.rstrip()
                    if self._passes(line):
                        self.out.after(0, self._append, line)
            except Exception as e:
                self.out.after(0, self._append, f"Tail error: {e}")
        threading.Thread(target=do, daemon=True).start()

    def _stop_tail(self):
        if self.tail_proc:
            try:
                self.tail_proc.terminate()
            except Exception:
                pass
            self.tail_proc = None

    def _fetch(self, n=200):
        def do():
            try:
                r = ssh_cmd(f"tail -{n} /tmp/goball.log", timeout=15)
                self.out.after(0, lambda: self.out.delete(1.0, tk.END))
                if r.returncode == 0:
                    lines = r.stdout.strip().split("\n")
                    count = 0
                    for line in lines:
                        if self._passes(line):
                            self.out.after(0, self._append, line)
                            count += 1
                    self.out.after(0, lambda: self.line_count.set(f"{count} lines (of {len(lines)})"))
                else:
                    self.out.after(0, self._append, f"Error: {r.stderr.strip()}")
            except Exception as e:
                self.out.after(0, self._append, f"Fetch error: {e}")
        threading.Thread(target=do, daemon=True).start()

    def _clear_remote(self):
        if messagebox.askyesno("Confirm", "Clear /tmp/goball.log on Pi?"):
            def do():
                ssh_cmd("> /tmp/goball.log")
                self.out.after(0, lambda: self.out.delete(1.0, tk.END))
                self.out.after(0, self._append, "Remote log cleared.")
            threading.Thread(target=do, daemon=True).start()


# ============================================================================
# Tab 5 — Config Editor
# ============================================================================

class ConfigTab(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self._build_ui()

    def _build_ui(self):
        # --- Sound delays ---
        sf = ttk.LabelFrame(self, text="Sound Timing (milliseconds)", padding=10)
        sf.pack(fill=tk.X, padx=10, pady=5)

        self.sound_vars = {}
        for key, info in SOUND_DELAYS.items():
            row = ttk.Frame(sf); row.pack(fill=tk.X, pady=1)
            ttk.Label(row, text=f"{info['desc']}:", width=32, anchor=tk.W).pack(side=tk.LEFT, padx=5)
            var = tk.StringVar(value=str(info["default"]))
            self.sound_vars[key] = var
            ttk.Entry(row, textvariable=var, width=8).pack(side=tk.LEFT, padx=5)
            ttk.Label(row, text=f"({key})", foreground="gray").pack(side=tk.LEFT, padx=5)

        sb = ttk.Frame(sf); sb.pack(fill=tk.X, pady=5)
        ttk.Button(sb, text="Load from File", command=self._load_sound).pack(side=tk.LEFT, padx=5)
        ttk.Button(sb, text="Save to File",   command=self._save_sound).pack(side=tk.LEFT, padx=5)
        ttk.Button(sb, text="Reset Defaults",  command=self._reset_sound).pack(side=tk.LEFT, padx=5)

        # --- Debug config ---
        df = ttk.LabelFrame(self, text="Debug Configuration", padding=10)
        df.pack(fill=tk.X, padx=10, pady=5)

        r1 = ttk.Frame(df); r1.pack(fill=tk.X, pady=2)
        ttk.Label(r1, text="Build type / trace:").pack(side=tk.LEFT, padx=5)
        self.trace_var = tk.BooleanVar(value=False)
        ttk.Checkbutton(r1, variable=self.trace_var, text="Enable TRACE level").pack(side=tk.LEFT, padx=5)

        r2 = ttk.Frame(df); r2.pack(fill=tk.X, pady=2)
        ttk.Label(r2, text="Colors:").pack(side=tk.LEFT, padx=5)
        self.colors_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(r2, variable=self.colors_var, text="Enable").pack(side=tk.LEFT, padx=5)
        ttk.Label(r2, text="Timestamps:").pack(side=tk.LEFT, padx=15)
        self.ts_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(r2, variable=self.ts_var, text="Enable").pack(side=tk.LEFT, padx=5)

        db = ttk.Frame(df); db.pack(fill=tk.X, pady=5)
        ttk.Button(db, text="Load from CMake", command=self._load_debug).pack(side=tk.LEFT, padx=5)
        ttk.Button(db, text="Save to CMake",   command=self._save_debug).pack(side=tk.LEFT, padx=5)

        # --- Quick file viewer ---
        ff = ttk.LabelFrame(self, text="Quick File Viewer", padding=10)
        ff.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        fr = ttk.Frame(ff); fr.pack(fill=tk.X, pady=2)
        self.file_var = tk.StringVar(value="modules/sound_logic/sound_logic_event.h")
        common = [
            "modules/sound_logic/sound_logic_event.h",
            "modules/debug/debug.h",
            "modules/player_name/player_name.h",
            "modules/player_name/player_name.c",
            "CMakeLists.txt",
            "main.c",
            "lv_conf.h",
        ]
        ttk.Combobox(fr, textvariable=self.file_var, values=common, width=50).pack(side=tk.LEFT, padx=5)
        ttk.Button(fr, text="View", command=self._view_file).pack(side=tk.LEFT, padx=5)

        self.fout = scrolledtext.ScrolledText(
            ff, height=12, font=("Monospace", 9),
            bg="#1e1e1e", fg="#d4d4d4", wrap=tk.NONE,
        )
        self.fout.pack(fill=tk.BOTH, expand=True, pady=5)

    # -- Sound config --

    def _load_sound(self, silent=False):
        path = os.path.join(PROJECT_ROOT, "modules/sound_logic/sound_logic_event.h")
        try:
            with open(path) as f:
                content = f.read()
            for key, var in self.sound_vars.items():
                m = re.search(rf"#define\s+{key}\s+(\d+)", content)
                if m:
                    var.set(m.group(1))
            if not silent:
                messagebox.showinfo("Loaded", "Sound config loaded from file.")
        except Exception as e:
            messagebox.showerror("Error", str(e))

    def _save_sound(self):
        path = os.path.join(PROJECT_ROOT, "modules/sound_logic/sound_logic_event.h")
        try:
            with open(path) as f:
                content = f.read()
            for key, var in self.sound_vars.items():
                val = var.get().strip()
                if val.isdigit():
                    content = re.sub(
                        rf"(#define\s+{key}\s+)\d+",
                        rf"\g<1>{val}",
                        content,
                    )
            with open(path, "w") as f:
                f.write(content)
            messagebox.showinfo("Saved", "Sound config saved! Rebuild required.")
        except Exception as e:
            messagebox.showerror("Error", str(e))

    def _reset_sound(self):
        for key, info in SOUND_DELAYS.items():
            self.sound_vars[key].set(str(info["default"]))

    # -- Debug config --

    def _load_debug(self):
        path = os.path.join(PROJECT_ROOT, "CMakeLists.txt")
        try:
            with open(path) as f:
                content = f.read()
            self.trace_var.set("ENABLE_DEBUG_TRACE" in content and "ON)" in content.split("ENABLE_DEBUG_TRACE")[1][:40])
        except Exception as e:
            messagebox.showerror("Error", str(e))

    def _save_debug(self):
        path = os.path.join(PROJECT_ROOT, "CMakeLists.txt")
        try:
            with open(path) as f:
                content = f.read()

            trace_on = "ON" if self.trace_var.get() else "OFF"
            content = re.sub(
                r'(option\(ENABLE_DEBUG_TRACE\s+"[^"]+"\s+)\w+\)',
                rf"\g<1>{trace_on})",
                content,
            )

            with open(path, "w") as f:
                f.write(content)
            messagebox.showinfo("Saved", f"Debug trace={'ON' if self.trace_var.get() else 'OFF'}. Rebuild required.")
        except Exception as e:
            messagebox.showerror("Error", str(e))

    # -- File viewer --

    def _view_file(self):
        path = os.path.join(PROJECT_ROOT, self.file_var.get())
        try:
            with open(path) as f:
                content = f.read()
            self.fout.delete(1.0, tk.END)
            self.fout.insert(1.0, content)
        except Exception as e:
            self.fout.delete(1.0, tk.END)
            self.fout.insert(1.0, f"Error: {e}")


# ============================================================================
# Main Application
# ============================================================================

class GoBallDashboard(tk.Tk):
    def __init__(self):
        super().__init__()

        self.title("GoBall Development Dashboard")
        self.geometry("1200x780")
        self.minsize(900, 600)

        # Use clam theme for a modern-ish look
        style = ttk.Style()
        style.theme_use("clam")

        # Tab notebook
        nb = ttk.Notebook(self)
        nb.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Create tabs
        self.deploy_tab = DeployTab(nb)
        self.gpio_tab   = GPIOTab(nb)
        self.test_tab   = TestHarnessTab(nb)
        self.log_tab    = LogTab(nb)
        self.config_tab = ConfigTab(nb)

        nb.add(self.deploy_tab, text="  Deploy & Run  ")
        nb.add(self.gpio_tab,   text="  GPIO Simulator  ")
        nb.add(self.test_tab,   text="  Test Harness  ")
        nb.add(self.log_tab,    text="  Log Analyzer  ")
        nb.add(self.config_tab, text="  Config Editor  ")

        # Status bar
        self.status_var = tk.StringVar(value=f"Ready — Project: {PROJECT_ROOT}")
        status = ttk.Label(self, textvariable=self.status_var, relief=tk.SUNKEN, anchor=tk.W)
        status.pack(fill=tk.X, side=tk.BOTTOM)

        # Auto-load on startup
        self.after(100, self._startup)

    def _startup(self):
        self.deploy_tab._refresh_status()
        self.config_tab._load_sound(silent=True)

    def destroy(self):
        # Clean up any running tail process
        self.log_tab._stop_tail()
        super().destroy()


def main():
    app = GoBallDashboard()
    app.mainloop()


if __name__ == "__main__":
    main()
