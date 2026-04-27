// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_timeselect.cpp $
// $DateTime: 2012/01/25 13:33:09 $
// $Change: 36369 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================
#include "gstate_timeselect.h"

#include "gstate_anim_utils.h"
#include "gstate_node_visitor.h"
#include "gstate_token_context.h"
#include "gstate_telemetry.h"

#include <string.h>
#include <stdlib.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE timeselect::timeselectImpl
{
    granny_bool32 TimeIsUnitRelative;
};

granny_data_type_definition GSTATE
timeselect::timeselectImplType[] =
{
    { GrannyInt32Member, "TimeIsUnitRelative" },
    { GrannyEndMember },
};

// timeselect is a concrete class, so we must create a slotted container
struct timeselect_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(timeselect);
};

granny_data_type_definition timeselect::timeselectTokenType[] =
{
    DECL_UID_MEMBER(timeselect),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(timeselect),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(timeselect);
IMPL_CREATE_NODE_BOILERPLATE(timeselect);

GSTATE
timeselect::timeselect(token_context*               Context,
                       granny_data_type_definition* TokenType,
                       void*                        TokenObject,
                       token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_timeselectToken(0)
{
    IMPL_INIT_FROM_TOKEN(timeselect);

    if (EditorCreated())
    {
        // Add our default input/output
        AddInputImpl(ScalarEdge, "Time");
        AddInputImpl(PoseEdge,   "Pose");
        AddOutputImpl(PoseEdge,  "Pose");
    }
}

GSTATE
timeselect::~timeselect()
{
    DTOR_RELEASE_TOKEN(timeselect);
}

bool GSTATE
timeselect::FillDefaultToken(granny_data_type_definition* TokenType,
                             void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares timeselectImpl*& Slot = // member
    GET_TOKEN_SLOT(timeselect);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}

granny_real32 GSTATE
timeselect::GetPosition(granny_real32 AtT)
{
    node* PositionNode = 0;
    granny_int32x PositionEdge = -1;
    GetInputConnection(0, &PositionNode, &PositionEdge);

    granny_real32 Result = 0.0f;
    if (PositionNode != 0)
        Result = PositionNode->SampleScalarOutput(PositionEdge, AtT, NULL);

    if (m_timeselectToken->TimeIsUnitRelative)
        Result = Clamp(0, Result, 1.0f);

    return Result;
}

granny_real32 GSTATE
timeselect::GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx)
{
    GStateAssert(OutputIdx == 0);

    // 0 input is the time, advance one for the correct wireup...
    INPUT_CONNECTION(OutputIdx + 1, Pose);
    if (!PoseNode)
        return 0;

    return PoseNode->GetLocalTimeOffset(AtT, PoseEdge);
}

granny_local_pose* GSTATE
timeselect::SamplePoseOutput(granny_int32x      OutputIdx,
                             granny_real32      AtT,
                             granny_real32      AllowedError,
                             granny_pose_cache* PoseCache,
                             granny_real32*     PoseWeight)
{
    GSTATE_AUTO_ZONE_FN_OR_NAME();

    GStateAssert(PoseCache != 0);
    GStateAssert(GetOutputType(OutputIdx) == PoseEdge);

    // 0 input is the time, advance one for the correct wireup...
    INPUT_CONNECTION(OutputIdx + 1, Pose);
    if (!PoseNode)
        return 0;

    granny_real32 Position = GetPosition(AtT);
    granny_real32 SampleT  = ComputeSampleT(AtT, Position, PoseNode, PoseEdge);

    return PoseNode->SamplePoseOutput(PoseEdge, SampleT, AllowedError, PoseCache, PoseWeight);
}

bool GSTATE
timeselect::GetRootMotionVectors(granny_int32x  OutputIdx,
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

    // This is a bit of a philisophical issue.  For the moment, I'm going to assume that
    // for a positionally selected animation, there is no need to maintain continuity
    // between samples.  This may wind up being false in the future, TODO: revisit.
    memset(Translation, 0, sizeof(granny_real32)*3);
    memset(Rotation, 0, sizeof(granny_real32)*3);

    return true;
}

IMPL_PASSTHROUGH_GETMORPHCHANNELBINDINGS(timeselect, 1, true);
IMPL_PASSTHROUGH_GETNUMMORPHCHANNELS(timeselect, 1, true);
IMPL_PASSTHROUGH_GETALLEVENTS(timeselect, 1, true);
IMPL_PASSTHROUGH_GETSCALAROUTPUTRANGE(timeselect, 1, true);

granny_real32 GSTATE
timeselect::ComputeSampleT(granny_real32 AtT, 
                           granny_real32 Position,
                           node*         Node,
                           granny_int32x Edge)
{
    GStateAssert(Node);

    if (m_timeselectToken->TimeIsUnitRelative)
    {
        granny_real32 Duration = Node->GetDuration(Edge, Position);
        if (Duration >= 0.0f)
            Position *= Duration;
        else
            Position = 0;
    }

    return Position + Node->GetLocalTimeOffset(AtT, Edge);
}

bool GSTATE
timeselect::SampleMorphOutput(granny_int32x  OutputIdx,
                              granny_real32  AtT,
                              granny_real32* MorphWeights,
                              granny_int32x  NumMorphWeights)
{
    GStateAssert(GetOutputType(OutputIdx) == MorphEdge);

    // 0 input is the time, advance one for the correct wireup...
    INPUT_CONNECTION(OutputIdx + 1, Morph);
    if (!MorphNode)
        return 0;

    granny_real32 Position = GetPosition(AtT);
    granny_real32 SampleT = ComputeSampleT(AtT, Position, MorphNode, MorphEdge);

    return MorphNode->SampleMorphOutput(MorphEdge, SampleT, MorphWeights, NumMorphWeights);
}



granny_int32x GSTATE
timeselect::AddInput(node_edge_type EdgeType, char const* EdgeName)
{
    // Forward this to AddOutput to eliminate dupes
    granny_int32x NewOutput = AddOutput(EdgeType, EdgeName);
    if (NewOutput == -1)
        return -1;

    GStateAssert(GS_InRange(NewOutput+1, GetNumInputs()));
    return NewOutput + 1;
}

bool GSTATE
timeselect::DeleteInput(granny_int32x InputIdx)
{
    GStateAssert(GS_InRange(InputIdx, GetNumInputs()));
    // Cannot delete the select9ion input...
    if (InputIdx < 1)
        return false;

    GStateAssert(GS_InRange(InputIdx-2,GetNumOutputs()));
    return DeleteOutput(InputIdx - 1);
}

granny_int32x GSTATE
timeselect::AddOutput(node_edge_type EdgeType, char const* EdgeName)
{
    if (EdgeType != PoseEdge && EdgeType != MorphEdge && EdgeType != ScalarEdge && EdgeType != EventEdge)
        return -1;

    if (EdgeName == 0)
        return -1;

    granny_int32x NewInput  = AddInputImpl(EdgeType, EdgeName);
    granny_int32x NewOutput = AddOutputImpl(EdgeType, EdgeName);
    GStateAssert(NewInput == NewOutput + 1);
    GStateUnused(NewInput);
    GStateUnused(NewOutput);

    return NewOutput;
}

bool GSTATE
timeselect::DeleteOutput(granny_int32x OutputIndex)
{
    if (!GS_InRange(OutputIndex, GetNumOutputs()))
        return false;
    GStateAssert(GS_InRange(OutputIndex+1, GetNumInputs()));

    if (parent::DeleteInputImpl(OutputIndex+1) == false)
    {
        GS_InvalidCodePath("catastrohpic");
    }

    return parent::DeleteOutputImpl(OutputIndex);
}

void
timeselect::AddInputsForOutput(granny_int32x OutputIndex)
{
    //Do nothing. In this class, adding outputs automatically adds inputs
}

void
timeselect::DeleteInputsForOutput(granny_int32x OutputIndex)
{
    //Do nothing. In this class, deleting outputs automatically adds inputs
}

void GSTATE
timeselect::SetInputNamesForOutput(granny_int32x OutputIndex, char const* NewEdgeName)
{
    SetInputName(OutputIndex + 1, NewEdgeName);
}

granny_int32x GSTATE
timeselect::GetOutputPassthrough(granny_int32x OutputIdx) const
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    return OutputIdx + 1;
}


bool GSTATE
timeselect::GetTimeIsRelative() const
{
    return m_timeselectToken->TimeIsUnitRelative != 0;
}

void GSTATE
timeselect::SetTimeIsRelative(bool IsRelative)
{
    TakeTokenOwnership();

    m_timeselectToken->TimeIsUnitRelative = (granny_bool32)IsRelative;
}

granny_real32 GSTATE
timeselect::SampleScalarOutput(granny_int32x OutputIdx,
                               granny_real32 AtT, 
                               gstate_scalar_track_entry * TrackEntry)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    INPUT_CONNECTION(OutputIdx+1, Scalar);
    if (!ScalarNode)
        return 0;

    granny_real32 Position = GetPosition(AtT);
    granny_real32 SampleT = ComputeSampleT(AtT, Position, ScalarNode, ScalarEdge);

    return ScalarNode->SampleScalarOutput(ScalarEdge, SampleT, TrackEntry);
}

bool GSTATE
timeselect::SampleEventOutput(granny_int32x            OutputIdx,
                              granny_real32            AtT,
                              granny_real32            DeltaT,
                              gstate_text_track_entry* EventBuffer,
                              granny_int32x const      EventBufferSize,
                              granny_int32x*           NumEvents)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    INPUT_CONNECTION(OutputIdx+1, Scalar);
    if (!ScalarNode)
        return 0;

    granny_real32 Position = GetPosition(AtT);
    granny_real32 SampleT = ComputeSampleT(AtT, Position, ScalarNode, ScalarEdge);

    return ScalarNode->SampleEventOutput(ScalarEdge, SampleT, DeltaT, EventBuffer, EventBufferSize, NumEvents);
}

bool GSTATE
timeselect::GetNextEvent(granny_int32x            OutputIdx,
                   granny_real32            AtT,
                   gstate_text_track_entry*  Event)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    INPUT_CONNECTION(OutputIdx+1, Event);
    if (!EventNode)
        return 0;

    granny_real32 Position = GetPosition(AtT);
    granny_real32 SampleT = ComputeSampleT(AtT, Position, EventNode, EventEdge);

    return EventNode->GetNextEvent(EventEdge, SampleT, Event);
}

bool GSTATE
timeselect::GetCloseEventTimes(granny_int32x  OutputIdx,
                               granny_real32  AtT,
                               char const*    TextToFind,
                               granny_real32* PrevTime,
                               granny_real32* NextTime)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    INPUT_CONNECTION(OutputIdx+1, Event);
    if (!EventNode)
        return 0;

    granny_real32 Position = GetPosition(AtT);
    granny_real32 SampleT = ComputeSampleT(AtT, Position, EventNode, EventEdge);

    return EventNode->GetCloseEventTimes(EventEdge, SampleT, TextToFind, PrevTime, NextTime);
}


