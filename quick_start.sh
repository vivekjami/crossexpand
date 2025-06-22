#!/bin/bash

# CrossExpand Day 3 - Quick Demo
echo "🎯 CrossExpand Day 3 - Quick Demo"
echo "================================="
echo

# Check if built
if [ ! -f "build/crossexpand_day3" ]; then
    echo "🔨 Building CrossExpand Day 3..."
    mkdir -p build && cd build
    if cmake .. && make crossexpand_day3 -j$(nproc); then
        echo "✅ Build successful!"
        cd ..
    else
        echo "❌ Build failed"
        exit 1
    fi
fi

echo "📋 System Information:"
echo "  Executable: $(ls -lh build/crossexpand_day3 | awk '{print $5, $9}')"
echo "  Platform: $(uname -s) $(uname -m)"
echo "  Compiler: $(g++ --version | head -1)"
echo

echo "🚀 Starting CrossExpand Day 3..."
echo "  Web Interface: http://localhost:8080"
echo "  Press Ctrl+C to stop"
echo

# Check for X11
if [ -z "$DISPLAY" ]; then
    echo "⚠️  No X11 display detected - text injection features may be limited"
fi

echo "Starting in 3 seconds..."
sleep 3

cd build && ./crossexpand_day3
