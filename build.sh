#!/bin/bash
echo "ESP32-Agata Robot Build Script"
echo "==============================="

# Set the target to ESP32-C6
echo "Setting target to ESP32-C6..."
idf.py set-target esp32c6

# Clean previous build
echo "Cleaning previous build..."
idf.py fullclean

# Build the project
echo "Building project..."
idf.py build

echo "Build completed!"
