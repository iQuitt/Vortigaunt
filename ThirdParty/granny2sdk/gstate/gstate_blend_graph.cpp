// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_blend_graph.h"

#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_token_context.h"
#include "gstate_selection.h"
#include "gstate_blend.h"
#include "gstate_telemetry.h"

#include <string.h>

#include "gstate_cpp_settings.h"
#include "granny.h"
USING_GSTATE_NAMESPACE;

struct GSTATE blend_graph::blend_graphImpl
{
    granny_int32 DummyMember;
    granny_bool32 AllowAnyTransition;
    granny_bool32 AllowTransitionExit;
    granny_real32 AnyTransitionDuration;
};

granny_data_type_definition GSTATE
blend_graph::blend_graphImplType[] =
{
    { GrannyInt32Member, "DummyMember" },
    { GrannyBool32Member,           "AllowAnyTransition" },
    { GrannyBool32Member,           "AllowTransitionExit" },
    { GrannyReal32Member,           "AnyTransitionDuration" },
    { GrannyEndMember },
};

// blend_graph is a concrete class, so we must create a slotted container
struct blend_graph_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_OPAQUE_TOKEN_SLOT(container);
    DECL_TOKEN_SLOT(blend_graph);
};

granny_data_type_definition blend_graph::blend_graphTokenType[] =
{
    DECL_UID_MEMBER(blend_graph),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(container),
    DECL_TOKEN_MEMBER(blend_graph),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(blend_graph);

IMPL_CREATE_NODE_BOILERPLATE(blend_graph);

GSTATE
blend_graph::blend_graph(token_context*                Context,
                         granny_data_type_definition* TokenType,
                         void*                        TokenObject,
                         token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_blend_graphToken(0)
{
    IMPL_INIT_FROM_TOKEN(blend_graph);
    {
        //LinkContainerChildren();
    }

    if (EditorCreated())
    {
        // Must go though real function to get the linkups...
        AddOutput(PoseEdge, "Pose");
    }
}

//Note there are similarities in the work done by UpgradeBlendNodes and AddOutput. Check the other one if you change this one. 
void GSTATE
blend_graph::AutoLinkOutputs()
{
    //Remember what outputs we add so that we only auto-link those. 
    //+1 here because we are re-using array to store the added output to the blend_graph too. 
    granny_int32x* AddedOutputs = GStateAllocArray(granny_int32x, GetNumChildren()+1);

    for(int OutputIdx = 0; OutputIdx < GetNumOutputs(); ++OutputIdx)
    {
        node_edge_type EdgeType = GetOutputType(OutputIdx);
        const char * EdgeName = GetOutputName(OutputIdx);

        if( EdgeType == PoseEdge || !IsOutputExternal(OutputIdx) )
        {
            continue;
        }

        int NthOutputWithSameName = 0;
        for(int MatchOutputIdx = 0; MatchOutputIdx <= OutputIdx; ++MatchOutputIdx)
        {
            if( IsOutputExternal(MatchOutputIdx) && GetOutputType(MatchOutputIdx) == EdgeType && _stricmp(GetOutputName(MatchOutputIdx),EdgeName)==0 )
            {
                NthOutputWithSameName++;
            }
        }

        AddedOutputs[GetNumChildren()] = OutputIdx;

        for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Child = GetChild(Idx);

            // Do not add outputs to parameter nodes
            if((GSTATE_DYNCAST(Child, parameters) != 0) ||
                (GSTATE_DYNCAST(Child, event_source) != 0))
            {
                continue;
            }


            int HadMatchingOutput = -1;
            for( int ChildOutputIdx=0; ChildOutputIdx<Child->GetNumOutputs(); ChildOutputIdx++ )
            {
                int NthChildOutputWithSameName = 0;
                for(int MatchOutputIdx = 0; MatchOutputIdx <= ChildOutputIdx; ++MatchOutputIdx)
                {
                    if( Child->IsOutputExternal(MatchOutputIdx) && Child->GetOutputType(MatchOutputIdx) == EdgeType && _stricmp(Child->GetOutputName(MatchOutputIdx),EdgeName)==0 )
                    {
                        NthChildOutputWithSameName++;
                    }
                }

                if( NthChildOutputWithSameName == NthOutputWithSameName && IsOutputExternal(OutputIdx) && Child->GetOutputType(ChildOutputIdx) == EdgeType && _stricmp(Child->GetOutputName(ChildOutputIdx),EdgeName) == 0 )
                {
                    HadMatchingOutput = ChildOutputIdx;
                    break;
                }
            }

            granny_int32x ChildOutputIdx = -1;
            if( HadMatchingOutput >= 0 )
            {
                ChildOutputIdx = HadMatchingOutput;
            }
            else
            {
                ChildOutputIdx = Child->AddOutput(EdgeType, EdgeName);
            }

            bool HadInput = false;
            for( int ChildInputIdx=0; ChildInputIdx<Child->GetNumInputs(); ChildInputIdx++ )
            {
                int NthChildInputWithSameName = 0;
                for(int MatchInputIdx = 0; MatchInputIdx <= ChildInputIdx; ++MatchInputIdx)
                {
                    if( Child->IsInputExternal(MatchInputIdx) && Child->GetInputType(MatchInputIdx) == EdgeType && _stricmp(Child->GetInputName(MatchInputIdx),EdgeName)==0 )
                    {
                        NthChildInputWithSameName++;
                    }
                }

                if( NthChildInputWithSameName == NthOutputWithSameName && Child->IsInputExternal(ChildInputIdx) && Child->GetInputType(ChildInputIdx) == EdgeType && _stricmp(Child->GetInputName(ChildInputIdx),EdgeName) == 0 )
                {
                    HadInput = true;
                    break;
                }
            }

            if( !HadInput && ChildOutputIdx >= 0 )
            {
                Child->AddInputsForOutput(ChildOutputIdx);
                AddedOutputs[Idx] = ChildOutputIdx;
            }
        }

        for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Child = GetChild(Idx);

            // Do not link outputs to parameter nodes
            if((GSTATE_DYNCAST(Child, parameters) != 0) ||
                (GSTATE_DYNCAST(Child, event_source) != 0))
            {
                continue;
            }

            //Now link up new inputs that got added for any nodes with connected poses. 
            //+1 here because we are re-using this loop to test for connections with the blend_graph too. 
            for (int ChildTestIdx = 0; ChildTestIdx < GetNumChildren()+1; ++ChildTestIdx)
            {
                node* ChildForInputTest = ChildTestIdx == GetNumChildren() ? this : GetChild(ChildTestIdx);

                for( int ChildTestInputIdx=0; ChildTestInputIdx<ChildForInputTest->GetNumInputs(); ChildTestInputIdx++ )
                {
                    bool InternalnessCorrect = ChildForInputTest == this ? ChildForInputTest->IsInputInternal(ChildTestInputIdx) : ChildForInputTest->IsInputExternal(ChildTestInputIdx);
                    if( InternalnessCorrect && ChildForInputTest->GetInputType(ChildTestInputIdx) == PoseEdge )
                    {
                        node * OtherNode = NULL;
                        granny_int32x OtherNodeOutputIdx = -1;
                        ChildForInputTest->GetInputConnection(ChildTestInputIdx, &OtherNode, &OtherNodeOutputIdx);

                        //Found a pose connection from OtherNode -> Child, auto-link it
                        if( OtherNode == Child )
                        {
                            node::AutoLinkBlendNode(OtherNode, OtherNodeOutputIdx, ChildForInputTest, ChildTestInputIdx, false, AddedOutputs[Idx]);
                        }
                    }
                }
            }        
        }
    }
    GStateDeallocate(AddedOutputs);

    parent::AutoLinkOutputs();
}

GSTATE
blend_graph::~blend_graph()
{
    DTOR_RELEASE_TOKEN(blend_graph);
}


bool GSTATE
blend_graph::FillDefaultToken(granny_data_type_definition* TokenType,
                              void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares blend_graphImpl*& Slot = // member
    GET_TOKEN_SLOT(blend_graph);

    // Our slot in this token should be empty.
    // Create a new blend_graph Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    // Initialize to default.  In a blendgraph, not much here...
    Slot->DummyMember = 0;
    Slot->AnyTransitionDuration = 0.25f;

    return true;
}

granny_int32x GSTATE
blend_graph::AddInput(node_edge_type EdgeType, char const* EdgeName)
{
    return parent::AddInput(EdgeType, EdgeName);
}

bool GSTATE
blend_graph::DeleteInput(granny_int32x InputIndex)
{
    return parent::DeleteInput(InputIndex);
}

// =============================================================================
// Published parameters (corresponds to edges other than 0)
// =============================================================================
//Note there are similarities in the work done by UpgradeBlendNodes and AddOutput. Check the other one if you change this one. 
granny_int32x GSTATE
blend_graph::AddOutput(node_edge_type EdgeType, char const* EdgeName)
{
    granny_int32x Result = parent::AddOutput(EdgeType, EdgeName);
    if( Result < 0 )
    {
        return -1;
    }

    if( node::GetAutoAddBlendGraphChildOutputs() )
    {
        //Remember what outputs we add so that we only auto-link those. 
        //+1 here because we are re-using array to store the added output to the blend_graph too. 
        granny_int32x* AddedOutputs = GStateAllocArray(granny_int32x, GetNumChildren()+1);
        AddedOutputs[GetNumChildren()] = Result;

        for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Child = GetChild(Idx);

            // Do not add outputs to parameter nodes
            if((GSTATE_DYNCAST(Child, parameters) != 0) ||
                (GSTATE_DYNCAST(Child, event_source) != 0))
            {
                continue;
            }

            granny_int32x ChildOutputIdx = Child->AddOutput(EdgeType, EdgeName);
            if( node::GetAutoAddBlendGraphChildInputsForOutputs() )
            {
                if( ChildOutputIdx >= 0 )
                {
                    Child->AddInputsForOutput(ChildOutputIdx);
                    AddedOutputs[Idx] = ChildOutputIdx;
                }
            }
        }

        for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Child = GetChild(Idx);

            // Do not link outputs to parameter nodes
            if((GSTATE_DYNCAST(Child, parameters) != 0) ||
               (GSTATE_DYNCAST(Child, event_source) != 0))
            {
                continue;
            }

            //Now link up new inputs that got added for any nodes with connected poses. 
            //+1 here because we are re-using this loop to test for connections with the blend_graph too. 
            for (int ChildTestIdx = 0; ChildTestIdx < GetNumChildren()+1; ++ChildTestIdx)
            {
                node* ChildForInputTest = ChildTestIdx == GetNumChildren() ? this : GetChild(ChildTestIdx);

                for( int ChildTestInputIdx=0; ChildTestInputIdx<ChildForInputTest->GetNumInputs(); ChildTestInputIdx++ )
                {
                    bool InternalnessCorrect = ChildForInputTest == this ? ChildForInputTest->IsInputInternal(ChildTestInputIdx) : ChildForInputTest->IsInputExternal(ChildTestInputIdx);
                    if( InternalnessCorrect && ChildForInputTest->GetInputType(ChildTestInputIdx) == PoseEdge )
                    {
                        node * OtherNode = NULL;
                        granny_int32x OtherNodeOutputIdx = -1;
                        ChildForInputTest->GetInputConnection(ChildTestInputIdx, &OtherNode, &OtherNodeOutputIdx);

                        //Found a pose connection from OtherNode -> Child, auto-link it
                        if( OtherNode == Child )
                        {
                            if( GetAutoLinkBlendNodeOutputs() )
                            {
                                node::AutoLinkBlendNode(OtherNode, OtherNodeOutputIdx, ChildForInputTest, ChildTestInputIdx, false, AddedOutputs[Idx]);
                            }
                        }
                    }
                }
            }
        }

        GStateDeallocate(AddedOutputs);
    }

    return Result;
}

bool GSTATE
blend_graph::DeleteOutput(granny_int32x OutputIndex)
{
    return parent::DeleteOutput(OutputIndex);
}

bool GSTATE
blend_graph::SetOutputName(granny_int32x OutputIdx, char const* NewEdgeName)
{
    if( !parent::SetOutputName(OutputIdx, NewEdgeName) )
    {
        return false;
    }

    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        node* Child = GetChild(Idx);

        //Some nodes don't match their outputs to parent container (parameter, animation, eventsource etc)
        if( OutputIdx < Child->GetNumOutputs() )
        {
            Child->SetOutputName( OutputIdx, NewEdgeName );
            Child->SetInputNamesForOutput( OutputIdx, NewEdgeName );
        }
    }

    return true;
}

int GSTATE
blend_graph::AddNewChild(node* Child)
{
    int Result = parent::AddNewChild(Child);

    if( Child->AllowSyncInputsWithParentOutputs() )
    {
        Child->AddInputsForParentOutputs();
    }

    if( Child->AllowSyncOutputsWithParentOutputs() )
    {
        Child->AddOutputsForParentOutputs();
    }

    return Result;
}

int GSTATE
blend_graph::AddCopiedChild(node* Child)
{
    // Ok, it passes, add it in.
    TakeTokenOwnership();

    //Clear out the parent. AddNewChild will set it. 
    Child->SetParent(NULL);

    for( int InputIdx=0; InputIdx<Child->GetNumInputs(); InputIdx++ )
    {
        node * OtherNode = NULL;
        int OtherNodeOutputIdx = 0;
        Child->GetInputConnection(InputIdx,&OtherNode,&OtherNodeOutputIdx);
    }

    int NewIndex = parent::AddNewChild(Child); //Use AddNewChild so we don't trigger blend_graph's AddNewChild (which parent::AddCopiedChild would invoke). 
    GStateAssert(NewIndex == GetNumChildren() - 1);  // other cases not yet handled.

    return NewIndex;
}

granny_real32 GSTATE
blend_graph::SampleScalarOutput(granny_int32x OutputIdx,
                                granny_real32 AtT,
                                gstate_scalar_track_entry * TrackEntry)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(GetOutputType(OutputIdx) == ScalarEdge);

    INPUT_CONNECTION(OutputIdx, Scalar);
    if (!ScalarNode)
        return 0.0f;

    return ScalarNode->SampleScalarOutput(ScalarEdge, AtT, TrackEntry);
}

bool GSTATE
blend_graph::GetScalarOutputRange(granny_int32x OutputIdx,
                                  granny_real32* OutMin,
                                  granny_real32* OutMax)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(GetOutputType(OutputIdx) == ScalarEdge);
    
    INPUT_CONNECTION(OutputIdx, Scalar);
    if (!ScalarNode)
        return 0.0f;

    return ScalarNode->GetScalarOutputRange(ScalarEdge, OutMin, OutMax);
}

bool GSTATE
blend_graph::GetAllowAnyTransition() const
{
    return (m_blend_graphToken->AllowAnyTransition != 0);
}

void GSTATE
blend_graph::SetAllowAnyTransition(bool Allow)
{
    TakeTokenOwnership();

    m_blend_graphToken->AllowAnyTransition = (Allow ? 1 : 0);

    //Ensure that we have an any transition transition in our transition list. 
    if( Allow )
    {
        EnsureAnyTransition();
    }
}

granny_real32 GSTATE
blend_graph::GetAnyTransitionDuration() const
{
    return m_blend_graphToken->AnyTransitionDuration;
}

void GSTATE
blend_graph::SetAnyTransitionDuration(granny_real32 Duration)
{
    TakeTokenOwnership();

    m_blend_graphToken->AnyTransitionDuration = Duration;
}

bool GSTATE
blend_graph::GetAllowTransitionExit() const
{
    return (m_blend_graphToken->AllowTransitionExit != 0);
}

void GSTATE
blend_graph::SetAllowTransitionExit(bool Allow)
{
    TakeTokenOwnership();

    m_blend_graphToken->AllowTransitionExit = (Allow ? 1 : 0);
}

bool GSTATE
blend_graph::GetMorphChannelBindings(granny_int32x OutputIdx,
                                     char const**  MeshNames,
                                     granny_int32x MeshNameCount)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(GetOutputType(OutputIdx) == MorphEdge);

    INPUT_CONNECTION(OutputIdx, Morph);
    if (!MorphNode)
        return false;

    return MorphNode->GetMorphChannelBindings(MorphEdge, MeshNames, MeshNameCount);
}

granny_int32x GSTATE
blend_graph::GetNumMorphChannels(granny_int32x OutputIdx)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(GetOutputType(OutputIdx) == MorphEdge);

    INPUT_CONNECTION(OutputIdx, Morph);
    if (!MorphNode)
        return -1;

    return MorphNode->GetNumMorphChannels(MorphEdge);
}

bool GSTATE
blend_graph::SampleMorphOutput(granny_int32x  OutputIdx,
                               granny_real32  AtT,
                               granny_real32* MorphWeights,
                               granny_int32x NumMorphWeights)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(GetOutputType(OutputIdx) == MorphEdge);

    INPUT_CONNECTION(OutputIdx, Morph);
    if (!MorphNode)
        return false;

    return MorphNode->SampleMorphOutput(MorphEdge, AtT, MorphWeights, NumMorphWeights);
}

granny_local_pose* GSTATE
blend_graph::SamplePoseOutput(granny_int32x      OutputIdx,
                              granny_real32      AtT,
                              granny_real32      AllowedError,
                              granny_pose_cache* PoseCache,
                              granny_real32*     PoseWeight)
{
    GSTATE_AUTO_ZONE_FN_OR_NAME();

    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(PoseCache);

    // Find the input that corresponds to this output.
    node* Node;
    granny_int32x EdgeIdx;
    if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx) || Node == 0)
    {
        // Return null, caller is responsible for building rest pose if necessary
        return 0;
    }

    GStateAssert(Node->GetOutputType(EdgeIdx) == PoseEdge);
    return Node->SamplePoseOutput(EdgeIdx, AtT, AllowedError, PoseCache, PoseWeight);
}

bool GSTATE
blend_graph::SampleMaskOutput(granny_int32x      OutputIdx,
                              granny_real32      AtT,
                              granny_track_mask* ModelMask)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    // Find the input that corresponds to this output.
    node* Node;
    granny_int32x EdgeIdx;
    if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
    {
        // todo: is this a warning?  full mask?
        return 0;
    }

    GStateAssert(Node->GetOutputType(EdgeIdx) == MaskEdge);
    return Node->SampleMaskOutput(EdgeIdx, AtT, ModelMask);
}


bool GSTATE
blend_graph::SampleEventOutput(granny_int32x            OutputIdx,
                               granny_real32            AtT,
                               granny_real32            DeltaT,
                               gstate_text_track_entry* EventBuffer,
                               granny_int32x const      EventBufferSize,
                               granny_int32x*           NumEvents)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    if (EventBuffer == 0 || EventBufferSize < 0 || NumEvents == 0)
    {
        GS_PreconditionFailed;
        return false;
    }

    // Find the input that corresponds to this output.
    node* Node;
    granny_int32x EdgeIdx;
    if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
    {
        *NumEvents = 0;
        return true;
    }

    GStateAssert(Node->GetOutputType(EdgeIdx) == EventEdge);
    return Node->SampleEventOutput(EdgeIdx, AtT, DeltaT, EventBuffer, EventBufferSize, NumEvents);
}


bool GSTATE
blend_graph::GetAllEvents(granny_int32x            OutputIdx,
                          gstate_text_track_entry*  EventBuffer,
                          granny_int32x const      EventBufferSize,
                          granny_int32x*           NumEvents)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    // Find the input that corresponds to this output.
    node* Node;
    granny_int32x EdgeIdx;
    if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
    {
        *NumEvents = 0;
        return true;
    }

    GStateAssert(Node->GetOutputType(EdgeIdx) == EventEdge);
    return Node->GetAllEvents(EdgeIdx, EventBuffer, EventBufferSize, NumEvents);
}


bool GSTATE
blend_graph::GetCloseEventTimes(granny_int32x  OutputIdx,
                                granny_real32  AtT,
                                char const*    TextToFind,
                                granny_real32* PreviousTime,
                                granny_real32* NextTime)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    // Find the input that corresponds to this output.
    node* Node;
    granny_int32x EdgeIdx;
    if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
    {
        return false;
    }

    GStateAssert(Node->GetOutputType(EdgeIdx) == EventEdge);
    return Node->GetCloseEventTimes(EdgeIdx, AtT, TextToFind, PreviousTime, NextTime);
}

bool GSTATE
blend_graph::GetRootMotionVectors(granny_int32x  OutputIdx,
                                  granny_real32  AtT,
                                  granny_real32  DeltaT,
                                  granny_real32* Translation,
                                  granny_real32* Rotation,
                                  bool Inverse)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    // Find the input that corresponds to this output.
    node* Node;
    granny_int32x EdgeIdx;
    if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx) || Node == 0)
    {
        return false;
    }

    GStateAssert(Node->GetOutputType(EdgeIdx) == PoseEdge);
    return Node->GetRootMotionVectors(EdgeIdx, AtT, DeltaT, Translation, Rotation, Inverse);
}


void GSTATE
blend_graph::Activate(granny_real32 AtT)
{
    parent::Activate(AtT);
    
    // Activate all child nodes...
    int NumChildren = GetNumChildren();
    for (int Idx = 0; Idx < NumChildren; ++Idx)
        GetChild(Idx)->Activate(AtT);
}

granny_real32 GSTATE
blend_graph::GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    // Find the input that corresponds to this output.
    node* Node;
    granny_int32x EdgeIdx;
    if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx) || Node == 0)
    {
        // Return null, caller is responsible for building rest pose if necessary
        return 0;
    }

    GStateAssert(Node->GetOutputType(EdgeIdx) == PoseEdge);
    return Node->GetLocalTimeOffset(AtT, EdgeIdx);
}


granny_real32 GSTATE
blend_graph::GetDuration(granny_int32x OnOutput, granny_real32 AtT)
{
    GStateAssert(OnOutput >= 0 && OnOutput < GetNumOutputs());

    INPUT_CONNECTION(OnOutput, Duration);
    if (!DurationNode)
        return 0.0f;

    return DurationNode->GetDuration(DurationEdge, AtT);
}

bool GSTATE
blend_graph::DidLoopOccur(granny_int32x OnOutput,
                          granny_real32 AtT,
                          granny_real32 DeltaT)
{
    GStateAssert(OnOutput >= 0 && OnOutput < GetNumOutputs());

    // Find the input that corresponds to this output.
    node* Node = 0;
    granny_int32x EdgeIdx;
    if (!ConnectedInput(OnOutput, &Node, &EdgeIdx) || Node == 0)
    {
        return false;
    }

    return Node->DidLoopOccur(EdgeIdx, AtT, DeltaT);
}

bool GSTATE
blend_graph::DidSubLoopOccur(node*         SubNode,
                             granny_int32  OnOutput,
                             granny_real32 AtT,
                             granny_real32 DeltaT)
{
    GStateAssert(SubNode != 0);

    GStateAssert(OnOutput >= 0 && OnOutput < SubNode->GetNumOutputs());
    if( !IsNodeActive(SubNode) )
    {
        return false;
    }

    return SubNode->DidLoopOccur(OnOutput, AtT, DeltaT);
}


void GSTATE
blend_graph::AdvanceT(granny_real32 CurrentTime, granny_real32 DeltaT)
{
    // Nothing to do for ourselves, but pass the info onto our children...
    {for (int Idx = 0; Idx < m_containerToken->ChildTokenCount; ++Idx)
    {
        GStateAssert(m_ChildNodes[Idx]);
        m_ChildNodes[Idx]->AdvanceT(CurrentTime, DeltaT);
    }}

    parent::AdvanceT(CurrentTime, DeltaT);
}


