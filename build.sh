#!/bin/bash
# Smart Resource Scheduler Build Script
# This script automates the build process
set -e # Exit on error

echo "====================================="
echo "Smart Resource Scheduler Build Script"
echo "====================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if running as root (we don't want to build as root)
if [ "$EUID" -eq 0 ]; then
    echo -e "${RED}Error: Do not run the build script as root!${NC}"
    echo "Build the project as a normal user, then run the application with sudo."
    exit 1
fi

# Check for required tools
echo "Checking dependencies..."
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}Error: cmake not found${NC}"
    echo "Install with: sudo apt-get install cmake"
    exit 1
fi
if ! command -v g++ &> /dev/null; then
    echo -e "${RED}Error: g++ not found${NC}"
    echo "Install with: sudo apt-get install build-essential"
    exit 1
fi
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}Error: python3 not found${NC}"
    echo "Install with: sudo apt-get install python3"
    exit 1
fi

# Check for Python headers
if ! python3-config --includes &> /dev/null; then
    echo -e "${RED}Error: Python development headers not found${NC}"
    echo "Install with: sudo apt-get install python3-dev"
    exit 1
fi

# Check for pybind11
if ! python3 -c "import pybind11" 2>/dev/null; then
    echo -e "${YELLOW}Warning: pybind11 not found${NC}"
    echo "Installing pybind11..."
    pip3 install --user pybind11
fi

# Check for matplotlib
if ! python3 -c "import matplotlib" 2>/dev/null; then
    echo -e "${YELLOW}Warning: matplotlib not found${NC}"
    echo "Installing matplotlib..."
    pip3 install --user matplotlib
fi

echo -e "${GREEN}All dependencies found${NC}"
echo ""

# Clean previous build
if [ -d "build" ]; then
    echo "Cleaning previous build..."
    rm -rf build
fi

# Create build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Run CMake
echo ""
echo "Running CMake..."
if cmake .. ; then
    echo -e "${GREEN}CMake configuration successful${NC}"
else
    echo -e "${RED}CMake configuration failed${NC}"
    exit 1
fi

# Build
echo ""
echo "Building project..."
if make -j$(nproc); then
    echo -e "${GREEN}Build successful${NC}"
else
    echo -e "${RED}Build failed${NC}"
    exit 1
fi

# === FINAL STEP: Module is already in gui/ thanks to CMake ===
cd ..
echo ""
echo -e "${GREEN}Python module is already in gui/:${NC}"
SO_FILE=$(find gui -name "scheduler_module*.so" -type f | head -n 1)
if [ -n "$SO_FILE" ]; then
    echo "  $(basename "$SO_FILE")"
else
    echo -e "${RED}Warning: Module not found in gui/!${NC}"
    # Fallback: try build/
    SO_FILE=$(find build -name "scheduler_module*.so" -type f | head -n 1)
    if [ -n "$SO_FILE" ]; then
        mkdir -p gui
        cp "$SO_FILE" gui/
        echo -e "${GREEN}Copied from build/ to gui/${NC}"
    else
        echo -e "${RED}No .so file found anywhere!${NC}"
        exit 1
    fi
fi

# Check if dashboard.py exists
if [ ! -f "gui/dashboard.py" ]; then
    echo -e "${YELLOW}Warning: gui/dashboard.py not found${NC}"
    echo "Please make sure dashboard.py is in the gui/ directory"
fi

echo ""
echo "====================================="
echo -e "${GREEN}Build completed successfully!${NC}"
echo "====================================="
echo ""
echo "To run the application:"
echo " cd gui"
echo " sudo python3 dashboard.py"
echo ""
echo "Note: Root privileges are required for process management."
echo ""