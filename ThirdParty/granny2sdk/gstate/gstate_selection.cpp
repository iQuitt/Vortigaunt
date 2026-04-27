// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_selection.h"

#include "gstate_anim_utils.h"
#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_snapshotutils.h"
#include "gstate_token_context.h"
#include "gstate_input_map.h"
#include "gstate_quick_vecs.h"
#include "gstate_parameters.h"
#include "gstate_telemetry.h"

#include <string.h>
#include <math.h>
#include <float.h>

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE selection::selectionImpl
{
    granny_int32 DummyMember;
    granny_int32  InputMapCount;
    granny_int32* InputMap;
    granny_int32  NumPoseInputs;
    granny_bool32 ChooseOnActivate;
};


granny_data_type_definition GSTATE
selection::selectionImplType[] =
{
    { GrannyInt32Member, "DummyMember" },
    { GrannyReferenceToArrayMember, "InputMap",  GrannyInt32Type },
    { GrannyInt32Member, "NumPoseInputs" }, 
    { GrannyBool32Member, "ChooseOnActivate" }, 
    { GrannyEndMember },
};

// selection is a concrete class, so we must create a slotted container
struct selection_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(selection);
};

granny_data_type_definition selection::selectionTokenType[] =
{
    DECL_UID_MEMBER(selection),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(selection),

    { GrannyEndMember }
};

IMPL_CREATE_NODE_BOILERPLATE(selection);

void GSTATE
selection::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(selection);

    InputMap_TakeTokenOwnership(m_selectionToken->InputMap, OldToken->InputMap, m_selectionToken->InputMapCount);
}

void GSTATE
selection::ReleaseOwnedToken_selection()
{
    InputMap_ReleaseOwnedToken(m_selectionToken->InputMap, m_selectionToken->InputMapCount);
}



void GSTATE
selection::CacheNumPoseInputs()
{
    TakeTokenOwnership();

    InputMap_CacheNumPoseInputs(this, m_selectionToken->NumPoseInputs);
}

GSTATE
selection::selection(token_context*               Context,
                     granny_data_type_definition* TokenType,
                     void*                        TokenObject,
                     token_ownership              TokenIsOwned)
  : parent(Context, TokenType, TokenObject, TokenIsOwned),
    m_selectionToken(0),
    m_ActivationParam(FLT_MAX)
{
    IMPL_INIT_FROM_TOKEN(selection);

    if (EditorCreated())
    {
        // Add our default output (goto parent, impl filters)
        AddInputImpl(ScalarEdge, "Param");
        granny_int32& NewInputMapping = QVecPushNewElement(m_selectionToken->InputMapCount, m_selectionToken->InputMap);
        NewInputMapping = 0;
    }

    //Backwards Compatibility
    if( IsEditable() )
    {
        if( m_selectionToken->InputMap == NULL )
        {
            TakeTokenOwnership();
            for( int InputIdx=0; InputIdx<GetNumInputs(); InputIdx++ )
            {
                granny_int32& NewInputMapping = QVecPushNewElement(m_selectionToken->InputMapCount, m_selectionToken->InputMap);
                NewInputMapping = InputIdx;
            }
        }

        if( m_selectionToken->NumPoseInputs == 0 )
        {
            CacheNumPoseInputs();
        }
    }
}


GSTATE
selection::~selection()
{
    DTOR_RELEASE_TOKEN(selection);
}


bool GSTATE
selection::FillDefaultToken(granny_data_type_definition* TokenType,
                            void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares selectionImpl*& Slot = // member
    GET_TOKEN_SLOT(selection);

    // Our slot in this token should be empty.
    // Create a new selection Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}


granny_int32x GSTATE
selection::AddInput(node_edge_type EdgeType, char const* EdgeName)
{
    return AddInputImpl(EdgeType, EdgeName);
}

bool GSTATE
selection::DeleteInput(granny_int32x InputIndex)
{
    if (InputIndex == 0)
        return false;

    node_edge_type RemovedInputType = GetInputType(InputIndex);

    if( !parent::DeleteInputImpl(InputIndex) )
    {
        return false;
    }

    // If it's a pose edge, auto-remove the corresponding morph edges
    if( RemovedInputType == PoseEdge )
    {
        if( !InputMap_DeleteInputSet(this, InputIndex, m_selectionToken->InputMap, m_selectionToken->InputMapCount, m_selectionToken->NumPoseInputs) )
            return false;
    }

    return true;
}

granny_int32x GSTATE
selection::AddOutput(node_edge_type EdgeType, char const* EdgeName)
{
    return parent::AddOutput(EdgeType, EdgeName);
}

bool GSTATE
selection::DeleteOutput(granny_int32x OutputIndex)
{
    return parent::DeleteOutput(OutputIndex);
}

bool GSTATE
selection::GetMorphChannelBindings(granny_int32x OutputIdx,
                                   char const**  MeshNames,
                                   granny_int32x MeshNameCount)
{
    //Inputs are: Param, Pose, Morph *, ... Pose, Morph *, so do + 1 to skip over the Parameter input and get to the 1st pose input. 
    granny_int32x InputIdx = GetInputIndexFromVisualIndex(OutputIdx + 1); 

    INPUT_CONNECTION(InputIdx, Morph);
    if (!MorphNode)
        return false;

    return MorphNode->GetMorphChannelBindings(MorphEdge, MeshNames, MeshNameCount);
}

granny_int32x GSTATE
selection::GetNumMorphChannels(granny_int32x OutputIdx)
{
    //Inputs are: Param, Pose, Morph *, ... Pose, Morph *, so do + 1 to skip over the Parameter input and get to the 1st pose input. 
    granny_int32x InputIdx = GetInputIndexFromVisualIndex(OutputIdx + 1); 

    INPUT_CONNECTION(InputIdx, Morph);
    if (!MorphNode)
        return -1;

    return MorphNode->GetNumMorphChannels(MorphEdge);
}

bool GSTATE
selection::SampleMorphOutput(granny_int32x  OutputIdx,
                             granny_real32  AtT,
                             granny_real32* MorphWeights,
                             granny_int32x NumMorphWeights)
{

    GStateAssert(MorphWeights);

    int Index;
    if (!ObtainSampleIndex(AtT, OutputIdx, &Index))
        return 0;

    GStateAssert(Index >= 1 && Index < GetNumInputs());

    node* SampleNode = 0;
    granny_int32x SampleEdge = -1;
    GetInputConnection(Index, &SampleNode, &SampleEdge);

    if (!SampleNode)
    {
        // Unconnected
        return 0;
    }

    return SampleNode->SampleMorphOutput(SampleEdge, AtT, MorphWeights, NumMorphWeights);
}

granny_real32 GSTATE
selection::SampleScalarOutput(granny_int32x OutputIdx,
                         granny_real32 AtT, 
                         gstate_scalar_track_entry * TrackEntry)
{
    int Index;
    if (!ObtainSampleIndex(AtT, OutputIdx, &Index))
        return 0;

    GStateAssert(Index >= 1 && Index < GetNumInputs());

    node* SampleNode = 0;
    granny_int32x SampleEdge = -1;
    GetInputConnection(Index, &SampleNode, &SampleEdge);

    if (!SampleNode)
    {
        // Unconnected
        return 0;
    }

    return SampleNode->SampleScalarOutput(SampleEdge, AtT, TrackEntry);
}

bool GSTATE
selection::GetScalarOutputRange(granny_int32x  OutputIdx,
                           granny_real32* OutMin,
                           granny_real32* OutMax)
{
    bool HadInputs = false;
    for( int InputIdx=0; InputIdx<GetNumInputs(); InputIdx++)
    {
        node* SampleNode = 0;
        granny_int32x SampleEdge = -1;
        GetInputConnection(1, &SampleNode, &SampleEdge);

        if (SampleNode && GetInputType(InputIdx) == ScalarEdge)
        {
            HadInputs = true;

            granny_real32 MinTest = -1.0f;
            granny_real32 MaxTest = -1.0f;
            SampleNode->GetScalarOutputRange(SampleEdge, &MinTest, &MaxTest);
            if( MinTest < *OutMin )
            {
                *OutMin  = MinTest;
            }
            if( MaxTest > *OutMax )
            {
                *OutMax  = MaxTest;
            }
        }
    }

    return HadInputs;
}

bool GSTATE
selection::SampleEventOutput(granny_int32x            OutputIdx,
                        granny_real32            AtT,
                        granny_real32            DeltaT,
                        gstate_text_track_entry* EventBuffer,
                        granny_int32x const      EventBufferSize,
                        granny_int32x*           NumEvents)
{
    int Index;
    if (!ObtainSampleIndex(AtT, OutputIdx, &Index))
        return 0;

    GStateAssert(Index >= 1 && Index < GetNumInputs());

    node* SampleNode = 0;
    granny_int32x SampleEdge = -1;
    GetInputConnection(Index, &SampleNode, &SampleEdge);

    if (!SampleNode)
    {
        // Unconnected
        return 0;
    }

    return SampleNode->SampleEventOutput(SampleEdge, AtT, DeltaT, EventBuffer, EventBufferSize, NumEvents);
}

bool GSTATE
selection::GetNextEvent(granny_int32x            OutputIdx,
                   granny_real32            AtT,
                   gstate_text_track_entry*  Event)
{
    int Index;
    if (!ObtainSampleIndex(AtT, OutputIdx, &Index))
        return 0;

    GStateAssert(Index >= 1 && Index < GetNumInputs());

    node* SampleNode = 0;
    granny_int32x SampleEdge = -1;
    GetInputConnection(Index, &SampleNode, &SampleEdge);

    if (!SampleNode)
    {
        // Unconnected
        return 0;
    }

    return SampleNode->GetNextEvent(SampleEdge, AtT, Event);
}


bool GSTATE
selection::GetAllEvents(granny_int32x            OutputIdx,
                   gstate_text_track_entry*  EventBuffer,
                   granny_int32x const      EventBufferSize,
                   granny_int32x*           NumEvents)
{
    GS_InvalidCodePath("GetAllEvents doesn't make sense for selection nodes");
    return false;
}

bool GSTATE
selection::GetCloseEventTimes(granny_int32x  OutputIdx,
                         granny_real32  AtT,
                         char const*    TextToFind,
                         granny_real32* PreviousTime,
                         granny_real32* NextTime)
{
    int Index;
    if (!ObtainSampleIndex(AtT, OutputIdx, &Index))
        return 0;

    GStateAssert(Index >= 1 && Index < GetNumInputs());

    node* SampleNode = 0;
    granny_int32x SampleEdge = -1;
    GetInputConnection(Index, &SampleNode, &SampleEdge);

    if (!SampleNode)
    {
        // Unconnected
        return 0;
    }

    return SampleNode->GetCloseEventTimes(SampleEdge, AtT, TextToFind, PreviousTime, NextTime);
}

bool GSTATE
selection::ObtainSampleIndex(granny_real32 AtT, granny_int32 OutputIdx, int* Index)
{
    INPUT_CONNECTION(0, Param);

    granny_real32 ParamMin = 0;
    granny_real32 ParamMax = 1;
    granny_real32 Param = 0;
    if (ParamNode == 0)
    {
        // If param is null no return index
        return false;
    }
    else
    {
        if (ParamNode->GetScalarOutputRange(ParamEdge, &ParamMin, &ParamMax) == false ||
            ParamMin == ParamMax)
        {
            ParamMin = 0;
            ParamMax = 1;
        }
        
        // Clamp the parameter
        float Value = 0;
        if( GetChooseOnActivate() && m_ActivationParam != FLT_MAX )
        {
            Value = m_ActivationParam; 
        }
        else
        {
            Value = ParamNode->SampleScalarOutput(ParamEdge, AtT, NULL);
            m_ActivationParam = Value;
        }
        Param = Clamp(ParamMin, Value, ParamMax);
    }

    // Put the min at 0
    GStateAssert(ParamMin < ParamMax);
    Param    -= ParamMin;
    ParamMax -= ParamMin;
    ParamMin  = 0;

    int NumInputs = m_selectionToken->NumPoseInputs;
    if (ParamMax == NumInputs-1)
    {
        // Special case to ensure that if the parameters are set to integers, we do this perfectly.
        *Index = (int)floor(Param);
    }
    else
    {
        granny_real32 NormalizedParam = (Param - ParamMin) / (ParamMax - ParamMin);

        // Choose the input that this parameter designates
        *Index = (int)floor(NormalizedParam * NumInputs);
        if (*Index >= NumInputs)
        {
            // Handle closed interval (i.e, 1.0)
            *Index = NumInputs - 1;
        }
    }
    GStateAssert(*Index >= 0 && *Index < NumInputs);

    *Index = InputMap_GetNthInputIndexForOutput(*Index, this, m_selectionToken->InputMap, m_selectionToken->InputMapCount, OutputIdx);

    return true;
}

granny_real32 GSTATE
selection::GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx)
{
    GStateAssert(OutputIdx == 0);

    int Index;
    if (!ObtainSampleIndex(AtT, OutputIdx, &Index))
        return 0;

    GStateAssert(Index >= 0 && Index < GetNumInputs());

    node* SampleNode = 0;
    granny_int32x SampleEdge = -1;
    GetInputConnection(Index, &SampleNode, &SampleEdge);

    if (!SampleNode)
    {
        // Unconnected
        return 0;
    }
    
    return SampleNode->GetLocalTimeOffset(AtT, SampleEdge);
}


granny_local_pose* GSTATE
selection::SamplePoseOutput(granny_int32x      OutputIdx,
                            granny_real32      AtT,
                            granny_real32      AllowedError,
                            granny_pose_cache* PoseCache,
                            granny_real32*     PoseWeight)
{
    GSTATE_AUTO_ZONE_FN_OR_NAME();

    GStateAssert(OutputIdx == 0);
    GStateAssert(PoseCache);

    int Index;
    if (!ObtainSampleIndex(AtT, OutputIdx, &Index))
        return 0;

    GStateAssert(Index >= 0 && Index < GetNumInputs());

    node* SampleNode = 0;
    granny_int32x SampleEdge = -1;
    GetInputConnection(Index, &SampleNode, &SampleEdge);

    if (!SampleNode)
    {
        // Unconnected
        return 0;
    }

    return SampleNode->SamplePoseOutput(SampleEdge, AtT, AllowedError, PoseCache, PoseWeight);
}

bool GSTATE
selection::GetRootMotionVectors(granny_int32x  OutputIdx,
                                granny_real32  AtT,
                                granny_real32  DeltaT,
                                granny_real32* ResultTranslation,
                                granny_real32* ResultRotation,
                                bool           Inverse)
{
    memset(ResultTranslation, 0, sizeof(granny_real32)*3);
    memset(ResultRotation,    0, sizeof(granny_real32)*3);

    int Index;
    if (!ObtainSampleIndex(AtT, OutputIdx, &Index))
    {
        return false;
    }
    GStateAssert(Index >= 1 && Index < GetNumInputs());

    node* SampleNode = 0;
    granny_int32x SampleEdge = -1;
    GetInputConnection(Index, &SampleNode, &SampleEdge);

    // Unconnected
    if (!SampleNode)
        return false;

    return SampleNode->GetRootMotionVectors(SampleEdge, AtT, DeltaT,
                                            ResultTranslation, ResultRotation,
                                            Inverse);
}

granny_real32 GSTATE
selection::GetDuration(granny_int32x OnOutput,
                       granny_real32 AtT)
{
    int Index;
    if (!ObtainSampleIndex(AtT, OnOutput, &Index))
        return -1;
    GStateAssert(Index >= 1 && Index < GetNumInputs());

    INPUT_CONNECTION(Index, Sample);
    if (!SampleNode)
        return -1;

    return SampleNode->GetDuration(SampleEdge, AtT);
}


bool GSTATE
selection::DidLoopOccur(granny_int32x OnOutput,
                        granny_real32 AtT,
                        granny_real32 DeltaT)
{
    GStateAssert(OnOutput == 0);

    int Index;
    if (!ObtainSampleIndex(AtT, OnOutput, &Index))
        return false;
    GStateAssert(Index >= 1 && Index < GetNumInputs());

    INPUT_CONNECTION(Index, Sample);
    if (!SampleNode)
        return false;

    return SampleNode->DidLoopOccur(OnOutput, AtT, DeltaT);
}

granny_int32 GSTATE
selection::GetInputIndexFromVisualIndex(granny_int32 Index) const
{
    int RemappedIndex = InputMap_GetInputIndex(Index, m_selectionToken->InputMap, m_selectionToken->InputMapCount);
    if( RemappedIndex >= 0 )
    {
        return RemappedIndex;
    }
    else
    {
        return Index;
    }
}

void GSTATE
selection::Activate(granny_real32 AtT)
{
    parent::Activate(AtT);

    if( GetChooseOnActivate() )
    {
        node* ParamNode = 0;
        granny_int32x ParamEdge = -1;
        GetInputConnection(0, &ParamNode, &ParamEdge);

        container * ParamNodeContainer = GSTATE_DYNCAST(ParamNode,container);

        container * Parent = GetParent();
        if( ParamNodeContainer == Parent )
        {
            node * ScalarNode = 0;
            granny_int32x ScalarEdge = -1;
            ParamNode->GetInputConnection(ParamEdge,&ScalarNode,&ScalarEdge);

            ParamNode = ScalarNode;
            ParamEdge = ScalarEdge;
        }

        if (ParamNode != 0)
        {
            //Do some simple repeat-avoidance. 

            int PrevIndex = -1;
            ObtainSampleIndex(AtT, 0, &PrevIndex);

            granny_int32x ParamTestEdge = ParamEdge;
            node * ParamTestNode = ParamNode;

            parameters * ParamTestNodeAsParameters = GSTATE_DYNCAST(ParamTestNode,parameters);
            while(ParamTestNode != NULL && ParamTestNodeAsParameters == NULL)
            {
                node * ScalarNode = 0;
                granny_int32x ScalarEdge = -1;
                ParamTestNode->GetInputConnection(ParamTestEdge,&ScalarNode,&ScalarEdge);
                ParamTestNode = ScalarNode;
                ParamTestEdge = ScalarEdge;
                ParamTestNodeAsParameters = GSTATE_DYNCAST(ParamTestNode,parameters);
            }

            if( ParamTestNodeAsParameters )
            {
                ParamTestNodeAsParameters->Activate(AtT);
            }

            m_ActivationParam = ParamNode->SampleScalarOutput(ParamEdge, AtT, NULL);

            int CurrIndex = -1;
            ObtainSampleIndex(AtT, 0, &CurrIndex);

            //APTODO: Should only do repeat tries if the parameter is a random parameter. 
            for( int RepeatTry=0; RepeatTry<3; RepeatTry++ )
            {
                if( CurrIndex == PrevIndex )
                {
                    m_ActivationParam = ParamNode->SampleScalarOutput(ParamEdge, AtT, NULL);

                    ObtainSampleIndex(AtT, 0, &CurrIndex);
                }
                else
                {
                    break;
                }
            }
        }
    }
}

void GSTATE
selection::AddInputSet()
{
    TakeTokenOwnership();

    InputMap_AddInputSet(this, m_selectionToken->InputMap, m_selectionToken->InputMapCount, m_selectionToken->NumPoseInputs);
}

void GSTATE
selection::AddInputsForOutput(granny_int32 OutputIdx)
{
    TakeTokenOwnership();

    InputMap_AddInputChannelToEachInputSet(this, OutputIdx, m_selectionToken->InputMap, m_selectionToken->InputMapCount);
}

void GSTATE
selection::DeleteInputsForOutput(granny_int32 OutputIdx)
{
    TakeTokenOwnership();

    InputMap_RemoveInputChannelFromEachInputSet(this, OutputIdx, m_selectionToken->InputMap, m_selectionToken->InputMapCount);
}

bool GSTATE
selection::GetChooseOnActivate() const
{
    return m_selectionToken->ChooseOnActivate != 0;
}

void GSTATE
selection::SetChooseOnActivate(bool ChooseOnActivate)
{
    TakeTokenOwnership();

    m_selectionToken->ChooseOnActivate = ChooseOnActivate;
}


void GSTATE
selection::SetInputNamesForOutput(granny_int32x OutputIdx, char const* NewEdgeName)
{
    TakeTokenOwnership();

    InputMap_SetNameForEachInputInInputSet(this, OutputIdx, NewEdgeName, m_selectionToken->InputMap, m_selectionToken->InputMapCount);
}


void GSTATE
selection::AddInputsForParentOutputs()
    {
    //Build the outputs first. Blend node bases its inputs on its outputs. 
    parent::AddOutputsForParentOutputs();

    AddInputSet();
    AddInputSet();
}
