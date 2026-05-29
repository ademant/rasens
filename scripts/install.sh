#!/bin/bash

# Installation script for Rasens service
# Run with sudo or as root

set -e

PROJECT_DIR="$(dirname "$(dirname "$0")")"
BIN_DIR="/opt/rasens/bin"
CONFIG_DIR="/etc/rasens"
SYSTEMD_DIR="/etc/systemd/system"
LOG_DIR="/var/log/rasens"
RUN_DIR="/var/run"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Installing Rasens Service ===${NC}"

# Check if running as root
if [ "$(id -u)" -ne 0 ]; then
    echo -e "${RED}Error: This script must be run as root (use sudo)${NC}"
    exit 1
fi

# Create directories
echo -e "${YELLOW}Creating directories...${NC}"
mkdir -p "$BIN_DIR"
mkdir -p "$CONFIG_DIR"
mkdir -p "$LOG_DIR"
mkdir -p "$RUN_DIR"
echo "  ✓ Directories created"

# Build the project
echo -e "${YELLOW}Building project...${NC}"
cd "$PROJECT_DIR"
if [ -d build ]; then
    rm -rf build
fi
mkdir -p build
cd build

if command -v cmake &> /dev/null; then
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    echo "  ✓ Project built with CMake"
else
    # Fallback to direct g++ compilation
    g++ -std=c++17 -o rasens ../src/main.cpp ../src/utils.cpp ../src/config.cpp -I../include -lpthread
    echo "  ✓ Project built with g++"
fi

# Install binary
echo -e "${YELLOW}Installing binary...${NC}"
cp rasens "$BIN_DIR/rasens"
chmod +x "$BIN_DIR/rasens"
echo "  ✓ Binary installed to $BIN_DIR/rasens"

# Install configuration
echo -e "${YELLOW}Installing configuration...${NC}"
cp "$PROJECT_DIR/config/rasens.conf" "$CONFIG_DIR/config.conf"
chmod 644 "$CONFIG_DIR/config.conf"
echo "  ✓ Configuration installed to $CONFIG_DIR/config.conf"

# Install systemd service
echo -e "${YELLOW}Installing systemd service...${NC}"
cp "$PROJECT_DIR/systemd/rasens.service" "$SYSTEMD_DIR/rasens.service"
chmod 644 "$SYSTEMD_DIR/rasens.service"
echo "  ✓ Systemd service installed to $SYSTEMD_DIR/rasens.service"

# Reload systemd
echo -e "${YELLOW}Reloading systemd...${NC}"
systemctl daemon-reload
echo "  ✓ Systemd reloaded"

# Enable service (optional - comment out if you want manual start)
echo -e "${YELLOW}Enabling service...${NC}"
systemctl enable rasens.service
echo "  ✓ Service enabled (will start on boot)"

echo ""
echo -e "${GREEN}=== Installation Complete ===${NC}"
echo ""
echo "To start the service:"
echo "  systemctl start rasens"
echo ""
echo "To check status:"
echo "  systemctl status rasens"
echo ""
echo "To view logs:"
echo "  journalctl -u rasens -f"
echo ""
echo "Configuration file:"
echo "  $CONFIG_DIR/config.conf"
echo ""
echo "Edit the configuration and restart the service:"
echo "  systemctl restart rasens"
