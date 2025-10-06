#!/bin/bash
# Build script for Hantei-chan (cross-compile for Windows from Linux/WSL)

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Hantei-chan Build Script ===${NC}"

# Check if git is available (needed for version info)
if ! command -v git &> /dev/null; then
    echo -e "${YELLOW}Warning: Git not found. Build numbers will default to 0.${NC}"
else
    # Show current version info
    COMMIT_COUNT=$(git rev-list --count HEAD 2>/dev/null || echo "0")
    COMMIT_HASH=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
    echo -e "${BLUE}Current build number: ${COMMIT_COUNT} (commit ${COMMIT_HASH})${NC}"
fi

# Check if mingw is installed
if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo -e "${RED}Error: MinGW-w64 not found!${NC}"
    echo "Install it with: sudo apt install mingw-w64"
    exit 1
fi

# Clean build directory if requested
if [[ "$1" == "clean" ]]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf build
fi

# Create build directory
mkdir -p build
cd build

# Configure with MinGW toolchain
echo -e "${YELLOW}Configuring CMake with MinGW toolchain...${NC}"
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../mingw-toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release

# Build
echo -e "${YELLOW}Building (version info will be auto-generated)...${NC}"
make -j$(nproc)

echo -e "${GREEN}Build complete!${NC}"
echo -e "Executable: ${YELLOW}build/gonptechan.exe${NC}"

# Show the generated version info if available
if [ -f build/generated/version.h ]; then
    echo -e "${BLUE}Version info:${NC}"
    grep "define VERSION_WITH_COMMIT" build/generated/version.h | sed 's/.*"\(.*\)"/  \1/' || true
fi
