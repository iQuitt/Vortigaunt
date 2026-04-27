// ========================================================================
// $Notice: $
// ========================================================================
#if !defined(GSTATE_NODE_MACROS_H)

// Handy macros
#define INPUT_CONNECTION_ON(ptr, EdgeIdx, VarPrefix)                            \
    node* VarPrefix ## Node = 0;                                                \
    granny_int32x VarPrefix ## Edge = -1;                                       \
    (ptr)->GetInputConnection((EdgeIdx), &(VarPrefix##Node), &(VarPrefix##Edge))

#define INPUT_CONNECTION(EdgeIdx, VarPrefix) INPUT_CONNECTION_ON(this, EdgeIdx, VarPrefix)

#define DECL_SAMPLEPOSE_INTERFACE()                                                                  \
    public:                                                                                     \
    virtual granny_real32 GetDuration(granny_int32x OnOutput, granny_real32 AtT);               \
    virtual bool DidLoopOccur(granny_int32x OnOutput, granny_real32 AtT, granny_real32 DeltaT); \
    virtual granny_local_pose* SamplePoseOutput(granny_int32x      OutputIdx,                   \
                                                granny_real32      AtT,                         \
                                                granny_real32      AllowedError,                \
                                                granny_pose_cache* PoseCache,                   \
                                                granny_real32*     PoseWeight);                 \
    virtual bool GetRootMotionVectors(granny_int32x  OutputIdx,                                 \
                                      granny_real32  AtT,                                       \
                                      granny_real32  DeltaT,                                    \
                                      granny_real32* ResultTranslation,                         \
                                      granny_real32* ResultRotation,                            \
                                      bool           Inverse)

#define DECL_SAMPLESCALAR_INTERFACE()                                   \
    public:                                                             \
    virtual granny_real32 SampleScalarOutput(granny_int32x OutputIdx,   \
                                             granny_real32 AtT,         \
                                             gstate_scalar_track_entry * TrackEntry); \
    virtual bool GetScalarOutputRange(granny_int32x  OutputIdx,         \
                                      granny_real32* OutMin,            \
                                      granny_real32* OutMax)

#define DECL_SAMPLEMORPH_INTERFACE()                                            \
    public:                                                                     \
    virtual bool          GetMorphChannelBindings(granny_int32x OutputIdx,      \
                                                  char const**  MeshNames,      \
                                                  granny_int32x MeshNameCount); \
    virtual granny_int32x GetNumMorphChannels(granny_int32x OutputIdx);         \
    virtual bool          SampleMorphOutput(granny_int32x  OutputIdx,           \
                                            granny_real32  AtT,                 \
                                            granny_real32* MorphWeights,        \
                                            granny_int32x NumMorphWeights)        

#define DECL_SAMPLEEVENT_INTERFACE()                                            \
    public:                                                                     \
    virtual bool SampleEventOutput(granny_int32x            OutputIdx,          \
                                   granny_real32            AtT,                \
                                   granny_real32            DeltaT,             \
                                   gstate_text_track_entry* EventBuffer,        \
                                   granny_int32x const      EventBufferSize,    \
                                   granny_int32x*           NumEvents);         \
    virtual bool GetNextEvent(granny_int32x OutputIdx,                          \
                              granny_real32 AtT,                                \
                              gstate_text_track_entry* Event);                  \
    virtual bool GetAllEvents(granny_int32x            OutputIdx,               \
                              gstate_text_track_entry* EventBuffer,             \
                              granny_int32x const      EventBufferSize,         \
                              granny_int32x*           NumEvents);              \
    virtual bool GetCloseEventTimes(granny_int32x  OutputIdx,                   \
                                    granny_real32  AtT,                         \
                                    char const*    TextToFind,                  \
                                    granny_real32* PreviousTime,                \
                                    granny_real32* NextTime)

#define DECL_SAMPLEMASK_INTERFACE()                         \
    public:                                                 \
    virtual bool SampleMaskOutput(granny_int32x OutputIdx,  \
                                  granny_real32 AtT,        \
                                  granny_track_mask*)


#define DECL_IO_MANIPULATION_INTERFACE()                                            \
    public:                                                                         \
    virtual granny_int32x AddInput(node_edge_type EdgeType, char const* EdgeName);  \
    virtual bool          DeleteInput(granny_int32x InputIndex);                    \
    virtual granny_int32x AddOutput(node_edge_type EdgeType, char const* EdgeName); \
    virtual bool          DeleteOutput(granny_int32x OutputIndex)

#define DISALLOW_IO_MANIPULATION_INTERFACE()                                                        \
    public:                                                                                         \
    virtual granny_int32x AddInput(node_edge_type EdgeType, char const* EdgeName) { return -1; }    \
    virtual bool          DeleteInput(granny_int32x InputIndex) { return false; }                   \
    virtual granny_int32x AddOutput(node_edge_type EdgeType, char const* EdgeName) { return -1; }   \
    virtual bool          DeleteOutput(granny_int32x OutputIndex) { return false; }


#define IMPL_PARENT_DIDLOOPOCCUR(Typename)                  \
    bool GSTATE                                             \
    Typename ::DidLoopOccur(granny_int32x OnOutput,         \
                            granny_real32 AtT,              \
                            granny_real32 DeltaT)           \
    {                                                       \
        return parent::DidLoopOccur(OnOutput, AtT, DeltaT); \
    } typedef int __require_semi

#define IMPL_PASSTHROUGH_DIDLOOPOCCUR(Typename, CheckCond)      \
    bool GSTATE                                                 \
    Typename ::DidLoopOccur(granny_int32x OnOutput,             \
                            granny_real32 AtT,                  \
                            granny_real32 DeltaT)               \
    {                                                           \
        GStateAssert(CheckCond);                                \
        INPUT_CONNECTION(0, Pose);                              \
        if (!PoseNode)                                          \
            return false;                                       \
        return PoseNode->DidLoopOccur(PoseEdge, AtT, DeltaT);   \
    } typedef int __require_semi


#define IMPL_PARENT_GETDURATION(Typename)                               \
    granny_real32 GSTATE                                                \
    Typename ::GetDuration(granny_int32x OnOutput, granny_real32 AtT)   \
    {                                                                   \
        return parent::GetDuration(OnOutput, AtT);                      \
    } typedef int __require_semi

#define IMPL_PASSTHROUGH_GETDURATION(Typename, CheckCond)               \
    granny_real32 GSTATE                                                \
    Typename ::GetDuration(granny_int32x OnOutput, granny_real32 AtT)   \
    {                                                                   \
        GStateAssert(CheckCond);                                        \
        node* PoseNode = 0;                                             \
        granny_int32x PoseEdge = -1;                                    \
        GetInputConnection(0, &PoseNode, &PoseEdge);                    \
        if (!PoseNode)                                                  \
            return -1;                                                  \
        return PoseNode->GetDuration(PoseEdge, AtT);                    \
    } typedef int __require_semi


#define IMPL_PARENT_SAMPLEPOSEOUTPUT(Typename, CheckCond)                           \
    granny_local_pose* GSTATE                                                       \
    Typename ::SamplePoseOutput(granny_int32x OnOutput,                             \
                                granny_real32 AtT,                                  \
                                granny_real32 AllowedError,                         \
                                granny_pose_cache* PoseCache,                       \
                                granny_real32* PoseWeight)                                  \
    {                                                                               \
        return parent::SamplePoseOutput(OnOutput, AtT, AllowedError, PoseCache, PoseWeight);    \
    } typedef int __require_semi

#define IMPL_PASSTHROUGH_SAMPLEPOSEOUTPUT(Typename, CheckCond)                      \
    granny_local_pose* GSTATE                                                       \
    Typename ::SamplePoseOutput(granny_int32x OnOutput,                             \
                                granny_real32 AtT,                                  \
                                granny_real32 AllowedError,                         \
                                granny_pose_cache* PoseCache,                       \
                                granny_real32* PoseWeight)                                  \
    {                                                                               \
        GStateAssert(CheckCond);                                                    \
        GStateAssert(PoseCache != 0);                                               \
        node* PoseNode = 0;                                                         \
        granny_int32x PoseEdge = -1;                                                \
        GetInputConnection(OnOutput, &PoseNode, &PoseEdge);                         \
        if (!PoseNode)                                                              \
            return 0;                                                               \
        return PoseNode->SamplePoseOutput(PoseEdge, AtT, AllowedError, PoseCache, PoseWeight);  \
    } typedef int __require_semi


#define IMPL_PARENT_GETROOTMOTIONVECTORS(Typename, CheckCond)                   \
    bool GSTATE                                                                 \
    Typename ::GetRootMotionVectors(granny_int32x  OnOutput,                    \
                                    granny_real32  AtT,                         \
                                    granny_real32  DeltaT,                      \
                                    granny_real32* ResultTranslation,           \
                                    granny_real32* ResultRotation,              \
                                    bool           Inverse)                     \
    {                                                                           \
        return parent::GetRootMotionVectors(OnOutput, AtT, DeltaT,              \
                                            ResultTranslation, ResultRotation,  \
                                            Inverse);                           \
    } typedef int __require_semi

#define IMPL_PASSTHROUGH_GETROOTMOTIONVECTORS(Typename, CheckCond)                      \
    bool GSTATE                                                                         \
    Typename ::GetRootMotionVectors(granny_int32x  OnOutput,                            \
                                    granny_real32  AtT,                                 \
                                    granny_real32  DeltaT,                              \
                                    granny_real32* ResultTranslation,                   \
                                    granny_real32* ResultRotation,                      \
                                    bool           Inverse)                             \
    {                                                                                   \
        GStateAssert(GetBoundCharacter());                                              \
        GStateAssert(CheckCond);                                                        \
        if (DeltaT < 0) {                                                               \
            GS_PreconditionFailed;                                                      \
            return false;                                                               \
        }                                                                               \
        if (!ResultTranslation || !ResultRotation) {                                    \
            GS_PreconditionFailed;                                                      \
            return false;                                                               \
        }                                                                               \
        node* PoseNode = 0;                                                             \
        granny_int32x PoseEdge = -1;                                                    \
        GetInputConnection(OnOutput, &PoseNode, &PoseEdge);                             \
                                                                                        \
        if (PoseNode) {                                                                 \
            return PoseNode->GetRootMotionVectors(PoseEdge, AtT, DeltaT,                \
                                                  ResultTranslation, ResultRotation,    \
                                                  Inverse);                             \
        }                                                                               \
        else {                                                                          \
            memset(ResultTranslation, 0, sizeof(granny_real32)*3);                      \
            memset(ResultRotation,    0, sizeof(granny_real32)*3);                      \
            return false;                                                               \
        }                                                                               \
    } typedef int __require_semi


#define IMPL_PASSTHROUGH_GETMORPHCHANNELBINDINGS(Typename, Offset, CheckCond)           \
    bool GSTATE Typename :: GetMorphChannelBindings(granny_int32x OnOutput,             \
                                                    char const**  MeshNames,            \
                                                    granny_int32x MeshNameCount)        \
    {                                                                                   \
        GStateAssert(GS_InRange(OnOutput, GetNumOutputs()));                            \
        GStateAssert(CheckCond);                                                        \
        node* MorphNode = 0;                                                            \
        granny_int32x MorphEdge = -1;                                                   \
        GetInputConnection(OnOutput + (Offset), &MorphNode, &MorphEdge);                \
        if (!MorphNode)                                                                 \
            return false;                                                               \
        return MorphNode->GetMorphChannelBindings(MorphEdge, MeshNames, MeshNameCount); \
    } typedef int __require_semi



#define IMPL_PASSTHROUGH_GETNUMMORPHCHANNELS(Typename, Offset, CheckCond)           \
    granny_int32x GSTATE Typename :: GetNumMorphChannels(granny_int32x OnOutput)    \
    {                                                                               \
        GStateAssert(GS_InRange(OnOutput, GetNumOutputs()));                        \
        GStateAssert(CheckCond);                                                    \
        node* MorphNode = 0;                                                        \
        granny_int32x MorphEdge = -1;                                               \
        GetInputConnection(OnOutput + (Offset), &MorphNode, &MorphEdge);            \
        if (!MorphNode)                                                             \
            return -1;                                                              \
        return MorphNode->GetNumMorphChannels(MorphEdge);                           \
    } typedef int __require_semi



#define IMPL_PASSTHROUGH_SAMPLEMORPHOUTPUT(Typename, Offset, CheckCond)                 \
    bool GSTATE Typename :: SampleMorphOutput(granny_int32x  OnOutput,                  \
    granny_real32  AtT,                           \
    granny_real32* MorphWeights,                  \
    granny_int32x NumMorphWeights)                \
    {                                                                                   \
    GStateAssert(GS_InRange(OnOutput, GetNumOutputs()));                                \
    GStateAssert(CheckCond);                                                            \
    node* MorphNode = 0;                                                                \
    granny_int32x MorphEdge = -1;                                                       \
    GetInputConnection(OnOutput + (Offset), &MorphNode, &MorphEdge);                    \
    if (!MorphNode)                                                                     \
    return false;                                                                       \
    return MorphNode->SampleMorphOutput(MorphEdge, AtT, MorphWeights, NumMorphWeights); \
    } typedef int __require_semi

#define IMPL_PASSTHROUGH_SAMPLEEVENTOUTPUT(Typename, Offset, CheckCond)                 \
    bool GSTATE Typename :: SampleEventOutput(granny_int32x            OnOutput,        \
    granny_real32            AtT,                \
    granny_real32            DeltaT,             \
    gstate_text_track_entry* EventBuffer,        \
    granny_int32x const      EventBufferSize,    \
    granny_int32x*           NumEvents)          \
    {                                                                                   \
    GStateAssert(GS_InRange(OnOutput, GetNumOutputs()));                               \
    GStateAssert(CheckCond);                                                            \
    node* EventNode = 0;                                                                \
    granny_int32x EventEdge = -1;                                                       \
    GetInputConnection(OnOutput + (Offset), &EventNode, &EventEdge);                    \
    if (!EventNode)                                                                     \
    return false;                                                                       \
    return EventNode->SampleEventOutput(EventEdge, AtT, DeltaT, EventBuffer, EventBufferSize, NumEvents); \
    } typedef int __require_semi

#define IMPL_PASSTHROUGH_GETNEXTEVENT(Typename, Offset, CheckCond)                 \
    bool GSTATE Typename :: GetNextEvent(granny_int32x            OnOutput,        \
    granny_real32            AtT,                                                  \
    gstate_text_track_entry* Event)                                                \
    {                                                                                   \
    GStateAssert(GS_InRange(OnOutput, GetNumOutputs()));                               \
    GStateAssert(CheckCond);                                                            \
    node* EventNode = 0;                                                                \
    granny_int32x EventEdge = -1;                                                       \
    GetInputConnection(OnOutput + (Offset), &EventNode, &EventEdge);                    \
    if (!EventNode)                                                                     \
    return false;                                                                       \
    return EventNode->GetNextEvent(EventEdge, AtT, Event);                              \
    } typedef int __require_semi


#define IMPL_PASSTHROUGH_GETALLEVENTS(Typename, Offset, CheckCond)                      \
    bool GSTATE Typename :: GetAllEvents(granny_int32x            OnOutput,             \
    gstate_text_track_entry* EventBuffer,                                               \
    granny_int32x const      EventBufferSize,                                           \
    granny_int32x*           NumEvents)                                                 \
    {                                                                                   \
    GStateAssert(GS_InRange(OnOutput, GetNumOutputs()));                               \
    GStateAssert(CheckCond);                                                            \
    node* EventNode = 0;                                                                \
    granny_int32x EventEdge = -1;                                                       \
    GetInputConnection(OnOutput + (Offset), &EventNode, &EventEdge);                    \
    if (!EventNode)                                                                     \
    return false;                                                                       \
    return EventNode->GetAllEvents(EventEdge, EventBuffer, EventBufferSize, NumEvents); \
    } typedef int __require_semi


#define IMPL_PASSTHROUGH_GETCLOSEEVENTTIMES(Typename, Offset, CheckCond)                \
    bool GSTATE Typename :: GetCloseEventTimes(granny_int32x            OnOutput,       \
    granny_real32  AtT,                                                                 \
    char const*    TextToFind,                                                          \
    granny_real32* PreviousTime,                                                        \
    granny_real32* NextTime)                                                            \
    {                                                                                   \
    GStateAssert(GS_InRange(OnOutput, GetNumOutputs()));                               \
    GStateAssert(CheckCond);                                                            \
    node* EventNode = 0;                                                                \
    granny_int32x EventEdge = -1;                                                       \
    GetInputConnection(OnOutput + (Offset), &EventNode, &EventEdge);                    \
    if (!EventNode)                                                                     \
    return false;                                                                       \
    return EventNode->GetCloseEventTimes(EventEdge, AtT, TextToFind, PreviousTime, NextTime); \
    } typedef int __require_semi


#define IMPL_PASSTHROUGH_SAMPLESCALAROUTPUT(Typename, Offset, CheckCond)                \
    granny_real32 GSTATE Typename :: SampleScalarOutput(granny_int32x            OnOutput,       \
    granny_real32            AtT,                                                       \
    gstate_scalar_track_entry * TrackEntry)                                             \
    {                                                                                   \
    GStateAssert(GS_InRange(OnOutput, GetNumOutputs()));                                \
    GStateAssert(CheckCond);                                                            \
    node* ScalarNode = 0;                                                               \
    granny_int32x ScalarEdge = -1;                                                      \
    GetInputConnection(OnOutput + (Offset), &ScalarNode, &ScalarEdge);                  \
    if (!ScalarNode)                                                                    \
    return false;                                                                       \
    return ScalarNode->SampleScalarOutput(ScalarEdge, AtT, TrackEntry);                  \
    } typedef int __require_semi

#define IMPL_PASSTHROUGH_GETSCALAROUTPUTRANGE(Typename, Offset, CheckCond)              \
    bool GSTATE Typename :: GetScalarOutputRange(granny_int32x  OnOutput,               \
    granny_real32* OutMin,                                                              \
    granny_real32* OutMax)                                                              \
    {                                                                                   \
    GStateAssert(GS_InRange(OnOutput, GetNumOutputs()));                                \
    GStateAssert(CheckCond);                                                            \
    node* ScalarNode = 0;                                                               \
    granny_int32x ScalarEdge = -1;                                                      \
    GetInputConnection(OnOutput + (Offset), &ScalarNode, &ScalarEdge);                  \
    if (!ScalarNode)                                                                    \
    return false;                                                                       \
    return ScalarNode->GetScalarOutputRange(ScalarEdge, OutMin, OutMax);                \
    } typedef int __require_semi


#define IMPL_PASSTHROUGH_SAMPLEPOSEINTERFACE(Typename, CheckCond)                       \
    IMPL_PASSTHROUGH_SAMPLEPOSEOUTPUT(Typename, CheckCond);                             \
    IMPL_PASSTHROUGH_GETDURATION(Typename, CheckCond);                                  \
    IMPL_PASSTHROUGH_DIDLOOPOCCUR(Typename, CheckCond)                                  

#define IMPL_PASSTHROUGH_SAMPLEMORPHINTERFACE(Typename, Offset, CheckCond)              \
    IMPL_PASSTHROUGH_GETMORPHCHANNELBINDINGS(Typename, Offset, CheckCond);              \
    IMPL_PASSTHROUGH_GETNUMMORPHCHANNELS(Typename, Offset, CheckCond);                  \
    IMPL_PASSTHROUGH_SAMPLEMORPHOUTPUT(Typename, Offset, CheckCond)  

#define IMPL_PASSTHROUGH_SAMPLEEVENTINTERFACE(Typename, Offset, CheckCond)              \
    IMPL_PASSTHROUGH_SAMPLEEVENTOUTPUT(Typename, Offset, CheckCond);                    \
    IMPL_PASSTHROUGH_GETNEXTEVENT(Typename, Offset, CheckCond);                         \
    IMPL_PASSTHROUGH_GETALLEVENTS(Typename, Offset, CheckCond);                         \
    IMPL_PASSTHROUGH_GETCLOSEEVENTTIMES(Typename, Offset, CheckCond)

#define IMPL_PASSTHROUGH_SAMPLESCALARINTERFACE(Typename, Offset, CheckCond)             \
    IMPL_PASSTHROUGH_SAMPLESCALAROUTPUT(Typename, Offset, CheckCond);                   \
    IMPL_PASSTHROUGH_GETSCALAROUTPUTRANGE(Typename, Offset, CheckCond)

#define GSTATE_NODE_MACROS_H
#endif /* GSTATE_NODE_MACROS_H */







