#!/bin/bash

# CrossExpand Day 3 - Professional Execution Guide
# ===============================================

echo "🚀 CrossExpand Day 3 - Production System"
echo "========================================"
echo

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to print status
print_status() {
    if [ $? -eq 0 ]; then
        echo "✅ $1"
    else
        echo "❌ $1"
        return 1
    fi
}

# Check system requirements
echo "🔍 System Requirements Check:"
echo "-----------------------------"

# Check OS
if [ "$(uname -s)" = "Linux" ]; then
    print_status "Linux OS detected"
else
    echo "❌ Linux OS required"
    exit 1
fi

# Check build tools
command_exists cmake && print_status "CMake available" || { echo "❌ CMake required: sudo apt install cmake"; exit 1; }
command_exists make && print_status "Make available" || { echo "❌ Make required: sudo apt install build-essential"; exit 1; }
command_exists g++ && print_status "C++ compiler available" || { echo "❌ G++ required: sudo apt install g++"; exit 1; }

# Check X11 libraries
if pkg-config --exists x11 xtst; then
    print_status "X11 development libraries available"
else
    echo "❌ X11 libraries required: sudo apt install libx11-dev libxtst-dev"
    exit 1
fi

# Check if already built
if [ -f "build/crossexpand_day3" ]; then
    echo "✅ Executable already built"
    NEED_BUILD=false
else
    echo "🔨 Build required"
    NEED_BUILD=true
fi

echo

# Build if needed
if [ "$NEED_BUILD" = true ]; then
    echo "🔨 Building CrossExpand Day 3:"
    echo "-----------------------------"
    
    # Create build directory
    mkdir -p build
    cd build
    
    # Configure with CMake
    echo "📋 Configuring build..."
    if cmake .. ; then
        print_status "CMake configuration successful"
    else
        echo "❌ CMake configuration failed"
        exit 1
    fi
    
    # Build the project
    echo "🏗️  Building project..."
    if make crossexpand_day3 -j$(nproc); then
        print_status "Build successful"
    else
        echo "❌ Build failed"
        exit 1
    fi
    
    cd ..
else
    echo "⏭️  Skipping build (executable exists)"
fi

echo

# Check executable
echo "🎯 Executable Information:"
echo "-------------------------"
if [ -f "build/crossexpand_day3" ]; then
    ls -lh build/crossexpand_day3
    echo "Size: $(stat -c%s build/crossexpand_day3) bytes"
    echo "Permissions: $(stat -c%A build/crossexpand_day3)"
    
    if [ -x "build/crossexpand_day3" ]; then
        print_status "Executable has proper permissions"
    else
        echo "❌ Executable lacks execute permissions"
        chmod +x build/crossexpand_day3
        print_status "Fixed executable permissions"
    fi
else
    echo "❌ Executable not found"
    exit 1
fi

echo

# Check dependencies
echo "📚 Library Dependencies:"
echo "-----------------------"
if command_exists ldd; then
    echo "Required libraries:"
    ldd build/crossexpand_day3 | grep -E "(libX11|libXtst|libuuid|libpthread|libdl)" | head -10
    echo "..."
    echo
fi

# Port check
echo "🌐 Network Requirements:"
echo "-----------------------"
if netstat -tlnp 2>/dev/null | grep -q ":8080 "; then
    echo "⚠️  Port 8080 is already in use"
    echo "   The web interface may not start properly"
    echo "   Stop other services on port 8080 or configure a different port"
else
    print_status "Port 8080 is available for web interface"
fi

echo

# Execution options
echo "🚀 Execution Options:"
echo "======================"
echo
echo "1. 🖥️  Run with GUI (Recommended):"
echo "   cd build && ./crossexpand_day3"
echo "   Then open: http://localhost:8080"
echo
echo "2. 🐛 Run with debug output:"
echo "   cd build && ./crossexpand_day3 2>&1 | tee crossexpand.log"
echo
echo "3. 🔧 Run in background:"
echo "   cd build && nohup ./crossexpand_day3 > crossexpand.log 2>&1 &"
echo "   Check status: ps aux | grep crossexpand_day3"
echo
echo "4. 📊 Test basic functionality:"
echo "   curl http://localhost:8080/api/status"
echo

# Safety warnings
echo "⚠️  Important Notes:"
echo "==================="
echo "• The application may need elevated privileges for system-wide text injection"
echo "• X11 environment required for text injection features"
echo "• Web interface runs on port 8080 (configurable)"
echo "• Logs are written to ~/.config/crossexpand/logs/"
echo "• Configuration stored in ~/.config/crossexpand/config.json"
echo

# Final execution
echo "🎬 Ready to Launch!"
echo "=================="
echo
read -p "Start CrossExpand Day 3 now? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "🚀 Starting CrossExpand Day 3..."
    echo "Press Ctrl+C to stop"
    echo "Web interface: http://localhost:8080"
    echo
    cd build
    exec ./crossexpand_day3
else
    echo "✨ CrossExpand Day 3 is ready!"
    echo "Run: cd build && ./crossexpand_day3"
    echo "Web interface: http://localhost:8080"
fi
