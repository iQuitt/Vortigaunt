#pragma once

/**
 * VortigauntLog.h - Unified Logging System
 * 
 * Features:
 * - Persistent File Logging (Vortigaunt.log)
 * - Quake-style Color Codes (^1 Red, ^2 Green, etc.)
 * - Timestamped Output
 * - Thread-safe
 */

#include <string>
#include <cstdarg>
#include <type_traits>

#ifdef QT_CORE_LIB
#include <QString>
class QPlainTextEdit;
#endif

namespace VortigauntLog
{
    // Log levels for filtering
    enum class LogLevel {
        Info,
        Warning,
        Error
    };

    // --- Configuration ---
    void Initialize(const std::string& logFileName = "VortigauntTool.log");
    
#ifdef QT_CORE_LIB
    void addLogWidget(QPlainTextEdit* widget);
#endif

    void setDeveloperMode(bool enabled);
    bool isDeveloperMode();
    
    // --- Logging Functions ---
    // Log level is auto-detected from message prefix:
    //   ERROR: or [ERROR]   -> Red (^1)
    //   WARNING: or [WARNING] -> Yellow (^3)
    //   DEBUG:               -> Gray (^9), developer mode only
    //   INFO:                -> Default color

    // Standard string log
    void Vortigaunt_Printf(const std::string& message);

    // C-string overloads
    void Vortigaunt_Printf(const char* message);
    
#ifdef QT_CORE_LIB
    // QString overloads
    void Vortigaunt_Printf(const QString& message);
    
    void Vortigaunt_Printf(QPlainTextEdit* logWidget, const QString& message);
#endif

    //  Formatted Logging (LogF) 
    
#ifdef QT_CORE_LIB
    void LogInternal(const std::string& msg, bool debugOnly, QPlainTextEdit* targetWidget = nullptr);
#else
    void LogInternal(const std::string& msg, bool debugOnly);
#endif
    inline void LogF(const char* fmt, ...)
    {
        char buffer[4096];
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        
        LogInternal(std::string(buffer), false);
    }

}

// Bring LogLevel into global scope
using VortigauntLog::LogLevel;
