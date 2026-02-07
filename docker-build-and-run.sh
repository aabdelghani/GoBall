#!/bin/bash

set -e  # Exit on any error

SCRIPT_NAME=$(basename "$0")
CONFIG_FILE="deploy.conf"

# Default configuration
RPI_IP="192.168.50.92"
RPI_USER="q"
RPI_PATH="/home/q/Desktop/SquareLine_Project"
SSH_KEY="$HOME/.ssh/id_ed25519"
BUILD_TYPE="Debug"  # Can be Debug or Release

# Load configuration if exists
load_config() {
    if [ -f "$CONFIG_FILE" ]; then
        source "$CONFIG_FILE"
    else
        # Create default config file
        create_default_config
    fi
}

create_default_config() {
    cat > "$CONFIG_FILE" << EOF
# Raspberry Pi Deployment Configuration
RPI_IP="192.168.50.92"
RPI_USER="q"
RPI_PATH="/home/q/Desktop/SquareLine_Project"
SSH_KEY="$HOME/.ssh/id_ed25519"
BUILD_TYPE="Debug"  # Can be Debug or Release
EOF
    echo "Created default config file: $CONFIG_FILE"
    echo "Please edit it if your RPi IP or build type changes"
}

show_usage() {
    echo "Usage: $SCRIPT_NAME [command] [--release]"
    echo ""
    echo "Commands:"
    echo "  build        - Build the Docker image"
    echo "  run          - Run the container and build project (binary stays in container)"
    echo "  output       - Run the container and copy binary to ./docker-output/"
    echo "  deploy       - Build and deploy to Raspberry Pi"
    echo "  sync-only    - Only sync files to Raspberry Pi (no build)"
    echo "  all          - Build image, build project, and deploy to RPi"
    echo ""
    echo "Options:"
    echo "  --release    - Build in Release mode (default is Debug from config)"
    echo "  --debug      - Build in Debug mode"
    echo ""
    echo "Examples:"
    echo "  $SCRIPT_NAME build           # Build Docker image only"
    echo "  $SCRIPT_NAME deploy          # Build (Debug) and deploy to RPi"
    echo "  $SCRIPT_NAME deploy --release # Build (Release) and deploy to RPi"
    echo "  $SCRIPT_NAME sync-only       # Only sync files to RPi"
    echo "  $SCRIPT_NAME all --release   # Build image and deploy (Release)"
}

parse_build_type() {
    # Check command line arguments for build type override
    for arg in "$@"; do
        case "$arg" in
            --release)
                BUILD_TYPE="Release"
                ;;
            --debug)
                BUILD_TYPE="Debug"
                ;;
        esac
    done
    echo "Build type: $BUILD_TYPE"
}

build_image() {
    echo "=== Building Docker image ==="
    
    # Create Dockerfile
    cat > Dockerfile << 'EOF'
FROM ubuntu:24.04

# Set environment variables to avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Install cross-compilation toolchain and dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    autoconf \
    automake \
    libtool \
    autoconf-archive \
    git \
    wget \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    && rm -rf /var/lib/apt/lists/*

# Create project directory structure
RUN mkdir -p /home/q/Projects/SquareLine_Project

# Set working directory
WORKDIR /home/q/Projects/SquareLine_Project

# Copy entire project
COPY . .

# Build and install libgpiod v1.6.x to sysroot
RUN cd libgpiod-v1.6.x && \
    ./autogen.sh && \
    ./configure \
        --host=aarch64-linux-gnu \
        --prefix=/home/q/Projects/SquareLine_Project/rpi5-sysroot/usr \
        --enable-static=no \
        CFLAGS="--sysroot=/home/q/Projects/SquareLine_Project/rpi5-sysroot" \
        CXXFLAGS="--sysroot=/home/q/Projects/SquareLine_Project/rpi5-sysroot" && \
    make -j$(nproc) && \
    make install

# Create the build script with ARG support
RUN cat > /usr/local/bin/build-project.sh << 'SCRIPTEOF'
#!/bin/bash
cd /home/q/Projects/SquareLine_Project

# Get build type from environment or default to Debug
BUILD_TYPE=${BUILD_TYPE:-Debug}

echo "Building with BUILD_TYPE=$BUILD_TYPE"

# Clean previous build
rm -rf build-cross

# Create build directory and build
mkdir -p build-cross
cd build-cross
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..
make -j$(nproc)

# Verify the binary
echo "Build complete. Binary info:"
file SquareLine_Project
echo "Build type: $BUILD_TYPE"

# Copy binary to output directory if specified
if [ -n "$OUTPUT_DIR" ]; then
    mkdir -p $OUTPUT_DIR
    cp SquareLine_Project $OUTPUT_DIR/
    # Also save build type to a file
    echo "$BUILD_TYPE" > $OUTPUT_DIR/.build_type
    echo "Binary copied to: $OUTPUT_DIR/SquareLine_Project"
    echo "Build type saved to: $OUTPUT_DIR/.build_type"
fi
SCRIPTEOF

RUN chmod +x /usr/local/bin/build-project.sh

# Set entrypoint to build the project automatically
ENTRYPOINT ["/usr/local/bin/build-project.sh"]
EOF

    # Create .dockerignore
    cat > .dockerignore << 'EOF'
build/
build-cross/
*.swp
*.swo
.*.swp
.*.swo
*.o
*.a
*.so
*.dll
*.exe
.DS_Store
.git/
*.log
tmp/
EOF

    docker build -t squareline-cross-compile:latest .
    echo "=== Docker image built successfully ==="
}

run_container() {
    echo "=== Running container (binary stays in container) with BUILD_TYPE=$BUILD_TYPE ==="
    docker run --rm \
        -e BUILD_TYPE="$BUILD_TYPE" \
        squareline-cross-compile:latest
}

run_with_output() {
    echo "=== Running container and copying binary to ./docker-output/ with BUILD_TYPE=$BUILD_TYPE ==="
    mkdir -p ./docker-output
    docker run --rm \
        -e BUILD_TYPE="$BUILD_TYPE" \
        -e OUTPUT_DIR=/output \
        -v $(pwd)/docker-output:/output \
        squareline-cross-compile:latest
    echo "=== Binary available in ./docker-output/ (BUILD_TYPE=$BUILD_TYPE) ==="
}

show_progress() {
    local duration=$1
    local steps=20
    local step_duration=$(echo "scale=3; $duration/$steps" | bc)
    
    for i in $(seq 1 $steps); do
        printf "\r[%-20s] %d%%" "$(printf '#%.0s' $(seq 1 $i))" "$((i*5))"
        sleep $step_duration
    done
    printf "\n"
}

kill_running_app() {
    echo "Checking for running SquareLine_Project on RPi..."
    
    # Use a more robust approach that doesn't fail the script
    set +e  # Disable exit on error temporarily
    
    # Check if process exists
    ssh -i "$SSH_KEY" "$RPI_USER@$RPI_IP" "pgrep -f 'SquareLine_Project'" > /dev/null 2>&1
    local process_exists=$?
    
    set -e  # Re-enable exit on error
    
    if [ $process_exists -eq 0 ]; then
        echo "Stopping running SquareLine_Project..."
        
        # Try graceful kill first
        set +e
        ssh -i "$SSH_KEY" "$RPI_USER@$RPI_IP" "pkill -f 'SquareLine_Project'" > /dev/null 2>&1
        set -e
        
        sleep 2
        
        # Check if still running and force kill if needed
        set +e
        ssh -i "$SSH_KEY" "$RPI_USER@$RPI_IP" "pgrep -f 'SquareLine_Project'" > /dev/null 2>&1
        local still_running=$?
        set -e
        
        if [ $still_running -eq 0 ]; then
            echo "Force stopping stubborn process..."
            set +e
            ssh -i "$SSH_KEY" "$RPI_USER@$RPI_IP" "pkill -9 -f 'SquareLine_Project'" > /dev/null 2>&1
            set -e
            sleep 1
        fi
        
        echo "✅ Stopped running application"
    else
        echo "No running SquareLine_Project found"
    fi
}

sync_missing_audio_files() {
    echo "Checking for missing audio files on RPi..."
    
    # Create remote directory if it doesn't exist
    ssh -i "$SSH_KEY" "$RPI_USER@$RPI_IP" "mkdir -p '$RPI_PATH/modules/game_sounds'"
    
    # Get list of local audio files
    local_local_files=$(find ./modules/game_sounds -type f -name "*.wav" 2>/dev/null | sort || echo "")
    
    if [ -z "$local_local_files" ]; then
        echo "No local audio files found, skipping audio sync"
        return
    fi
    
    # Get list of remote audio files
    local_remote_files=$(ssh -i "$SSH_KEY" "$RPI_USER@$RPI_IP" "find '$RPI_PATH/modules/game_sounds' -type f -name '*.wav' 2>/dev/null | sort" || echo "")
    
    # Find missing files
    local_missing_files=""
    for local_file in $local_local_files; do
        local_filename=$(basename "$local_file")
        if ! echo "$local_remote_files" | grep -q "$local_filename"; then
            local_missing_files="$local_missing_files $local_file"
        fi
    done
    
    if [ -n "$local_missing_files" ]; then
        echo "Found $(echo "$local_missing_files" | wc -w) missing audio files, syncing..."
        # Sync only missing files
        for file in $local_missing_files; do
            echo "Syncing missing file: $(basename "$file")"
            scp -i "$SSH_KEY" -o LogLevel=ERROR "$file" "$RPI_USER@$RPI_IP:$RPI_PATH/modules/game_sounds/"
        done
        echo "Missing audio files synced"
    else
        echo "All audio files are up to date on RPi"
    fi
}

check_binary_needs_sync() {
    # Check if binary exists locally
    if [ ! -f "./docker-output/SquareLine_Project" ]; then
        echo "Binary not found locally"
        return 0  # Needs sync (should build first)
    fi
    
    # Check if build type file exists
    if [ ! -f "./docker-output/.build_type" ]; then
        echo "Build type unknown, forcing sync"
        return 0  # Needs sync
    fi
    
    # Check if build type has changed
    local last_build_type=$(cat ./docker-output/.build_type 2>/dev/null || echo "Unknown")
    if [ "$last_build_type" != "$BUILD_TYPE" ]; then
        echo "Build type changed from $last_build_type to $BUILD_TYPE, forcing sync"
        return 0  # Needs sync
    fi
    
    # Check if remote binary exists and compare
    set +e
    ssh -i "$SSH_KEY" "$RPI_USER@$RPI_IP" "test -f '$RPI_PATH/SquareLine_Project'" 2>/dev/null
    local remote_exists=$?
    set -e
    
    if [ $remote_exists -ne 0 ]; then
        echo "Binary doesn't exist on RPi, syncing"
        return 0  # Needs sync
    fi
    
    # Binary exists on both sides and build type matches
    # Always sync to be safe (size/timestamp checks can be unreliable)
    echo "Syncing binary (BUILD_TYPE=$BUILD_TYPE)"
    return 0
}

sync_to_rpi() {
    echo "=== Syncing files to Raspberry Pi ($RPI_IP) ==="
    
    # Check if binary needs sync
    if ! check_binary_needs_sync; then
        echo "Error: Binary not ready for sync"
        exit 1
    fi
    
    # Check SSH connection
    echo "Testing SSH connection to $RPI_USER@$RPI_IP..."
    if ! ssh -o ConnectTimeout=5 -i "$SSH_KEY" "$RPI_USER@$RPI_IP" "echo 'Connection successful'"; then
        echo "Error: Cannot connect to Raspberry Pi. Check config in $CONFIG_FILE"
        exit 1
    fi
    
    # Kill running application
    echo "=== Stopping any running application ==="
    kill_running_app
    
    # Create destination directory on RPi
    echo "Creating directory on RPi: $RPI_PATH"
    ssh -i "$SSH_KEY" "$RPI_USER@$RPI_IP" "mkdir -p '$RPI_PATH'"
    ssh -i "$SSH_KEY" "$RPI_USER@$RPI_IP" "mkdir -p '$RPI_PATH/modules'"
    
    # Remove old binary and upload new one
    echo "Removing old binary and uploading new one..."
    ssh -i "$SSH_KEY" "$RPI_USER@$RPI_IP" "rm -f '$RPI_PATH/SquareLine_Project'"
    
    # Sync binary with progress
    local last_build_type=$(cat ./docker-output/.build_type 2>/dev/null || echo "Unknown")
    echo "Syncing binary (BUILD_TYPE=$last_build_type)..."
    scp -i "$SSH_KEY" -o LogLevel=ERROR ./docker-output/SquareLine_Project "$RPI_USER@$RPI_IP:$RPI_PATH/" &
    scp_pid=$!
    show_progress 2
    wait $scp_pid
    
    # Sync build type info to RPi for reference
    echo "$last_build_type" | ssh -i "$SSH_KEY" "$RPI_USER@$RPI_IP" "cat > '$RPI_PATH/.build_type'"
    
    # Sync only missing audio files
    sync_missing_audio_files
    
    # Set executable permission
    ssh -i "$SSH_KEY" "$RPI_USER@$RPI_IP" "chmod +x '$RPI_PATH/SquareLine_Project'"
    
    echo "=== Sync complete ==="
    echo "Files deployed to: $RPI_PATH on $RPI_IP"
    echo "Binary: $RPI_PATH/SquareLine_Project (BUILD_TYPE=$last_build_type)"
    echo "Game sounds: $RPI_PATH/modules/game_sounds/"
}

run_on_rpi() {
    echo "=== Starting SquareLine_Project on RPi (with GUI and terminal output) ==="
    
    local remote_build_type=$(ssh -i "$SSH_KEY" "$RPI_USER@$RPI_IP" "cat '$RPI_PATH/.build_type' 2>/dev/null" || echo "Unknown")
    echo "Running binary with BUILD_TYPE=$remote_build_type"
    
    echo "Starting application on RPi desktop with terminal output..."
    echo "The GUI will appear on the RPi screen and output will show below:"
    echo "------------------------------------------------------------"
    
    # Start the application on the RPi's desktop with terminal output
    ssh -i "$SSH_KEY" -t "$RPI_USER@$RPI_IP" \
        "export DISPLAY=:0 && cd '$RPI_PATH' && ./SquareLine_Project"
    
    echo "------------------------------------------------------------"
    echo "Application has stopped on the RPi"
    echo "To restart: ./$(basename "$0") sync-only"
}

deploy_to_rpi() {
    echo "=== Building and deploying to Raspberry Pi (BUILD_TYPE=$BUILD_TYPE) ==="
    run_with_output
    sync_to_rpi
    run_on_rpi
}

sync_only() {
    # For sync-only, we use whatever was last built
    if [ -f "./docker-output/.build_type" ]; then
        BUILD_TYPE=$(cat ./docker-output/.build_type)
        echo "Using last built binary (BUILD_TYPE=$BUILD_TYPE)"
    fi
    sync_to_rpi
    run_on_rpi
}

# Main script logic
load_config

# Parse build type from arguments
parse_build_type "$@"

# Filter out build type arguments for case statement
COMMAND="${1:-}"

case "$COMMAND" in
    "build")
        build_image
        ;;
    "run")
        run_container
        ;;
    "output")
        run_with_output
        ;;
    "deploy")
        deploy_to_rpi
        ;;
    "sync-only")
        sync_only
        ;;
    "all")
        build_image
        deploy_to_rpi
        ;;
    "")
        show_usage
        ;;
    *)
        echo "Error: Unknown command '$COMMAND'"
        show_usage
        exit 1
        ;;
esac