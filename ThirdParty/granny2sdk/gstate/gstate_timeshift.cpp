// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_timeshift.h"

#include "gstate_anim_utils.h"
#include "gstate_node_visitor.h"
#include "gstate_token_context.h"
#include "gstate_snapshotutils.h"
#include "gstate_telemetry.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE timeshift::timeshiftImpl
{
    granny_bool32 OffsetIsRelative;
};

granny_data_type_definition GSTATE
timeshift::timeshiftImplType[] =
{
    { GrannyBool32Member, "OffsetIsRelative" },
    { GrannyEndMember },
};

// timeshift is a concrete class, so we must create a slotted container
struct timeshift_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(timeshift);
};

granny_data_type_definition timeshift::timeshiftTokenType[] =
{
    DECL_UID_MEMBER(timeshift),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(timeshift),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(timeshift);
IMPL_CREATE_NODE_BOILERPLATE(timeshift);


GSTATE
timeshift::timeshift(token_context*               Context,
                     granny_data_type_definition* TokenType,
                     void*                        TokenObject,
                     token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_timeshiftToken(0),
    m_LocalOffset(0),
    m_LastObservedScale(-1)
{
    IMPL_INIT_FROM_TOKEN(timeshift);

    if (EditorCreated())
    {
        // Add our default input/output
        AddInputImpl(ScalarEdge, "TimeScale");
        AddInputImpl(ScalarEdge, "TimeOffset");
        AddInputImpl(PoseEdge,   "Pose");
        AddOutputImpl(PoseEdge,  "Pose");
    }
}


GSTATE
timeshift::~timeshift()
{
    DTOR_RELEASE_TOKEN(timeshift);
}


bool GSTATE
timeshift::FillDefaultToken(granny_data_type_definition* TokenType,
                            void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares timeshiftImpl*& Slot = // member
    GET_TOKEN_SLOT(timeshift);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}


bool GSTATE
timeshift::GetOffsetRelative() const
{
    return m_timeshiftToken->OffsetIsRelative != 0;
}

void GSTATE
timeshift::SetOffsetRelative(bool Relative)
{
    TakeTokenOwnership();

    m_timeshiftToken->OffsetIsRelative = Relative ? 1 : 0;
}

void GSTATE
timeshift::GetScaleOffset(granny_real32  AtT,
                          granny_real32& Scale,
                          granny_real32& Offset)
{
    Scale  = 1.0f;
    Offset = 0.0f;

    INPUT_CONNECTION(0, Scale);
    if (ScaleNode != 0)
    {
        Scale = ScaleNode->SampleScalarOutput(ScaleEdge, AtT, NULL);

        // Clamp the scale to 0, since we can't run backwards
        if (Scale < 0)
            Scale = 0.0f;
    }

    INPUT_CONNECTION(1, Offset);
    if (OffsetNode != 0)
    {
        Offset = OffsetNode->SampleScalarOutput(OffsetEdge, AtT, NULL);
    }

    if (m_timeshiftToken->OffsetIsRelative)
    {
        Offset = Clamp(0, Offset, 1);
    }
}

granny_real32 GSTATE
timeshift::ComputeSampleT(granny_real32 const AtT,
                          granny_real32 const Scale,
                          granny_real32 const Offset,
                          node*               PoseNode,
                          granny_int32x       ConnectedEdge)
{
    GStateAssert(PoseNode);
    GStateAssert(Scale >= 0.0f);

    granny_real32 const PoseOffset = PoseNode->GetLocalTimeOffset(AtT, ConnectedEdge);

    if (m_LastObservedScale >= 0 && m_LastObservedScale != Scale)
    {
        // Ok, recompute the local offset so that the time will match correctly.  Note
        // that offset cancels, so we can ignore that.
        granny_real32 OldT = ((AtT + m_LocalOffset) - PoseOffset) * m_LastObservedScale;

        if (Scale == 0)
        {
            // Scale is 0, all is ruined.
            m_LocalOffset = 0;
        }
        else
        {
            m_LocalOffset = (OldT / Scale) + PoseOffset - AtT;
        }
    }
    m_LastObservedScale = Scale;

    granny_real32 MyT = AtT + m_LocalOffset;
    granny_real32 const LocalT = MyT - PoseOffset;

    // Fall through to default case if the node attached doesn't support duration query...
    granny_real32 SampleT;
    do {
        if (m_timeshiftToken->OffsetIsRelative)
        {
            GStateAssert(Offset >= 0 && Offset <= 1);

            granny_real32 Duration = PoseNode->GetDuration(ConnectedEdge, LocalT);
            if (Duration > 0)
            {
                SampleT = (LocalT * Scale + (Offset * Duration));
                break; // exits to return fn
            }
        }

        SampleT  = LocalT * Scale + Offset;
    } while (false);

    return SampleT + PoseOffset;
}


granny_int32x GSTATE
timeshift::AddInput(node_edge_type EdgeType, char const* EdgeName)
{
    // Forward this to AddOutput to eliminate dupes
    granny_int32x NewOutput = AddOutput(EdgeType, EdgeName);
    if (NewOutput == -1)
        return -1;

    GStateAssert(GS_InRange(NewOutput+2, GetNumInputs()));
    return NewOutput + 2;
}

bool GSTATE
timeshift::DeleteInput(granny_int32x InputIdx)
{
    // Cannot delete the scale/offset inputs...
    if (InputIdx < 2)
        return false;

    GStateAssert(GS_InRange(InputIdx-2,GetNumOutputs()));
    return DeleteOutput(InputIdx - 2);
}

granny_int32x GSTATE
timeshift::AddOutput(node_edge_type EdgeType, char const* EdgeName)
{
    // 0 must be pose
    if (GetNumOutputs() == 0 && EdgeType != PoseEdge)
        return -1;

    // Nothing else may be pose.
    if (GetNumOutputs() >= 1 && EdgeType == PoseEdge)
        return -1;

    if (EdgeName == 0)
        return -1;

    granny_int32x NewInput  = AddInputImpl(EdgeType, EdgeName);
    granny_int32x NewOutput = AddOutputImpl(EdgeType, EdgeName);
    GStateAssert(NewInput == NewOutput + 2);
    GStateUnused(NewInput);
    GStateUnused(NewOutput);

    return NewOutput;
}

bool GSTATE
timeshift::DeleteOutput(granny_int32x OutputIndex)
{
    if (OutputIndex == 0)
        return false;

    if (!GS_InRange(OutputIndex, GetNumOutputs()))
        return false;
    GStateAssert(GS_InRange(OutputIndex+2, GetNumInputs()));

    if (parent::DeleteInputImpl(OutputIndex+2) == false)
    {
        GS_InvalidCodePath("catastrohpic");
    }

    return parent::DeleteOutputImpl(OutputIndex);
}

void
timeshift::AddInputsForOutput(granny_int32x OutputIndex)
{
    //Do nothing. In this class, adding outputs automatically adds inputs
}

void
timeshift::DeleteInputsForOutput(granny_int32x OutputIndex)
{
    //Do nothing. In this class, deleting outputs automatically adds inputs
}

void GSTATE
timeshift::SetInputNamesForOutput(granny_int32x OutputIndex, char const* NewEdgeName)
{
    SetInputName(OutputIndex + 2, NewEdgeName);
}


void GSTATE
timeshift::Activate(granny_real32 AtT)
{
    parent::Activate(AtT);

    m_LocalOffset = 0;
    m_LastObservedScale = -1;
}

granny_int32x GSTATE
timeshift::GetOutputPassthrough(granny_int32x OutputIdx) const
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    // Pose is always connected
    if (OutputIdx == 0)
        return 2;

    INPUT_CONNECTION(2, Pose);
    if (PoseNode)
        return OutputIdx + 2; // only return if connected

    return -1;
}

granny_real32 GSTATE
timeshift::GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx)
{
    GStateAssert(OutputIdx == 0);

    INPUT_CONNECTION(OutputIdx+2, Pose);
    if (!PoseNode)
        return 0;

    return PoseNode->GetLocalTimeOffset(AtT, PoseEdge);
}

granny_local_pose* GSTATE
timeshift::SamplePoseOutput(granny_int32x      OutputIdx,
                            granny_real32      AtT,
                            granny_real32      AllowedError,
                            granny_pose_cache* PoseCache,
                            granny_real32*     PoseWeight)
{
    GSTATE_AUTO_ZONE_FN_OR_NAME();

    GStateAssert(PoseCache != 0);
    GStateAssert(OutputIdx == 0);

    INPUT_CONNECTION(OutputIdx+2, Pose);
    if (!PoseNode)
        return 0;

    granny_real32 Scale  = 1.0f;
    granny_real32 Offset = 0.0f;
    GetScaleOffset(AtT, Scale, Offset);

    // Handles relative offset...
    granny_real32 SampleT = ComputeSampleT(AtT, Scale, Offset, PoseNode, PoseEdge);
    return PoseNode->SamplePoseOutput(PoseEdge, SampleT, AllowedError, PoseCache, PoseWeight);
}

bool GSTATE
timeshift::GetRootMotionVectors(granny_int32x  OutputIdx,
                                granny_real32  AtT,
                                granny_real32  DeltaT,
                                granny_real32* Translation,
                                granny_real32* Rotation,
                                bool Inverse)
{
    if (DeltaT < 0)
    {
        GS_PreconditionFailed;
        return false;
    }
    if (!Translation || !Rotation)
    {
        GS_PreconditionFailed;
        return false;
    }

    INPUT_CONNECTION(OutputIdx + 2, Pose);
    if (!PoseNode)
        return false;

    granny_real32 Scale  = 1.0f;
    granny_real32 Offset = 0.0f;
    GetScaleOffset(AtT, Scale, Offset);

    // Handles relative offset...
    granny_real32 SampleT = ComputeSampleT(AtT, Scale, Offset, PoseNode, PoseEdge);

    // Always the same...
    granny_real32 NewDelta = DeltaT * Scale;

    return PoseNode->GetRootMotionVectors(PoseEdge, SampleT, NewDelta, Translation, Rotation, Inverse);
}

granny_real32 GSTATE
timeshift::GetDuration(granny_int32x OnOutput,
                       granny_real32 AtT)
{
    INPUT_CONNECTION(OnOutput+2, Pose);
    if (!PoseNode)
        return -1;

    granny_real32 Scale  = 1.0f;
    granny_real32 Offset = 0.0f;
    GetScaleOffset(AtT, Scale, Offset);

    granny_real32 SampleT = ComputeSampleT(AtT, Scale, Offset, PoseNode, PoseEdge);
    granny_real32 UnscaledDuration = PoseNode->GetDuration(PoseEdge, SampleT);
    if (UnscaledDuration < 0)
        return -1;

    return (UnscaledDuration * Scale);
}

bool GSTATE
timeshift::DidLoopOccur(granny_int32x OnOutput,
                        granny_real32 AtT,
                        granny_real32 DeltaT)
{
    INPUT_CONNECTION(OnOutput+2, Pose);
    if (!PoseNode)
        return false;

    granny_real32 Scale  = 1.0f;
    granny_real32 Offset = 0.0f;
    GetScaleOffset(AtT, Scale, Offset);

    // Handles relative offset...
    granny_real32 SampleT = ComputeSampleT(AtT, Scale, Offset, PoseNode, PoseEdge);
    granny_real32 NewDelta = DeltaT * Scale;

    return PoseNode->DidLoopOccur(PoseEdge, SampleT, NewDelta);
}

IMPL_PASSTHROUGH_GETSCALAROUTPUTRANGE(timeshift, 2, true);

granny_real32 GSTATE
timeshift::SampleScalarOutput(granny_int32x OnOutput,
                              granny_real32 AtT, 
                              gstate_scalar_track_entry * TrackEntry)
{
    GStateAssert(GS_InRange(OnOutput, GetNumOutputs()));

    INPUT_CONNECTION(2, Pose);
    if (!PoseNode)
        return 0;

    INPUT_CONNECTION(OnOutput+2, Scalar);
    if (!ScalarNode)
        return 0;

    granny_real32 Scale  = 1.0f;
    granny_real32 Offset = 0.0f;
    GetScaleOffset(AtT, Scale, Offset);

    // Handles relative offset...
    granny_real32 SampleT = ComputeSampleT(AtT, Scale, Offset, PoseNode, PoseEdge);

    return ScalarNode->SampleScalarOutput(ScalarEdge, SampleT, TrackEntry);
}


// These passthroughs do not reference T, so they can go straight through to the morph
// edge on the other side
IMPL_PASSTHROUGH_GETMORPHCHANNELBINDINGS(timeshift, 2, true);
IMPL_PASSTHROUGH_GETNUMMORPHCHANNELS(timeshift, 2, true);

bool GSTATE
timeshift::SampleMorphOutput(granny_int32x  OnOutput,
                             granny_real32  AtT,
                             granny_real32* MorphWeights,
                             granny_int32x  NumMorphWeights)
{
    GStateAssert(GS_InRange(OnOutput, GetNumOutputs()));

    INPUT_CONNECTION(2, Pose);
    if (!PoseNode)
        return false;

    INPUT_CONNECTION(OnOutput+2, Morph);
    if (!MorphNode)
        return false;

    granny_real32 Scale  = 1.0f;
    granny_real32 Offset = 0.0f;
    GetScaleOffset(AtT, Scale, Offset);

    // Handles relative offset...
    granny_real32 SampleT = ComputeSampleT(AtT, Scale, Offset, PoseNode, PoseEdge);

    return MorphNode->SampleMorphOutput(MorphEdge, SampleT, MorphWeights, NumMorphWeights);
}

bool GSTATE
timeshift::SampleEventOutput(granny_int32x            OnOutput,
                             granny_real32            AtT,
                             granny_real32            DeltaT,
                             gstate_text_track_entry* EventBuffer,
                             granny_int32x const      EventBufferSize,
                             granny_int32x*           NumEvents)
{
    INPUT_CONNECTION(2, Pose);
    if (!PoseNode)
        return false;

    INPUT_CONNECTION(OnOutput+2, Event);
    if (!EventNode)
        return false;

    granny_real32 Scale  = 1.0f;
    granny_real32 Offset = 0.0f;
    GetScaleOffset(AtT, Scale, Offset);

    // Handles relative offset...
    granny_real32 SampleT = ComputeSampleT(AtT, Scale, Offset, PoseNode, PoseEdge);
    granny_real32 NewDelta = DeltaT * Scale;

    return EventNode->SampleEventOutput(EventEdge, SampleT, NewDelta,
                                        EventBuffer, EventBufferSize, NumEvents);
}

bool GSTATE
timeshift::GetNextEvent(granny_int32x OnOutput,
                        granny_real32 AtT,
                        gstate_text_track_entry*  Event)
{
    INPUT_CONNECTION(2, Pose);
    if (!PoseNode)
        return false;

    INPUT_CONNECTION(OnOutput+2, Event);
    if (!EventNode)
        return false;

    granny_real32 Scale  = 1.0f;
    granny_real32 Offset = 0.0f;
    GetScaleOffset(AtT, Scale, Offset);

    // Handles relative offset...
    granny_real32 SampleT = ComputeSampleT(AtT, Scale, Offset, PoseNode, PoseEdge);

    return EventNode->GetNextEvent(EventEdge, SampleT, Event);
}

bool GSTATE
timeshift::GetAllEvents(granny_int32x            OnOutput,
                        gstate_text_track_entry*  EventBuffer,
                        granny_int32x const      EventBufferSize,
                        granny_int32x*           NumEvents)
{
    INPUT_CONNECTION(OnOutput+2, Event);
    if (!EventNode)
        return false;

    return EventNode->GetAllEvents(EventEdge, EventBuffer, EventBufferSize, NumEvents);
}

bool GSTATE
timeshift::GetCloseEventTimes(granny_int32x  OnOutput,
                              granny_real32  AtT,
                              char const*    TextToFind,
                              granny_real32* PreviousTime,
                              granny_real32* NextTime)
{
    INPUT_CONNECTION(2, Pose);
    if (!PoseNode)
        return false;

    INPUT_CONNECTION(OnOutput+2, Event);
    if (!EventNode)
        return false;

    granny_real32 Scale  = 1.0f;
    granny_real32 Offset = 0.0f;
    GetScaleOffset(AtT, Scale, Offset);

    // Handles relative offset...
    granny_real32 SampleT = ComputeSampleT(AtT, Scale, Offset, PoseNode, PoseEdge);

    // todo: project out into global time.
    return EventNode->GetCloseEventTimes(EventEdge, SampleT,
                                         TextToFind, PreviousTime, NextTime);
}


CREATE_SNAPSHOT(timeshift)
{
    CREATE_WRITE_REAL32(m_LocalOffset);
    CREATE_WRITE_REAL32(m_LastObservedScale);
    CREATE_PASS_TO_PARENT();
}

RESET_FROMSNAPSHOT(timeshift)
{
    RESET_OFFSET_TRACKING();
    RESET_READ_REAL32(m_LocalOffset);
    RESET_READ_REAL32(m_LastObservedScale);
    RESET_PASS_TO_PARENT();
}


