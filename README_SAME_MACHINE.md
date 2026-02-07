# 🔧 SquareLine Project – Same Machine Cross-Compilation

This guide shows how to **cross-compile your SquareLine Project for Raspberry Pi 5** directly on your Ubuntu machine **without Docker**.

---

## 🎯 Quick Start

```bash
# Make the script executable and run it
chmod +x cross-compile.sh
./cross-compile.sh
```

---

## 📁 File Structure

```
SquareLine_Project/
├── 🔧 cross-compile.sh              # Cross-compilation script
├── ⚙️ toolchain-aarch64.cmake      # Cross-compilation toolchain
├── 📚 rpi5-sysroot/                # RPi5 system libraries  
├── 🎮 sdl2-dev-rpi64/              # SDL2 libraries for RPi5
├── 📦 build-cross/                 # Build output directory
└── ... (your source code)
```

---

## 🛠️ cross-compile.sh

```bash
#!/bin/bash
PROJECT_DIR="/home/q/Projects/SquareLine_Project"
BUILD_DIR="$PROJECT_DIR/build-cross"
TOOLCHAIN_FILE="$PROJECT_DIR/toolchain-aarch64.cmake"

echo "Project dir: $PROJECT_DIR"
echo "Build dir: $BUILD_DIR"
echo "Toolchain: $TOOLCHAIN_FILE"

# Check if files exist
if [ ! -f "$TOOLCHAIN_FILE" ]; then
    echo "ERROR: Toolchain file not found at $TOOLCHAIN_FILE"
    exit 1
fi

mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Clean previous build
rm -rf *

# Configure with explicit paths
cmake -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE -DCMAKE_BUILD_TYPE=Debug $PROJECT_DIR

# Build
make -j$(nproc)

echo "Cross-compilation complete. Binary: $BUILD_DIR/SquareLine_Project"
```

---

## ⚙️ Prerequisites

Install the cross-compilation toolchain:

```bash
sudo apt update
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu cmake pkg-config
```

---

## 📋 What the Script Does

- 🔍 Validates toolchain file exists  
- 📁 Creates build directory  
- 🧹 Cleans previous builds  
- ⚙️ Configures CMake with cross-compilation toolchain  
- 🔨 Builds the project using all CPU cores  
- ✅ Outputs the final binary location  

---

## 🎯 Output

After successful compilation, you'll get:

```
Cross-compilation complete. Binary: /home/q/Projects/SquareLine_Project/build-cross/SquareLine_Project
```

---

## 🔍 Verify the Binary

Check that the binary is compiled for RPi5 (aarch64):

```bash
file build-cross/SquareLine_Project
```

Should show:

```
SquareLine_Project: ELF 64-bit LSB pie executable, ARM aarch64, version 1 (SYSV), dynamically linked...
```

---

## 🚀 Manual Deployment

After building, deploy to RPi5 manually:

```bash
# Copy binary to RPi5
scp build-cross/SquareLine_Project q@192.168.50.92:/home/q/Desktop/SquareLine_Project/

# Copy audio files (if needed)
scp -r modules/game_sounds q@192.168.50.92:/home/q/Desktop/SquareLine_Project/modules/

# Set executable permissions and run
ssh q@192.168.50.92 "chmod +x /home/q/Desktop/SquareLine_Project/SquareLine_Project"
ssh q@192.168.50.92 "cd /home/q/Desktop/SquareLine_Project && ./SquareLine_Project"
```

---

## ⚡ Benefits of Same-Machine Cross-Compilation

- 🚀 **Faster** than Docker (no container overhead)  
- 🔧 **Direct access** to host tools and libraries  
- 🐛 **Easier debugging** with native tools  
- 📝 **Simpler workflow** for quick iterations  

---

## 🆚 Docker vs Same-Machine Comparison

| Aspect | Docker Approach | Same-Machine Approach |
|--------|----------------|-----------------------|
| Speed | 🐢 Slower (container overhead) | 🚀 Faster (native compilation) |
| Isolation | ✅ Complete environment isolation | ❌ Uses host tools |
| Setup | ⚙️ More complex (Docker setup) | 🔧 Simple (apt install) |
| Portability | ✅ Consistent across machines | ❌ Host-dependent |
| Use Case | Production builds, CI/CD | Daily development, quick iterations |

---

## 🚨 Troubleshooting

### ❌ Toolchain file not found?
```bash
# Create the toolchain file
cat > toolchain-aarch64.cmake << 'EOF'
# AArch64 Raspberry Pi 5 cross-compilation toolchain
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_SYSROOT /home/q/Projects/SquareLine_Project/rpi5-sysroot)
# ... (rest of toolchain content)
EOF
```

---

### ❌ Missing dependencies?
```bash
# Install required packages
sudo apt install build-essential cmake pkg-config libsdl2-dev
```

---

### ❌ Build failures?
```bash
# Check CMake configuration
cd build-cross
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-aarch64.cmake -DCMAKE_BUILD_TYPE=Debug .. --debug-output

# Check make output for specific errors
make -j$(nproc) VERBOSE=1
```

---

Choose this approach for **faster development cycles! 🔥**