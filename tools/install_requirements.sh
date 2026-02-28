#!/usr/bin/env bash
# Install dependencies for the GoBall Development Dashboard
# Usage: ./tools/install_requirements.sh

set -e

echo "=== GoBall Dashboard — Installing Requirements ==="

# Detect package manager
if command -v apt-get &>/dev/null; then
    PKG_MGR="apt-get"
elif command -v dnf &>/dev/null; then
    PKG_MGR="dnf"
elif command -v pacman &>/dev/null; then
    PKG_MGR="pacman"
else
    echo "ERROR: No supported package manager found (apt/dnf/pacman)"
    exit 1
fi

echo "Package manager: $PKG_MGR"

install_pkg() {
    echo "Installing: $*"
    if [ "$PKG_MGR" = "pacman" ]; then
        sudo pacman -S --noconfirm --needed "$@"
    else
        sudo "$PKG_MGR" install -y "$@"
    fi
}

# Python 3 + tkinter
if ! python3 -c "import tkinter" 2>/dev/null; then
    echo "tkinter not found — installing..."
    case $PKG_MGR in
        apt-get) install_pkg python3-tk ;;
        dnf)     install_pkg python3-tkinter ;;
        pacman)  install_pkg tk ;;
    esac
else
    echo "tkinter: OK"
fi

# SSH client
if ! command -v ssh &>/dev/null; then
    echo "ssh not found — installing..."
    case $PKG_MGR in
        apt-get) install_pkg openssh-client ;;
        dnf)     install_pkg openssh-clients ;;
        pacman)  install_pkg openssh ;;
    esac
else
    echo "ssh: OK"
fi

# SCP (usually bundled with SSH)
if ! command -v scp &>/dev/null; then
    echo "scp not found — installing..."
    case $PKG_MGR in
        apt-get) install_pkg openssh-client ;;
        dnf)     install_pkg openssh-clients ;;
        pacman)  install_pkg openssh ;;
    esac
else
    echo "scp: OK"
fi

# CMake + Make (for build tab)
if ! command -v cmake &>/dev/null; then
    echo "cmake not found — installing..."
    install_pkg cmake
else
    echo "cmake: OK"
fi

if ! command -v make &>/dev/null; then
    echo "make not found — installing..."
    case $PKG_MGR in
        apt-get) install_pkg build-essential ;;
        dnf)     install_pkg make ;;
        pacman)  install_pkg make ;;
    esac
else
    echo "make: OK"
fi

# Cross-compiler (for aarch64 build)
if ! command -v aarch64-linux-gnu-gcc &>/dev/null; then
    echo "aarch64 cross-compiler not found — installing..."
    case $PKG_MGR in
        apt-get) install_pkg gcc-aarch64-linux-gnu ;;
        dnf)     install_pkg gcc-aarch64-linux-gnu ;;
        pacman)  install_pkg aarch64-linux-gnu-gcc ;;
    esac
else
    echo "aarch64-linux-gnu-gcc: OK"
fi

echo ""
echo "=== All requirements satisfied ==="
echo "Run the dashboard with: python3 tools/goball_dashboard.py"
