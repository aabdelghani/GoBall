#!/bin/bash
PROJECT_DIR="/home/q/Projects/SquareLine_Project"
BUILD_DIR="$PROJECT_DIR/build-cross"
TOOLCHAIN_FILE="$PROJECT_DIR/toolchain-aarch64.cmake"

# Load configuration from deploy.conf
CONFIG_FILE="$PROJECT_DIR/deploy.conf"

# Default configuration (fallback if deploy.conf doesn't exist)
RPI_IP="192.168.50.92"
RPI_USER="q"
RPI_DEST_DIR="/home/q/Desktop"  # Direct to desktop

# Function to upload only the binary to Pi
upload_to_pi() {
    echo "Uploading binary to Raspberry Pi..."
    
    BINARY_PATH="$BUILD_DIR/SquareLine_Project"
    
    # Check if binary exists
    if [ ! -f "$BINARY_PATH" ]; then
        echo "ERROR: Binary not found at $BINARY_PATH"
        exit 1
    fi
    
    echo "Binary size: $(ls -lh $BINARY_PATH | awk '{print $5}')"
    
    # Test SSH connection first
    echo "Testing SSH connection to Pi..."
    if ! ssh $RPI_USER@$RPI_IP "echo 'SSH connection successful'"; then
        echo "ERROR: Cannot connect to Pi via SSH"
        echo "Please check:"
        echo "  1. Pi is powered on and connected to network"
        echo "  2. SSH is enabled on Pi"
        echo "  3. IP address is correct: $RPI_IP"
        echo "  4. Username is correct: $RPI_USER"
        exit 1
    fi
    
    # Check desktop directory permissions on Pi
    echo "Checking Pi desktop directory permissions..."
    ssh $RPI_USER@$RPI_IP "ls -la $RPI_DEST_DIR/ | head -5"
    
    # Try uploading to home directory first (which should always work)
    echo "Uploading SquareLine_Project to Pi's home directory..."
    scp "$BINARY_PATH" "$RPI_USER@$RPI_IP:/home/$RPI_USER/"
    
    if [ $? -eq 0 ]; then
        # Move from home to desktop and make executable
        echo "Moving file to desktop and making executable..."
        ssh $RPI_USER@$RPI_IP "mv /home/$RPI_USER/SquareLine_Project $RPI_DEST_DIR/ && chmod +x $RPI_DEST_DIR/SquareLine_Project"
        
        # Verify upload
        echo "Verifying upload..."
        ssh $RPI_USER@$RPI_IP "ls -la $RPI_DEST_DIR/SquareLine_Project && echo '✅ Upload successful!'"
        
        echo ""
        echo "🎉 Build and upload complete!"
        echo "📁 Binary uploaded to: $RPI_DEST_DIR/SquareLine_Project"
        echo "🚀 To run on Pi: $RPI_DEST_DIR/SquareLine_Project"
    else
        echo "❌ Upload failed! Trying alternative method..."
        
        # Alternative: Upload to home directory and create symlink on desktop
        echo "Trying alternative upload method..."
        scp "$BINARY_PATH" "$RPI_USER@$RPI_IP:/home/$RPI_USER/SquareLine_Project"
        
        if [ $? -eq 0 ]; then
            ssh $RPI_USER@$RPI_IP "chmod +x /home/$RPI_USER/SquareLine_Project"
            echo "✅ Uploaded to /home/$RPI_USER/SquareLine_Project"
            echo "📁 File location: /home/$RPI_USER/SquareLine_Project"
            echo "🚀 To run on Pi: ./SquareLine_Project"
        else
            echo "❌ All upload methods failed!"
            exit 1
        fi
    fi
}

# Load configuration if exists
if [ -f "$CONFIG_FILE" ]; then
    echo "Loading configuration from: $CONFIG_FILE"
    source "$CONFIG_FILE"
else
    echo "WARNING: Configuration file not found: $CONFIG_FILE"
    echo "Using default configuration"
fi

echo "Project dir: $PROJECT_DIR"
echo "Build dir: $BUILD_DIR"
echo "Toolchain: $TOOLCHAIN_FILE"
echo "Pi destination: $RPI_USER@$RPI_IP:$RPI_DEST_DIR/SquareLine_Project"

# Debug configuration options
DEBUG_TYPE="${1:-debug}"  # Default to debug, options: debug, dev, release, production
ENABLE_TRACE="${2:-OFF}"  # Default to OFF, set to ON for trace level
UPLOAD_TO_PI="${3:-ON}"   # Default to ON, set to OFF to skip upload

echo "Build type: $DEBUG_TYPE"
echo "Trace enabled: $ENABLE_TRACE"
echo "Upload to Pi: $UPLOAD_TO_PI"

# Check if files exist
if [ ! -f "$TOOLCHAIN_FILE" ]; then
    echo "ERROR: Toolchain file not found at $TOOLCHAIN_FILE"
    exit 1
fi

mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Clean previous build
echo "Cleaning previous build..."
rm -rf *

# Configure based on debug type
case $DEBUG_TYPE in
    "debug")
        echo "Configuring for DEBUG build (full debugging)"
        cmake -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE \
              -DCMAKE_BUILD_TYPE=Debug \
              -DENABLE_DEBUG=ON \
              -DENABLE_DEBUG_COLORS=ON \
              -DENABLE_DEBUG_TIMESTAMP=ON \
              -DENABLE_DEBUG_TRACE=$ENABLE_TRACE \
              $PROJECT_DIR
        ;;
    "dev")
        echo "Configuring for DEVELOPMENT build (balanced debugging)"
        cmake -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE \
              -DCMAKE_BUILD_TYPE=Debug \
              -DENABLE_DEBUG=ON \
              -DENABLE_DEBUG_COLORS=ON \
              -DENABLE_DEBUG_TIMESTAMP=ON \
              -DENABLE_DEBUG_TRACE=OFF \
              $PROJECT_DIR
        ;;
    "release")
        echo "Configuring for RELEASE build (minimal debugging)"
        cmake -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE \
              -DCMAKE_BUILD_TYPE=Release \
              -DENABLE_DEBUG=ON \
              -DENABLE_DEBUG_COLORS=OFF \
              -DENABLE_DEBUG_TIMESTAMP=OFF \
              -DENABLE_DEBUG_TRACE=OFF \
              $PROJECT_DIR
        ;;
    "production")
        echo "Configuring for PRODUCTION build (no debugging)"
        cmake -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_FILE \
              -DCMAKE_BUILD_TYPE=Release \
              -DENABLE_DEBUG=OFF \
              $PROJECT_DIR
        ;;
    *)
        echo "ERROR: Unknown build type '$DEBUG_TYPE'"
        echo "Usage: $0 [debug|dev|release|production] [ON|OFF] [ON|OFF]"
        echo "  First argument: build type"
        echo "  Second argument: enable trace (ON/OFF)"
        echo "  Third argument: upload to Pi (ON/OFF)"
        exit 1
        ;;
esac

# Build
echo "Building project..."
make -j$(nproc)

# Check if build was successful
if [ $? -eq 0 ]; then
    echo "Cross-compilation complete!"
    echo "Binary: $BUILD_DIR/SquareLine_Project"
    echo "Build type: $DEBUG_TYPE"
    echo "Debug level:"
    case $DEBUG_TYPE in
        "debug") echo "  - TRACE/DEBUG level (4-5)" ;;
        "dev") echo "  - DEBUG level (4)" ;;
        "release") echo "  - WARN level (2)" ;;
        "production") echo "  - NONE level (0)" ;;
    esac
    
    # Upload to Raspberry Pi if requested
    if [ "$UPLOAD_TO_PI" = "ON" ]; then
        upload_to_pi
    else
        echo "Skipping upload to Pi (UPLOAD_TO_PI=OFF)"
    fi
    
else
    echo "BUILD FAILED!"
    exit 1
fi