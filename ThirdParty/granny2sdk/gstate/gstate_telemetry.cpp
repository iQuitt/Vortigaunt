// ========================================================================
// $File: //jeffr/gstate_29/rt/gstate_telemetry.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "gstate_telemetry.h"

// Should always be the last header included
#include "gstate_cpp_settings.h"
// ***VERSION_CHECK***

#define SubsystemCode Undefined_LogMessage
USING_GSTATE_NAMESPACE;

#if GSTATE_TELEMETRY3

extern "C" { unsigned int global_GState_TmContext; }

#elif defined(GSTATE_TELEMETRY) && GSTATE_TELEMETRY

extern "C" { void* global_GState_TmContext; }

#endif


#if GSTATE_TELEMETRY3
static bool
VersionCheck()
{
    if (tmCheckVersion(TelemetryMajorVersion,
        TelemetryMinorVersion,
        TelemetryBuildNumber,
        TelemetryCustomization ) != TM_OK)
    {
        GStateError("Mismatched GState/Telemetry versions.");
        global_GState_TmContext = 0;

        return false;
    }

    return true;
}
#elif defined(GSTATE_TELEMETRY) && GSTATE_TELEMETRY
static bool
VersionCheck()
{
    if (global_GState_TmContext &&
        tmCheckVersion((HTELEMETRY)global_GState_TmContext,
                       TelemetryMajorVersion,
                       TelemetryMinorVersion,
                       TelemetryBuildNumber,
                       TelemetryCustomization ) != TM_OK)
    {
        GStateError("Mismatched GState/Telemetry versions.");
        global_GState_TmContext = 0;

        return false;
    }

    return true;
}
#endif


/*
bool GStateUseTmLite(void* Context)
{
#if GSTATE_TELEMETRY3
    GS_InvalidCodePath("GStateUseTmLite called, but GSTATE_TELEMETRY3 is set. You should be calling GStateUseTelemetry() instead!");
#elif defined(GSTATE_TELEMETRY) && GSTATE_TELEMETRY
#if GSTATE_REAL_TELEMETRY
    GS_InvalidCodePath("GStateUseTmLite called, but GSTATE_REAL_TELEMETRY is set. You should be calling GStateUseTelemetry() instead!");
#else
    global_GState_TmContext = TM_CONTEXT_LITE( Context );
#endif

    return VersionCheck();
#else
    return false;

#endif
}
*/

bool GStateUseTelemetry(void* Context)
{
#if GSTATE_TELEMETRY3

    global_GState_TmContext = (unsigned int)Context;

    return VersionCheck();

#elif defined(GSTATE_TELEMETRY) && GSTATE_TELEMETRY

#if GSTATE_REAL_TELEMETRY
    global_GState_TmContext = Context;
#else
    global_GState_TmContext = TM_CONTEXT_FULL( Context );
#endif

    return VersionCheck();
#else
    return false;
#endif
}

