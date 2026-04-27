// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_timeclamp.cpp $
// $DateTime: 2013/03/05 10:36:01 $
// $Change: 41645 $
// $Revision: #11 $
//
// $Notice: $
// ========================================================================
#include "gstate_timeclamp.h"

#include "gstate_anim_utils.h"
#include "gstate_node_visitor.h"
#include "gstate_token_context.h"
#include "gstate_telemetry.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE timeclamp::timeclampImpl
{
    granny_bool32 RangeIsRelative;
};

granny_data_type_definition GSTATE
timeclamp::timeclampImplType[] =
{
    { GrannyBool32Member, "RangeIsRelative" },
    { GrannyEndMember },
};

// timeclamp is a concrete class, so we must create a slotted container
struct timeclamp_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(timeclamp);
};

granny_data_type_definition timeclamp::timeclampTokenType[] =
{
    DECL_UID_MEMBER(timeclamp),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(timeclamp),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(timeclamp);
IMPL_CREATE_NODE_BOILERPLATE(timeclamp);


GSTATE
timeclamp::timeclamp(token_context*               Context,
                     granny_data_type_definition* TokenType,
                     void*                        TokenObject,
                     token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_timeclampToken(0)
{
    IMPL_INIT_FROM_TOKEN(timeclamp);

    if (EditorCreated())
    {
        // Add our default input/output (pass to parent, no dupe needed here)
        AddInputImpl(ScalarEdge, "MinTime");
        AddInputImpl(ScalarEdge, "MaxTime");

        AddInputImpl(PoseEdge, "Pose");
        AddOutputImpl(PoseEdge, "Pose");
    }
}


GSTATE
timeclamp::~timeclamp()
{
    DTOR_RELEASE_TOKEN(timeclamp);
}


bool GSTATE
timeclamp::FillDefaultToken(granny_data_type_definition* TokenType,
                                void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares timeclampImpl*& Slot = // member
    GET_TOKEN_SLOT(timeclamp);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}

bool GSTATE
timeclamp::GetRangeRelative() const
{
    return m_timeclampToken->RangeIsRelative != 0;
}

void GSTATE
timeclamp::SetRangeRelative(bool Relative)
{
    TakeTokenOwnership();

    m_timeclampToken->RangeIsRelative = Relative ? 1 : 0;
}

granny_real32 GSTATE
timeclamp::ComputeSampleT(granny_real32 const AtT,
                          node*               PoseNode,
                          granny_int32x       ConnectedEdge)
{
    GStateAssert(PoseNode);

    // Set default clamp parameters
    granny_real32 MinVal = 0;
    granny_real32 MaxVal = 1;

    INPUT_CONNECTION(0, Min);
    if (MinNode)
        MinVal = MinNode->SampleScalarOutput(MinEdge, AtT, NULL);

    INPUT_CONNECTION(1, Max);
    if (MaxNode)
        MaxVal = MaxNode->SampleScalarOutput(MaxEdge, AtT, NULL);


    // Ensure that minval <= maxval by just choosing minval for invalid range...  Note
    // that the sense of the test is deliberate to try to catch invalid floats.  Probably
    // futile.
    if (!(MinVal <= MaxVal))
        MaxVal = MinVal;
    
    granny_real32 const LocalT = AtT - PoseNode->GetLocalTimeOffset(AtT, ConnectedEdge);
    if (m_timeclampToken->RangeIsRelative)
    {
        granny_real32 Duration = PoseNode->GetDuration(ConnectedEdge, AtT);
        if (Duration > 0)
        {
            MinVal *= Duration;
            MaxVal *= Duration;
        }
    }

    return Clamp(MinVal, LocalT, MaxVal) + PoseNode->GetLocalTimeOffset(AtT, ConnectedEdge);
}

granny_real32 GSTATE
timeclamp::GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx)
{
    GStateAssert(OutputIdx == 0);

    INPUT_CONNECTION(2, Pose);
    if (!PoseNode)
        return 0;

    return PoseNode->GetLocalTimeOffset(AtT, PoseEdge);
}

granny_local_pose* GSTATE
timeclamp::SamplePoseOutput(granny_int32x      OutputIdx,
                            granny_real32      AtT,
                            granny_real32      AllowedError,
                            granny_pose_cache* PoseCache,
                            granny_real32*     PoseWeight)
{
    GSTATE_AUTO_ZONE_FN_OR_NAME();

    GStateAssert(PoseCache != 0);
    GStateAssert(OutputIdx == 0);

    INPUT_CONNECTION(2, Pose);
    if (!PoseNode)
        return 0;

    // Handles relative offset...
    granny_real32 SampleT = ComputeSampleT(AtT, PoseNode, PoseEdge);

    return PoseNode->SamplePoseOutput(PoseEdge, SampleT, AllowedError, PoseCache, PoseWeight);
}

bool GSTATE
timeclamp::GetRootMotionVectors(granny_int32x  OutputIdx,
                                granny_real32  AtT,
                                granny_real32  DeltaT,
                                granny_real32* Translation,
                                granny_real32* Rotation,
                                bool Inverse)
{
    GStateAssert(OutputIdx == 0);  // only one pose, please.

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

    INPUT_CONNECTION(2, Pose);
    if (!PoseNode)
        return false;

    // Handles relative offset.  Handle clamping, this double queries, which isn't ideal...
    granny_real32 SampleT    = ComputeSampleT(AtT, PoseNode, PoseEdge);
    granny_real32 OldSampleT = ComputeSampleT(AtT - DeltaT, PoseNode, PoseEdge);
    GStateAssert(OldSampleT <= SampleT);

    return PoseNode->GetRootMotionVectors(PoseEdge, SampleT, SampleT - OldSampleT, Translation, Rotation, Inverse);
}

granny_real32 GSTATE
timeclamp::GetDuration(granny_int32x OnOutput,
                       granny_real32 AtT)
{
    GStateAssert(OnOutput == 0);  // only one pose, please.

    INPUT_CONNECTION(2, Pose);
    if (!PoseNode)
        return -1;

    // Set default clamp parameters
    granny_real32 MinVal = 0;
    granny_real32 MaxVal = 1;
    INPUT_CONNECTION(0, Min);
    if (MinNode)
        MinVal = MinNode->SampleScalarOutput(MinEdge, AtT, NULL);

    INPUT_CONNECTION(1, Max);
    if (MaxNode)
        MaxVal = MaxNode->SampleScalarOutput(MaxEdge, AtT, NULL);

    // Ensure that minval <= maxval by just choosing minval for invalid range...  Note
    // that the sense of the test is deliberate to try to catch invalid floats.  Probably
    // futile.
    if (!(MinVal <= MaxVal))
        MaxVal = MinVal;
    
    if (m_timeclampToken->RangeIsRelative)
    {
        granny_real32 Duration = PoseNode->GetDuration(PoseEdge, AtT);
        if (Duration > 0)
        {
            MinVal *= Duration;
            MaxVal *= Duration;
        }
        else
        {
            return -1;
        }
    }
    GStateAssert(MaxVal >= MinVal);
    
    return (MaxVal - MinVal);
}

bool GSTATE
timeclamp::DidLoopOccur(granny_int32x OnOutput,
                        granny_real32 AtT,
                        granny_real32 DeltaT)
{
    GStateAssert(OnOutput == 0);  // only one pose, please.

    INPUT_CONNECTION(2, Pose);
    if (!PoseNode)
        return false;

    // Handles relative offset...
    // Handles relative offset.  Handle clamping, this double queries, which isn't ideal...
    granny_real32 SampleT    = ComputeSampleT(AtT, PoseNode, PoseEdge);
    granny_real32 OldSampleT = ComputeSampleT(AtT - DeltaT, PoseNode, PoseEdge);
    GStateAssert(OldSampleT <= SampleT);

    return PoseNode->DidLoopOccur(PoseEdge, SampleT, SampleT - OldSampleT);
}


granny_int32x GSTATE
timeclamp::AddInput(node_edge_type EdgeType, char const* EdgeName)
{
    // Forward this to AddOutput to eliminate dupes
    granny_int32x NewOutput = AddOutput(EdgeType, EdgeName);
    if (NewOutput == -1)
        return -1;

    GStateAssert(GS_InRange(NewOutput+2, GetNumInputs()));
    return NewOutput + 2;
}

bool GSTATE
timeclamp::DeleteInput(granny_int32x InputIdx)
{
    // Cannot delete the min/max inputs...
    if (InputIdx < 2)
        return false;

    GStateAssert(GS_InRange(InputIdx-2,GetNumOutputs()));
    return DeleteOutput(InputIdx - 2);
}


granny_int32x GSTATE
timeclamp::AddOutput(node_edge_type EdgeType, char const* EdgeName)
{
    // We actually route this to the parent at creation, but keep the check here
    if (GetNumOutputs() == 0 && EdgeType != PoseEdge)
        return -1;

    // Only one pose edge please
    if (GetNumOutputs() >= 1 && EdgeType == PoseEdge)
        return -1;

    // We don't do masks
    if (EdgeType == MaskEdge)
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
timeclamp::DeleteOutput(granny_int32x OutputIndex)
{
    // Never lose the pose edge...
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
timeclamp::AddInputsForOutput(granny_int32x OutputIndex)
{
    //Do nothing. In this class, adding outputs automatically adds inputs
}

void
timeclamp::DeleteInputsForOutput(granny_int32x OutputIndex)
{
    //Do nothing. In this class, deleting outputs automatically adds inputs
}

void GSTATE
timeclamp::SetInputNamesForOutput(granny_int32x OutputIndex, char const* NewEdgeName)
{
    SetInputName(OutputIndex + 2, NewEdgeName);
}

granny_int32x GSTATE
timeclamp::GetOutputPassthrough(granny_int32x OnOutput) const
{
    GStateAssert(GS_InRange(OnOutput, GetNumOutputs()));

    // Pose is always connected
    if (OnOutput == 0)
        return 2;

    INPUT_CONNECTION(2, Pose);
    if (PoseNode == 0)
        return -1;

    return OnOutput + 2;
}


IMPL_PASSTHROUGH_GETSCALAROUTPUTRANGE(timeclamp, 2, true);

granny_real32 GSTATE
timeclamp::SampleScalarOutput(granny_int32x OnOutput,
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
    
    // Handles relative offset...
    granny_real32 SampleT    = ComputeSampleT(AtT, PoseNode, PoseEdge);

    return ScalarNode->SampleScalarOutput(ScalarEdge, SampleT, NULL);
}


// These passthroughs do not reference T, so they can go straight through to the morph
// edge on the other side
IMPL_PASSTHROUGH_GETMORPHCHANNELBINDINGS(timeclamp, 2, true);
IMPL_PASSTHROUGH_GETNUMMORPHCHANNELS(timeclamp, 2, true);

bool GSTATE
timeclamp::SampleMorphOutput(granny_int32x  OnOutput,
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
    
    // Handles relative offset...
    granny_real32 SampleT = ComputeSampleT(AtT, PoseNode, PoseEdge);
    return MorphNode->SampleMorphOutput(MorphEdge, SampleT, MorphWeights, NumMorphWeights);
}

bool GSTATE
timeclamp::SampleEventOutput(granny_int32x            OnOutput,
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

    granny_real32 SampleT = ComputeSampleT(AtT, PoseNode, PoseEdge);
    granny_real32 OldT    = ComputeSampleT(AtT - DeltaT, PoseNode, PoseEdge);
    granny_real32 NewDelta = SampleT - OldT;

    return EventNode->SampleEventOutput(EventEdge, SampleT, NewDelta,
                                        EventBuffer, EventBufferSize, NumEvents);
}

bool GSTATE
timeclamp::GetNextEvent(granny_int32x OnOutput,
                        granny_real32 AtT,
                        gstate_text_track_entry*  Event)
{
    INPUT_CONNECTION(2, Pose);
    if (!PoseNode)
        return false;

    INPUT_CONNECTION(OnOutput+2, Event);
    if (!EventNode)
        return false;

    granny_real32 SampleT = ComputeSampleT(AtT, PoseNode, PoseEdge);
    return EventNode->GetNextEvent(EventEdge, SampleT, Event);
}

bool GSTATE
timeclamp::GetAllEvents(granny_int32x            OnOutput,
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
timeclamp::GetCloseEventTimes(granny_int32x  OnOutput,
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

    granny_real32 SampleT = ComputeSampleT(AtT, PoseNode, PoseEdge);

    // todo: project out into global time.
    return EventNode->GetCloseEventTimes(EventEdge, SampleT,
                                         TextToFind, PreviousTime, NextTime);
}
