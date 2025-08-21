#!/bin/bash

# ==============================================================================
# Build Number Incrementer Script
# ==============================================================================
# This script automatically increments the build number in BuildVersion.h
# and updates the version strings accordingly.

BUILD_VERSION_FILE="Source/BuildVersion.h"

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${BLUE}[BUILD]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

# Check if BuildVersion.h exists
if [ ! -f "$BUILD_VERSION_FILE" ]; then
    echo "Error: $BUILD_VERSION_FILE not found!"
    exit 1
fi

# Extract current build number
CURRENT_BUILD=$(grep "#define PLUGIN_BUILD_NUMBER" "$BUILD_VERSION_FILE" | awk '{print $3}')

if [ -z "$CURRENT_BUILD" ]; then
    echo "Error: Could not extract current build number!"
    exit 1
fi

# Increment build number
NEW_BUILD=$((CURRENT_BUILD + 1))

print_status "Incrementing build number from $CURRENT_BUILD to $NEW_BUILD"

# Get current timestamp
BUILD_DATE=$(date +"%b %d %Y")
BUILD_TIME=$(date +"%H:%M:%S")

# Create updated BuildVersion.h content
cat > "$BUILD_VERSION_FILE" << EOF
#pragma once

//==============================================================================
/** Build Version Information
    This file contains the current build number and version information.
    It gets automatically updated by the build script.
*/

// Build counter - gets incremented with each build
#define PLUGIN_BUILD_NUMBER $NEW_BUILD

// Version information
#define PLUGIN_VERSION_MAJOR 1
#define PLUGIN_VERSION_MINOR 0
#define PLUGIN_VERSION_PATCH 0

// Build timestamp (updated automatically)
#define PLUGIN_BUILD_DATE __DATE__
#define PLUGIN_BUILD_TIME __TIME__

// Convenience macros
#define PLUGIN_VERSION_STRING "1.0.0"
#define PLUGIN_BUILD_STRING "Build #$NEW_BUILD"
#define PLUGIN_FULL_VERSION_STRING "v1.0.0 Build #$NEW_BUILD"

//==============================================================================
/** Build Version Utilities */
class BuildVersion
{
public:
    static int getBuildNumber() { return PLUGIN_BUILD_NUMBER; }
    static const char* getVersionString() { return PLUGIN_VERSION_STRING; }
    static const char* getBuildString() { return PLUGIN_BUILD_STRING; }
    static const char* getFullVersionString() { return PLUGIN_FULL_VERSION_STRING; }
    static const char* getBuildDate() { return PLUGIN_BUILD_DATE; }
    static const char* getBuildTime() { return PLUGIN_BUILD_TIME; }
    
    /** Get formatted build info for display */
    static String getDisplayString()
    {
        return String("v") + String(PLUGIN_VERSION_MAJOR) + "." + 
               String(PLUGIN_VERSION_MINOR) + "." + 
               String(PLUGIN_VERSION_PATCH) + 
               " Build #" + String(PLUGIN_BUILD_NUMBER);
    }
    
    /** Get detailed build info */
    static String getDetailedString()
    {
        return getDisplayString() + "\nBuilt: " + String(PLUGIN_BUILD_DATE) + " " + String(PLUGIN_BUILD_TIME);
    }
};
EOF

print_success "Build number updated to #$NEW_BUILD"
print_status "Build timestamp: $BUILD_DATE $BUILD_TIME"
