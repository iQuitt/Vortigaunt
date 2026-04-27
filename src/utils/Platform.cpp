#include "Platform.h"

#ifdef _WIN32
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#else
#include <unistd.h>
#include <sys/resource.h>
#endif

namespace Platform
{

    // Store original priority to restore later
#ifdef _WIN32
    static DWORD s_originalPriority = NORMAL_PRIORITY_CLASS;
#else
    static int s_originalNice = 0;
#endif

    void setHighPriority()
    {
#ifdef _WIN32
        HANDLE hProcess = GetCurrentProcess();
        s_originalPriority = GetPriorityClass(hProcess);
        SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS);
#else
        // Save current nice value
        s_originalNice = getpriority(PRIO_PROCESS, 0);
        // Try to set higher priority (might fail without root)
        nice(-10);
#endif
    }

    void restoreNormalPriority()
    {
#ifdef _WIN32
        HANDLE hProcess = GetCurrentProcess();
        SetPriorityClass(hProcess, s_originalPriority);
#else
        // Restore original nice value
        nice(s_originalNice - getpriority(PRIO_PROCESS, 0));
#endif
    }



    // If the system theme is dark, the Qt application will also be dark. 
    // The reason i created this function is to make the text appear clearer.
    bool isSystemDarkMode() {
#ifdef _WIN32 
        bool isDark = false;
        HKEY hKey;
        const char* subKey = "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
        const char* valueName = "AppsUseLightTheme";

        if (RegOpenKeyExA(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD value = 0;
            DWORD valueSize = sizeof(value);
            if (RegQueryValueExA(hKey, valueName, nullptr, nullptr, (LPBYTE)&value, &valueSize) == ERROR_SUCCESS) {
                // 0 = Dark, 1 = Light
                isDark = (value == 0);
            }
            RegCloseKey(hKey);
        }
        return isDark;
#else
        // TODO: Detect system theme for linux.
        return false;
#endif
    }
}// namespace Platform

