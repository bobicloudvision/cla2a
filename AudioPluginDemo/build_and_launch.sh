#!/bin/bash

# ==============================================================================
# JUCE Compressor Plugin Build & Launch Script
# ==============================================================================
# This script builds the JUCE compressor plugin and automatically launches
# the standalone app after a successful build.
# 
# Usage: ./build_and_launch.sh [option]
# Options:
#   build       - Build the project (default)
#   clean       - Clean build files
#   launch      - Launch standalone app without building
#   projucer    - Open project in Projucer
#   help        - Show this help message
# ==============================================================================

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

# Configuration
PROJECT_NAME="AudioPluginDemo"
PROJECT_FILE="AudioPluginDemo.jucer"
BUILD_DIR="build"
PLUGIN_NAME="AudioPluginDemo"
STANDALONE_APP_NAME="AudioPluginDemo.app"
STANDALONE_EXE_NAME="AudioPluginDemo.exe"

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_build() {
    echo -e "${PURPLE}[BUILD]${NC} $1"
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to detect OS
detect_os() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "linux"
    elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
        echo "windows"
    else
        echo "unknown"
    fi
}

# Function to find the standalone app
find_standalone_app() {
    local OS=$(detect_os)
    local app_path=""
    
    case $OS in
        "macos")
            # Check multiple possible locations
            if [ -d "build/$STANDALONE_APP_NAME" ]; then
                app_path="build/$STANDALONE_APP_NAME"
            elif [ -d "Builds/MacOSX/build/Release/$STANDALONE_APP_NAME" ]; then
                app_path="Builds/MacOSX/build/Release/$STANDALONE_APP_NAME"
            elif [ -d "AudioPluginDemo.xcodeproj/build/Release/$STANDALONE_APP_NAME" ]; then
                app_path="AudioPluginDemo.xcodeproj/build/Release/$STANDALONE_APP_NAME"
            elif [ -d "build/Release/$STANDALONE_APP_NAME" ]; then
                app_path="build/Release/$STANDALONE_APP_NAME"
            fi
            ;;
        "windows")
            if [ -f "build/$STANDALONE_EXE_NAME" ]; then
                app_path="build/$STANDALONE_EXE_NAME"
            elif [ -f "build/Release/$STANDALONE_EXE_NAME" ]; then
                app_path="build/Release/$STANDALONE_EXE_NAME"
            fi
            ;;
        "linux")
            if [ -f "build/$PROJECT_NAME" ]; then
                app_path="build/$PROJECT_NAME"
            elif [ -f "build/Release/$PROJECT_NAME" ]; then
                app_path="build/Release/$PROJECT_NAME"
            fi
            ;;
    esac
    
    echo "$app_path"
}

# Function to launch the standalone app
launch_standalone_app() {
    local app_path=$(find_standalone_app)
    local OS=$(detect_os)
    
    if [ -z "$app_path" ]; then
        print_error "Standalone app not found! Please build the project first."
        return 1
    fi
    
    print_status "Launching standalone app: $app_path"
    
    case $OS in
        "macos")
            if [ -d "$app_path" ]; then
                open "$app_path"
                print_success "Standalone app launched!"
            else
                print_error "App bundle not found: $app_path"
                return 1
            fi
            ;;
        "windows")
            if [ -f "$app_path" ]; then
                start "" "$app_path"
                print_success "Standalone app launched!"
            else
                print_error "Executable not found: $app_path"
                return 1
            fi
            ;;
        "linux")
            if [ -f "$app_path" ]; then
                "$app_path" &
                print_success "Standalone app launched!"
            else
                print_error "Executable not found: $app_path"
                return 1
            fi
            ;;
    esac
}

# Function to build with Projucer
build_projucer() {
    print_status "Building with Projucer..."
    
    # Try to find Projucer in common locations
    local projucer_path=""
    
    if command_exists Projucer; then
        projucer_path="Projucer"
    elif [ -d "/Applications/JUCE/Projucer.app" ]; then
        projucer_path="/Applications/JUCE/Projucer.app/Contents/MacOS/Projucer"
    elif [ -f "/Applications/JUCE/extras/Projucer/Builds/MacOSX/build/Release/Projucer" ]; then
        projucer_path="/Applications/JUCE/extras/Projucer/Builds/MacOSX/build/Release/Projucer"
    else
        print_error "Projucer not found! Please install JUCE and Projucer first."
        print_status "You can download it from: https://juce.com/get-juce/"
        return 1
    fi
    
    # Create build directory
    mkdir -p "$BUILD_DIR"
    
    # Open project in Projucer
    print_status "Opening project in Projucer using: $projucer_path"
    "$projucer_path" "$PROJECT_FILE" &
    
    print_success "Project opened in Projucer!"
    print_status "Please save the project in Projucer to generate build files."
    print_status "Then run: ./build_and_launch.sh build"
}

# Function to build with Xcode (macOS)
build_xcode() {
    print_build "Building with Xcode..."
    
    # Check for Xcode project in common locations
    local xcode_project=""
    
    if [ -d "AudioPluginDemo.xcodeproj" ]; then
        xcode_project="AudioPluginDemo.xcodeproj"
    elif [ -d "Builds/MacOSX/AudioPluginDemo.xcodeproj" ]; then
        xcode_project="Builds/MacOSX/AudioPluginDemo.xcodeproj"
    else
        print_error "Xcode project not found! Please run Projucer first to generate build files."
        return 1
    fi
    
    # Build the project
    print_status "Building project: $xcode_project"
    xcodebuild -project "$xcode_project" -scheme "AudioPluginDemo - Standalone Plugin" -configuration Release
    
    if [ $? -eq 0 ]; then
        print_success "Build completed successfully!"
        return 0
    else
        print_error "Build failed!"
        return 1
    fi
}

# Function to build with Make (Linux)
build_make() {
    print_build "Building with Make..."
    
    if [ ! -f "Makefile" ]; then
        print_error "Makefile not found! Please run Projucer first to generate build files."
        return 1
    fi
    
    # Build the project
    print_status "Building project..."
    make -j$(nproc)
    
    if [ $? -eq 0 ]; then
        print_success "Build completed successfully!"
        return 0
    else
        print_error "Build failed!"
        return 1
    fi
}

# Function to build with CMake
build_cmake() {
    print_build "Building with CMake..."
    
    if ! command_exists cmake; then
        print_error "CMake not found! Please install CMake first."
        return 1
    fi
    
    # Create build directory
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure with CMake
    print_status "Configuring with CMake..."
    cmake .. -DJUCE_BUILD_EXAMPLES=ON -DJUCE_BUILD_EXTRAS=ON
    
    # Build
    print_status "Building project..."
    cmake --build . --config Release -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    
    cd ..
    
    if [ $? -eq 0 ]; then
        print_success "CMake build completed successfully!"
        return 0
    else
        print_error "CMake build failed!"
        return 1
    fi
}

# Function to build the project
build_project() {
    local OS=$(detect_os)
    
    print_status "Detected OS: $OS"
    
    case $OS in
        "macos")
            build_xcode
            ;;
        "linux")
            build_make
            ;;
        "windows")
            print_warning "Windows build not implemented yet. Please use Visual Studio."
            return 1
            ;;
        *)
            print_error "Unsupported operating system: $OS"
            return 1
            ;;
    esac
}

# Function to clean build files
clean_build() {
    print_status "Cleaning build files..."
    
    # Remove build directories
    rm -rf "$BUILD_DIR"
    rm -rf "AudioPluginDemo.xcodeproj"
    rm -rf "AudioPluginDemo.xcworkspace"
    rm -f "Makefile"
    rm -rf "obj"
    rm -rf "bin"
    
    print_success "Build files cleaned!"
}

# Function to show help
show_help() {
    echo "JUCE Compressor Plugin Build & Launch Script"
    echo "============================================="
    echo ""
    echo "Usage: ./build_and_launch.sh [option]"
    echo ""
    echo "Options:"
    echo "  build       - Build the project and launch standalone app (default)"
    echo "  clean       - Clean build files"
    echo "  launch      - Launch standalone app without building"
    echo "  projucer    - Open project in Projucer"
    echo "  help        - Show this help message"
    echo ""
    echo "Examples:"
    echo "  ./build_and_launch.sh              # Build and launch"
    echo "  ./build_and_launch.sh build        # Build and launch"
    echo "  ./build_and_launch.sh launch       # Launch only"
    echo "  ./build_and_launch.sh projucer     # Open in Projucer"
    echo "  ./build_and_launch.sh clean        # Clean build files"
    echo ""
    echo "Build Process:"
    echo "  1. Run './build_and_launch.sh' to build and launch"
    echo "  2. Or run './build_and_launch.sh build' for the same effect"
    echo "  3. Use './build_and_launch.sh launch' to just launch the app"
    echo ""
}

# Function to check dependencies
check_dependencies() {
    print_status "Checking dependencies..."
    
    local missing_deps=()
    
    # Check for JUCE
    if [ ! -f "$PROJECT_FILE" ]; then
        missing_deps+=("JUCE project file ($PROJECT_FILE)")
    fi
    
    # Check for source files
    if [ ! -f "Source/AudioPluginDemo.h" ]; then
        missing_deps+=("Source files")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing dependencies:"
        for dep in "${missing_deps[@]}"; do
            echo "  - $dep"
        done
        return 1
    fi
    
    print_success "All dependencies found!"
    return 0
}

# Main build and launch function
build_and_launch() {
    print_status "Starting build and launch process for $PROJECT_NAME..."
    
    # Check dependencies
    if ! check_dependencies; then
        return 1
    fi
    
    # Increment build number
    if [ -f "increment_build.sh" ]; then
        print_status "Incrementing build number..."
        ./increment_build.sh
    else
        print_warning "increment_build.sh not found, skipping build number increment"
    fi
    
    # Build the project
    if build_project; then
        print_success "Build completed! Launching standalone app..."
        
        # Wait a moment for files to be written
        sleep 2
        
        # Launch the standalone app
        if launch_standalone_app; then
            print_success "Build and launch completed successfully! 🎉"
            return 0
        else
            print_error "Failed to launch standalone app!"
            return 1
        fi
    else
        print_error "Build failed! Cannot launch standalone app."
        return 1
    fi
}

# Main script
main() {
    print_status "Starting build process for $PROJECT_NAME..."
    
    # Parse command line arguments
    case "${1:-build}" in
        "build")
            build_and_launch
            ;;
        "clean")
            clean_build
            ;;
        "launch")
            launch_standalone_app
            ;;
        "projucer")
            build_projucer
            ;;
        "help"|"-h"|"--help")
            show_help
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            return 1
            ;;
    esac
}

# Run main function
main "$@"
