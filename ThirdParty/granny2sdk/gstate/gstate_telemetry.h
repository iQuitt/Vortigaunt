// ========================================================================
// $Notice: $
// ========================================================================
#if !defined(GSTATE_TELEMETRY_H)
#include "gstate_header_prefix.h"

// Pertinent macros:
//
//  GSTATE_TELEMETRY 0/1
//    GSTATE_DEBUG_MEMORY_CALLSTACK 0/1

#if !defined(GSTATE_BASE_H)
#include "gstate_base.h"
#endif


#if GSTATE_TELEMETRY3
#include "rad_tm.h"

#define GSTATE_AUTO_ZONE(...)  tmZone(0, TMZF_NONE, ## __VA_ARGS__)
#define GSTATE_ENTER_ZONE(ZoneName) do { tmEnter(0, TMZF_NONE, #ZoneName); } while (false)

#define GSTATE_AUTO_IDLE_ZONE(name)   tmZone(0,TMZF_IDLE,#name)

#define GSTATE_AUTO_ZONE_FN()       GSTATE_AUTO_ZONE(__FUNCTION__)
#define GSTATE_ENTER_ZONE_FN()      do { tmEnter(0, __FUNCTION__,0,TMZF_NONE); } while (false)

#if GSTATE_TELEMETRY_NODE_NAMES
   #define GSTATE_AUTO_ZONE_FN_OR_NAME() GSTATE_AUTO_ZONE(GetName())
#else
    #define GSTATE_AUTO_ZONE_FN_OR_NAME() GSTATE_AUTO_ZONE_FN()
#endif

#define GSTATE_LEAVE_ZONE()         do { tmLeave(0); } while (false)

#define GSTATE_INC_INT_ACCUMULATOR(Var) ++(Var)

#define GSTATE_EMIT_INT_VALUE(Name, Var)                                            \
    do {                                                                            \
    tmPlot(0,TMPT_INTEGER,TMPF_NONE,(double)(Var),(Name)); \
    } while (false)

#define GSTATE_EMIT_FLOAT(Name, Var)                                        \
    do {                                                                    \
    tmPlot(0,TMPT_NONE,TMPF_NONE,(Var),(Name));  \
    } while (false)

#define GSTATE_EMIT_INT_ACCUMULATOR(Name, Var)                                      \
    do {                                                                            \
    tmPlot(0,TMPT_INTEGER,TMPF_NONE,(double)(Var),(Name));   \
    (Var) = 0;                                                                  \
    } while (false)

#define GSTATE_TM_MESSAGE(MsgFlag, Fmt, ...)                \
    tmMessage(0, MsgFlag, Fmt, ## __VA_ARGS__)

#define GSTATE_TM_LOG(Fmt, ...)                                         \
    tmMessage(0,  TMMF_SEVERITY_LOG, Fmt, ## __VA_ARGS__)


#elif defined(GSTATE_TELEMETRY) && GSTATE_TELEMETRY

#if defined(GSTATE_REAL_TELEMETRY)
#include "telemetry.h"
#else
#include "TmAPI.h"
#endif

extern "C" { extern bool global_GState_UseNames; }
#define GSTATE_TELEMETRY_SETUSENAMES(val) (global_GState_UseNames = val)
#define GSTATE_TELEMETRY_GETUSENAMES() global_GState_UseNames

extern "C" { extern void* global_GState_TmContext; }
#define GStateTMContext ((HTELEMETRY)global_GState_TmContext)

#define GSTATE_AUTO_ZONE(ZoneName)  tmZone((HTELEMETRY)global_GState_TmContext,TMZF_NONE,#ZoneName)
#define GSTATE_ENTER_ZONE(ZoneName) do { tmEnter((HTELEMETRY)global_GState_TmContext, TMZF_NONE, #ZoneName); } while (false)

#define GSTATE_AUTO_IDLE_ZONE(name)   tmZone((HTELEMETRY)global_GState_TmContext,TMZF_IDLE,#name)

#define GSTATE_AUTO_ZONE_FN()       tmZone((HTELEMETRY)global_GState_TmContext,TMZF_NONE,__FUNCTION__)
#define GSTATE_ENTER_ZONE_FN()      do { tmEnter((HTELEMETRY)global_GState_TmContext, __FUNCTION__,0,TMZF_NONE); } while (false)

#if GSTATE_AUTO_ZONE_USE_NAMES
#define GSTATE_AUTO_ZONE_FN_OR_NAME() GSTATE_AUTO_ZONE(GetName())
#else
#define GSTATE_AUTO_ZONE_FN_OR_NAME() GSTATE_AUTO_ZONE_FN()
#endif

#define GSTATE_LEAVE_ZONE()         do { tmLeave((HTELEMETRY)global_GState_TmContext); } while (false)

#define GSTATE_INC_INT_ACCUMULATOR(Var) ++(Var)

#define GSTATE_EMIT_INT_VALUE(Name, Var)                                            \
    do {                                                                            \
        tmPlotI32((HTELEMETRY)global_GState_TmContext,TMPT_INTEGER,TMPF_NONE,(Var),(Name)); \
    } while (false)

#define GSTATE_EMIT_FLOAT(Name, Var)                                        \
    do {                                                                    \
        tmPlotF64((HTELEMETRY)global_GState_TmContext,TMPT_NONE,TMPF_NONE,(Var),(Name));  \
    } while (false)

#define GSTATE_EMIT_INT_ACCUMULATOR(Name, Var)                                      \
    do {                                                                            \
        tmPlotI32((HTELEMETRY)global_GState_TmContext,TMPT_INTEGER,TMPF_NONE,(Var),(Name));   \
        (Var) = 0;                                                                  \
    } while (false)

#define GSTATE_TM_MESSAGE(MsgFlag, Fmt, ...)                \
    tmMessage((HTELEMETRY)global_GState_TmContext, MsgFlag, Fmt, ## __VA_ARGS__)

#define GSTATE_TM_LOG(Fmt, ...)                                         \
    tmMessage((HTELEMETRY)global_GState_TmContext,  TMMF_SEVERITY_LOG, Fmt, ## __VA_ARGS__)

#else // GSTATE_TELEMETRY==0 || !defined(GSTATE_TELEMETRY)

#define GSTATE_AUTO_ZONE(...)
#define GSTATE_AUTO_ZONE_FN() (void)sizeof(void*)
#define GSTATE_ENTER_ZONE(ZoneName) (void)sizeof(#ZoneName)
#define GSTATE_ENTER_ZONE_FN() (void)sizeof(void*)
#define GSTATE_LEAVE_ZONE() (void)sizeof(void*)
#define GSTATE_AUTO_ZONE_FN() (void)sizeof(void*)
#define GSTATE_AUTO_ZONE_FN_OR_NAME() (void)sizeof(void*)

#define GSTATE_AUTO_IDLE_ZONE(name) (void)sizeof(void*)

#define GSTATE_INC_INT_ACCUMULATOR(Var) (void)sizeof(Var)
#define GSTATE_EMIT_INT_VALUE(Name, Var) (void)sizeof(Var)
#define GSTATE_EMIT_FLOAT(Name, Var) (void)sizeof(Var)
#define GSTATE_EMIT_INT_ACCUMULATOR(Name, Var) (void)sizeof(Var)

#if !defined(COMPILER_SUPPORTS_VAR_MACRO) || !COMPILER_SUPPORTS_VAR_MACRO
// These will cause a warning, but it works.
#define GSTATE_TM_MESSAGE(MsgFlag, Fmt) (void)sizeof(Fmt)
#define GSTATE_TM_LOG(Fmt) (void)sizeof(Fmt)
#else
#define GSTATE_TM_MESSAGE(MsgFlag, Fmt, ...) (void)sizeof(Fmt)
#define GSTATE_TM_LOG(Fmt, ...) (void)sizeof(Fmt)
#endif

#endif // GSTATE_TELEMETRY

// What is the difference between a Value and an Accumulator?  Glad you asked!  A value is
// persisted across frames, and may affect behavior.  (Think "CurrentBindings" in the
// animation cache, which affects what happens on Release().)  These are incremented even
// when GSTATE_TELEMETRY is disabled.
// @@ atomic
#define GSTATE_ADD_INT_VALUE(Var, addend)  (Var) += (addend)
#define GSTATE_SUB_INT_VALUE(Var, minuend) (Var) -= (minuend)
#define GSTATE_INC_INT_VALUE(Var) GSTATE_ADD_INT_VALUE(Var, 1)
#define GSTATE_DEC_INT_VALUE(Var) GSTATE_SUB_INT_VALUE(Var, 1)

// @@ #evals
#define GSTATE_NOTE_INT_MAXVALUE(Var, i) ((Var) = (Var) < (i) ? (i) : (Var))


// Activate telemetry for GState
bool GStateUseTmLite(void* Context);
bool GStateUseTelemetry(void* Context);

#include "gstate_header_postfix.h"
#define GSTATE_TELEMETRY_H
#endif /* GSTATE_TELEMETRY_H */
