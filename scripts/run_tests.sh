#!/bin/bash

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}=== Gara Test Runner ===${NC}\n"

# Check if build directory exists
if [ ! -d "build" ]; then
    echo -e "${YELLOW}Build directory not found. Creating...${NC}"
    mkdir -p build
fi

cd build

# Configure
echo -e "${YELLOW}Configuring CMake...${NC}"
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build
echo -e "\n${YELLOW}Building project...${NC}"
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Run tests
echo -e "\n${YELLOW}Running tests...${NC}\n"
if ctest --output-on-failure --verbose; then
    echo -e "\n${GREEN}✓ All tests passed!${NC}"
    exit 0
else
    echo -e "\n${RED}✗ Tests failed!${NC}"
    exit 1
fi
