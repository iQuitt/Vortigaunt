#pragma once

/**
 * Platform.h - Cross-Platform Utilities
 * 
 * Abstracts platform-specific operations for windows/Linux compatibility.
 */

#include <string>

namespace Platform
{
    /**
     * Boosts the current process priority for intensive operations.
     * Windows: Sets HIGH_PRIORITY_CLASS
     * Linux: Sets nice value to -10 (if permitted)
     */
    void setHighPriority();

    /**
     * Restores normal process priority.
     * Windows: Restores to NORMAL_PRIORITY_CLASS
     * Linux: Sets nice value to 0
     */
    void restoreNormalPriority();

    /**
     * Detects if system is using dark mode (Windows only)
     */
    bool isSystemDarkMode();

}

