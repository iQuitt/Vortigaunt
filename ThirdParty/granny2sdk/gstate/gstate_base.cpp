// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_base.h"

#include "gstate_token_context.h"
#include <stdarg.h>
#include <stdio.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

namespace
{
    GStateAssertionHandler* g_AssertionHandler = 0;
    gstate_random_callback  g_RandomCallback = { 0, 0 };

    granny_uint32 KISS99()
    {
        static granny_uint32 x = 123456789,y = 362436000,z = 521288629,c = 7654321; // seeds

        x = 69069*x+12345;

        y ^= (y<<13);
        y ^= (y>>17);
        y ^= (y<<5);

        granny_uint64x t = c;
        t += (698769069ULL*z);
        c = (granny_uint32)(t>>32);
        z = (granny_uint32)t;

        return (x+y+z);
    }
}

void
GStateGetRandomCallback(gstate_random_callback* CurrentCallback)
{
    GStateCheckPtrNotNULL(CurrentCallback, return);

    *CurrentCallback = g_RandomCallback;
}


void
GStateOverrideRandomCallback(gstate_random_callback* Handler)
{
    if (Handler != 0)
    {
        g_RandomCallback = *Handler;
    }
    else
    {
        g_RandomCallback.Generator = 0;
        g_RandomCallback.UserData = 0;
    }
}


GStateAssertionHandler*
GStateGetAssertionHandler()
{
    return g_AssertionHandler;
}

void
GStateOverrideAssertionHandler(GStateAssertionHandler* Handler)
{
    g_AssertionHandler = Handler;
}


bool GSTATE_CALLBACK GSTATE
HandleAssertion(char const* Condition,
                char const* File,
                int Line)
{
    if (g_AssertionHandler != 0)
    {
        return (*g_AssertionHandler)(Condition, File, Line);
    }

    return false;
}

granny_real32 GSTATE
RandomUnit()
{
    granny_real32 RetVal;
    if (g_RandomCallback.Generator != 0)
    {
        RetVal = (*g_RandomCallback.Generator)(g_RandomCallback.UserData);
    }
    else
    {
        RetVal = KISS99() / granny_real32(granny_uint32(0xffffffff));
    }
    GStateAssert(RetVal >= 0 && RetVal <= 1.0f);

    return RetVal;
}

void* GSTATE
GStateAllocDriver(char const* File, int Line, size_t Size)
{
    granny_allocate_callback* Alloc;
    granny_deallocate_callback* Ignore;
    GrannyGetAllocator(&Alloc, &Ignore);
    return (*Alloc)(File, Line, GrannyDefaultAllocationAlignment, Size, GrannyAllocationUnknown);
}

void GSTATE
GStateDeallocateDriver(char const* File, int Line, void* Ptr)
{
    if (Ptr)
    {
        granny_allocate_callback* Ignore;
        granny_deallocate_callback* Dealloc;
        GrannyGetAllocator(&Ignore, &Dealloc);
        (*Dealloc)(File, Line, Ptr);
    }
}

void GSTATE
GStateLogCallback(char const* File, int Line,
                  granny_log_message_type Type,
                  char const* Message, ...)
{
    va_list args;
    va_start(args, Message);

    char MessageBuffer[512];
#if !UNSAFE_VSNPRINTF
    vsnprintf_s(MessageBuffer, sizeof(MessageBuffer), _TRUNCATE, Message, args);
#else
    vsnprintf(MessageBuffer, sizeof(MessageBuffer), Message, args);
#endif

    va_end(args);

    granny_log_callback Callback;
    GrannyGetLogCallback(&Callback);
    if (Callback.Function)
    {
        (*Callback.Function)(Type, GrannyGStateLogMessage,
                             File, Line,
                             MessageBuffer, Callback.UserData);
    }
}
