# Deployment Script Fixes - BUILD_TYPE Sync Issue

## Problem
The original script had these issues:
1. Build type (Debug/Release) was hardcoded in the Dockerfile
2. No way to change build type without editing the Dockerfile
3. Sync didn't detect when build type changed
4. Binary would appear up-to-date even after switching Debug ↔ Release

## Solution Overview
The fixed script adds proper BUILD_TYPE handling:
- ✅ Build type can be specified via command line flags
- ✅ Build type is tracked in `.build_type` file
- ✅ Sync detects build type changes and forces re-upload
- ✅ RPi stores build type info for reference

## Key Changes

### 1. Added BUILD_TYPE Configuration
```bash
# In deploy.conf (auto-generated)
BUILD_TYPE="Debug"  # Can be Debug or Release
```

### 2. Command Line Flags
```bash
--release    # Build in Release mode
--debug      # Build in Debug mode
```

### 3. Build Type Tracking
The script now:
- Saves build type to `./docker-output/.build_type` after each build
- Compares current BUILD_TYPE with last build before syncing
- Forces sync if build type changed
- Saves build type on RPi for reference

### 4. Docker Container Updates
The Dockerfile build script now:
- Accepts `BUILD_TYPE` environment variable
- Defaults to Debug if not specified
- Saves build type alongside binary

## Usage Examples

### Build and Deploy in Debug Mode (default)
```bash
./docker-build-and-run.sh deploy
```

### Build and Deploy in Release Mode
```bash
./docker-build-and-run.sh deploy --release
```

### Switch from Debug to Release
```bash
# First build was Debug
./docker-build-and-run.sh deploy

# Now switch to Release - will rebuild and force sync
./docker-build-and-run.sh deploy --release
```

### Just Build (no deploy)
```bash
# Debug build
./docker-build-and-run.sh output

# Release build
./docker-build-and-run.sh output --release
```

### Build Docker Image and Deploy Everything
```bash
# Debug
./docker-build-and-run.sh all

# Release
./docker-build-and-run.sh all --release
```

### Sync Only (use last built binary)
```bash
./docker-build-and-run.sh sync-only
```

## What Gets Synced

### Always Synced
- Binary executable (if build type changed or doesn't exist on RPi)
- Build type metadata

### Conditionally Synced
- Audio files in `modules/game_sounds/` (only missing files)

## Build Type Detection Logic

```
1. Check if binary exists locally
   ├─ No → Need to build first
   └─ Yes → Continue

2. Check if .build_type file exists
   ├─ No → Force sync (unknown previous build)
   └─ Yes → Continue

3. Compare current BUILD_TYPE with last build
   ├─ Different → Force sync (build type changed)
   └─ Same → Continue

4. Check if binary exists on RPi
   ├─ No → Sync needed
   └─ Yes → Sync anyway (to be safe)
```

## Configuration File (deploy.conf)

After first run, edit this file to customize:
```bash
# Raspberry Pi Deployment Configuration
RPI_IP="192.168.50.92"
RPI_USER="q"
RPI_PATH="/home/q/Desktop/SquareLine_Project"
SSH_KEY="$HOME/.ssh/id_ed25519"
BUILD_TYPE="Debug"  # Default build type
```

## Debugging

The script shows current build type at key points:
```
Build type: Release
Building with BUILD_TYPE=Release
Binary copied to: ./docker-output/SquareLine_Project
Build type saved to: ./docker-output/.build_type
Syncing binary (BUILD_TYPE=Release)...
Running binary with BUILD_TYPE=Release
```

## Troubleshooting

### "Build type changed, forcing sync"
This is normal when switching between Debug and Release. The script detected the change and will re-upload the binary.

### Check what's currently deployed on RPi
```bash
ssh q@192.168.50.92 "cat /home/q/Desktop/SquareLine_Project/.build_type"
```

### Force rebuild everything
```bash
# Remove local build artifacts
rm -rf ./docker-output

# Rebuild and deploy
./docker-build-and-run-fixed.sh deploy --release
```

## Migration from Old Script

1. **Backup old script** (optional):
   ```bash
   mv docker-build-and-run.sh docker-build-and-run.sh.bak
   ```

2. **Use new script**:
   ```bash
   mv docker-build-and-run-fixed.sh docker-build-and-run.sh
   chmod +x docker-build-and-run.sh
   ```

3. **First run** (creates config):
   ```bash
   ./docker-build-and-run.sh deploy
   ```

4. **Edit config if needed**:
   ```bash
   nano deploy.conf
   ```

## Performance Notes

- Build type tracking adds minimal overhead (just a small text file)
- Sync logic is safer: always syncs when in doubt
- Audio file sync remains efficient (only missing files)
- Progress indicators show deployment status

## Summary

The fixed script ensures that:
- ✅ Debug and Release builds are properly tracked
- ✅ Changing build type always triggers proper sync
- ✅ You can see what build type is currently deployed
- ✅ No manual Dockerfile editing required
- ✅ Simple command line interface for build type selection