#!/bin/bash

#set -e  # Exit on any error

# Define installation path
INSTALL_PATH="/usr/local/bin"

echo "Creating build directory..."
mkdir -p build
cd build

echo " Running CMake and compiling..."
cmake ..
make -j$(nproc)

echo "Installing 'dodone' system-wide..."
sudo cp dodone "$INSTALL_PATH"

echo " Installed 'dodone' to $INSTALL_PATH"
echo "You can now run it using: dodone"
echo "NOW YOU CAN SAFELY DELTE THIS FOLDER AS DODONE IS INSTALLED IN SYSTEM-WIDE :)"
