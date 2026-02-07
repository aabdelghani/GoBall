# Quick Fix Reference

## What Changed

### Before (Problem)
```bash
# Build type hardcoded in Dockerfile
cmake -DCMAKE_BUILD_TYPE=Debug ..

# No tracking of what was built
# Sync didn't know if you switched Debug → Release
# Had to manually edit Dockerfile to change build type
```

### After (Fixed)
```bash
# Build type passed as environment variable
BUILD_TYPE=${BUILD_TYPE:-Debug}
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..

# Tracking file: ./docker-output/.build_type
# Sync detects build type changes
# Use --release or --debug flags
```

## Side-by-Side Usage

| Task | Old Way | New Way |
|------|---------|---------|
| **Build Release** | Edit Dockerfile, change "Debug" to "Release" | `./script.sh deploy --release` |
| **Build Debug** | Edit Dockerfile, change "Release" to "Debug" | `./script.sh deploy --debug` |
| **Check build type** | Open Dockerfile and look | `cat ./docker-output/.build_type` |
| **Switch builds** | Edit, rebuild, hope sync works | Just add `--release` or `--debug` |

## New Features

1. **Command line flags**
   ```bash
   --release  # Build in Release mode
   --debug    # Build in Debug mode (or use default from config)
   ```

2. **Build type tracking**
   - Local: `./docker-output/.build_type`
   - Remote: `/home/q/Desktop/SquareLine_Project/.build_type`

3. **Smart sync detection**
   - Compares current vs. last build type
   - Forces sync when build type changes
   - Shows what's being synced

4. **Better feedback**
   ```
   Build type: Release
   Building with BUILD_TYPE=Release
   Build type changed from Debug to Release, forcing sync
   Syncing binary (BUILD_TYPE=Release)...
   Running binary with BUILD_TYPE=Release
   ```

## Common Workflows

### Development (frequent changes, Debug)
```bash
# Default is Debug from config
./docker-build-and-run.sh deploy
```

### Testing Release Build
```bash
./docker-build-and-run.sh deploy --release
```

### Quick Iteration (no rebuild)
```bash
# Uses last built binary
./docker-build-and-run.sh sync-only
```

### Full Clean Build
```bash
rm -rf ./docker-output
./docker-build-and-run.sh all --release
```

## Key Files

| File | Purpose | Location |
|------|---------|----------|
| `deploy.conf` | Configuration (IP, user, paths, default build type) | Local, auto-created |
| `.build_type` | Tracks what was built | `./docker-output/` (local) |
| `.build_type` | Tracks what's deployed | `/home/q/Desktop/SquareLine_Project/` (RPi) |
| Binary | The actual executable | Both local and remote |

## Verification Commands

```bash
# Check local build type
cat ./docker-output/.build_type

# Check remote build type
ssh q@192.168.50.92 'cat /home/q/Desktop/SquareLine_Project/.build_type'

# Check binary size (Debug is typically larger)
ls -lh ./docker-output/SquareLine_Project
ssh q@192.168.50.92 'ls -lh /home/q/Desktop/SquareLine_Project/SquareLine_Project'

# Verify binary architecture
file ./docker-output/SquareLine_Project
```

## Typical Debug vs Release Differences

```bash
# Debug build
- Larger binary size (~5-10x bigger)
- Contains debug symbols
- No optimization
- Slower execution
- Better for debugging with gdb

# Release build
- Smaller binary size
- Stripped debug symbols
- Optimized (-O2 or -O3)
- Faster execution
- Better for production/performance testing
```

## Quick Start

1. **Make executable**
   ```bash
   chmod +x docker-build-and-run-fixed.sh
   ```

2. **First run (Debug)**
   ```bash
   ./docker-build-and-run-fixed.sh deploy
   ```

3. **Build Release**
   ```bash
   ./docker-build-and-run-fixed.sh deploy --release
   ```

4. **Check what's deployed**
   ```bash
   cat ./docker-output/.build_type
   ```

That's it! The script now properly handles Debug/Release switching.