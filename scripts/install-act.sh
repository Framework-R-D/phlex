#!/bin/bash

# Install or update `act` (GitHub Actions local runner) to the latest version
# For use on AlmaLinux, Rocky, or other RHEL-compatible systems
#
# Usage: ./scripts/install-act.sh
#        sudo ./scripts/install-act.sh  # if /usr/local/bin is not writable

set -euo pipefail

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

log() {
    echo -e "${BLUE}[act installer]${NC} $1"
}

success() {
    echo -e "${GREEN}[act installer]${NC} $1"
}

error() {
    echo -e "${RED}[act installer]${NC} $1" >&2
}

# Check if curl is available
if ! command -v curl &> /dev/null; then
    error "curl is required but not installed. Please install it with: sudo dnf install curl"
    exit 1
fi

# Determine system architecture
ARCH=$(uname -m)
case "$ARCH" in
    x86_64)
        ARCH_SUFFIX="x86_64"
        ;;
    aarch64)
        ARCH_SUFFIX="arm64"
        ;;
    *)
        error "Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

log "Fetching latest act release..."

# Get the latest release
LATEST=$(curl -s https://api.github.com/repos/nektos/act/releases/latest | grep -o '"tag_name": "[^"]*"' | cut -d'"' -f4)

if [ -z "$LATEST" ]; then
    error "Failed to fetch latest release from GitHub. Please check your internet connection."
    exit 1
fi

log "Latest version: $LATEST"

DOWNLOAD_URL="https://github.com/nektos/act/releases/download/${LATEST}/act_Linux_${ARCH_SUFFIX}.tar.gz"

log "Downloading from: $DOWNLOAD_URL"

# Create a temporary directory
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

# Download and extract
if ! curl -sL "$DOWNLOAD_URL" | tar xz -C "$TEMP_DIR"; then
    error "Failed to download or extract act binary"
    exit 1
fi

# Determine install location
INSTALL_DIR="${INSTALL_DIR:-/usr/local/bin}"

# Check if we can write to the install directory
if [ ! -w "$INSTALL_DIR" ]; then
    error "$INSTALL_DIR is not writable. Please run with sudo or set INSTALL_DIR=/path/to/writable/dir"
    exit 1
fi

log "Installing to $INSTALL_DIR..."

# Copy the binary
if [ -f "$TEMP_DIR/act" ]; then
    cp "$TEMP_DIR/act" "$INSTALL_DIR/act"
    chmod +x "$INSTALL_DIR/act"
else
    error "act binary not found in archive"
    exit 1
fi

# Verify installation
if ! "$INSTALL_DIR/act" --version > /dev/null 2>&1; then
    error "Installation verification failed"
    exit 1
fi

success "Installation successful!"
"$INSTALL_DIR/act" --version
log "You can now use: act --help"
