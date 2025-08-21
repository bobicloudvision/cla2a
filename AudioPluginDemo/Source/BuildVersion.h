#pragma once

//==============================================================================
/** Build Version Information
    This file contains the current build number and version information.
    It gets automatically updated by the build script.
*/

// Build counter - gets incremented with each build
#define PLUGIN_BUILD_NUMBER 8

// Version information
#define PLUGIN_VERSION_MAJOR 1
#define PLUGIN_VERSION_MINOR 0
#define PLUGIN_VERSION_PATCH 0

// Build timestamp (updated automatically)
#define PLUGIN_BUILD_DATE __DATE__
#define PLUGIN_BUILD_TIME __TIME__

// Convenience macros
#define PLUGIN_VERSION_STRING "1.0.0"
#define PLUGIN_BUILD_STRING "Build #8"
#define PLUGIN_FULL_VERSION_STRING "v1.0.0 Build #8"

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
