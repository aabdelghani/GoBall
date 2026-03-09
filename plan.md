# Plan: User Manual + Build Tab

## Task 1: Comprehensive User Manual (`doc/user-manual.md`)

A player-facing guide explaining the GoBall system from a user perspective:

### Sections:
1. **Introduction** — What is GoBall, what hardware is involved
2. **Getting Started** — Power on, touchscreen basics, navigating the main menu
3. **Game Modes** (detailed rules for each):
   - **Stroke Play** (1-4 players, 9/18 holes) — lowest total score wins
   - **Match Play 1v1** (9/18 holes) — hole-by-hole "Up & Down" format, early victory
   - **Match Play 2v2** (9/18 holes) — team-based, combined scores per hole
   - **Quota Points** (1-4 players, 9/18 holes) — preset par targets (3pt/4pt/5pt) counting down to zero
   - **Vegas Quota Points** (1-4 players, 9/18 holes) — bonus scoring after completing categories, category locks when 2 players close it
4. **Scoring** — How the 4 holes score (5pts, 4pts, 3pts, 0pts), turn order
5. **Player Names** — How to tap and edit names
6. **Audio & Visual Feedback** — LED colors, sound announcements
7. **Video Tips** — How to access the Visualize instructional video
8. **Scorecard** — Reading the scorecard screen

Source material: README.md, doc/system-design-document.md, game mode source files.

---

## Task 2: New "Build" Tab in Dashboard (`tools/goball_dashboard.py`)

Replace/extend the simple build button with a full Build tab offering 3 build methods.

### UI Layout:
```
+-----------------------------------------------+
| Build Method:  ( ) Local Cross-Compile         |
|                ( ) Docker                       |
|                ( ) Yocto                        |
+-----------------------------------------------+
| [Method-specific config panel]                 |
|                                                |
| Local:                                         |
|   Build Dir: [build-master    ] [Browse]       |
|   Build Type: [Debug v]                        |
|   Toolchain: [toolchain-aarch64.cmake] [Browse]|
|                                                |
| Docker:                                        |
|   Docker Script: [docker-build-and-run.sh]     |
|   Build Type: [Debug v]                        |
|   Output Dir: [docker-output/] [Browse]        |
|                                                |
| Yocto:                                         |
|   Yocto Build Dir: [/path/to/poky/build]       |
|   Recipe Path: [meta-goball/recipes-app/...]    |
|   Machine: [raspberrypi5    ]                  |
|   Image: [core-image-goball]                   |
|   [Browse Build Dir] [Browse Recipe]           |
|                                                |
+-----------------------------------------------+
| [Build]  [Clean Build]         Status: Idle    |
+-----------------------------------------------+
| Build Output (live scrolling log)              |
|                                                |
| > make -C build-master -j12                    |
| > [2/45] Building C object ...                 |
| > ...                                          |
+-----------------------------------------------+
```

### Behavior:
- Radio buttons select build method
- Config panel dynamically swaps based on selection
- **Local**: runs `make -C <build-dir> -j$(nproc)`, clean = `make -C <dir> clean`
- **Docker**: runs `./docker-build-and-run.sh output [--release|--debug]`, clean = `docker rmi`
- **Yocto**: runs `source oe-init-build-env <build-dir> && bitbake <image>`, clean = `bitbake -c cleansstate goball`
- Build output streams live into ScrolledText widget
- Status label shows: Idle / Building... / Success / Failed
- Stop button to cancel a running build
- All file/dir fields have [Browse] buttons using filedialog

### Changes to existing tabs:
- Move the "Build" button logic out of DeployTab into this new BuildTab
- DeployTab keeps Deploy/Start/Stop/Kill/Restart (no more build button)
- Tab order: Deploy & Run, **Build**, GPIO Simulator, Test Harness, Log Analyzer, Config Editor
