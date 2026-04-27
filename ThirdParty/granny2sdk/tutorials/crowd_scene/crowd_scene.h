//-----------------------------------------------------------------------------
// File: crowd_scene.h
//
// Desc: Header file CrowdScene sample app
//-----------------------------------------------------------------------------
#pragma once

#include "granny.h"

#ifdef _DEBUG

// Allow ASSERT(false) or compile-constant asserts (sizeof(x) == sizeof(y))
//  without warning
#pragma warning(disable : 4127)
#define ASSERT(val) if ( !(val) ) { DebugBreak(); }
#define FAIL(reason) DebugBreak()

#else
#define ASSERT(val)  sizeof(val)
#define FAIL(reason) sizeof(reason)
#endif



//-----------------------------------------------------------------------------
// Defines, and constants
//-----------------------------------------------------------------------------

