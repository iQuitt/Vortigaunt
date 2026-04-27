// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_state_machine.h"

#include "gstate_anim_utils.h"
#include "gstate_blend_graph.h"
#include "gstate_character_instance.h"
#include "gstate_conditional.h"
#include "gstate_event_source.h"
#include "gstate_node_visitor.h"
#include "gstate_parameters.h"
#include "gstate_parent_condition.h"
#include "gstate_quick_vecs.h"
#include "gstate_snapshotutils.h"
#include "gstate_anim_source.h"
#include "gstate_token_context.h"
#include "gstate_transition.h"
#include "gstate_telemetry.h"
#include "gstate_transition_dynamic.h"
#include "gstate_telemetry.h"

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct edge_map
{
    granny_int32  EntryCount;
    granny_int32* Entries;
};
granny_data_type_definition EdgeMapType[] =
{
    { GrannyReferenceToArrayMember, "Entries", GrannyInt32Type },
    { GrannyEndMember }
};

struct GSTATE state_machine::state_machineImpl
{
    granny_int32  StartState;
    granny_bool32 UseLastActive;
    granny_bool32 RandomStartState;

    // One of these for each output of the state machine to relate them to the child node
    // outputs...
    granny_int32 OutputMapCount;
    edge_map*    OutputMaps;

    granny_int32    ConditionalCount;
    granny_variant* Conditionals;

    granny_bool32 AllowAnyTransition;
    granny_bool32 AllowTransitionExit;

    granny_bool32 AlwaysUseAnimEvents;
    granny_bool32 AutoForwardTransitionRequests;

    granny_real32 AnyTransitionDuration;

    granny_variant DynamicTransition;

    granny_int32 MorphMeshNameCount;
    char**       MorphMeshNames;

    granny_bool32 RetargetAnimationSources;
};

granny_data_type_definition GSTATE
state_machine::state_machineImplType[] =
{
    { GrannyInt32Member,            "StartState" },
    { GrannyBool32Member,           "UseLastActive" },
    { GrannyBool32Member,           "RandomStartState" },
    { GrannyReferenceToArrayMember, "OutputMaps", EdgeMapType },

    { GrannyReferenceToArrayMember, "Conditionals", GrannyVariantType },

    { GrannyBool32Member,           "AllowAnyTransition" },
    { GrannyBool32Member,           "AllowTransitionExit" },

    { GrannyBool32Member,           "AlwaysUseAnimEvents" },

    { GrannyBool32Member,           "AutoForwardTransitionRequests" },

    { GrannyReal32Member,           "AnyTransitionDuration" },

    { GrannyVariantReferenceMember, "DynamicTransition" },

    { GrannyReferenceToArrayMember, "MorphMeshNames", GrannyStringType },

    { GrannyBool32Member,           "RetargetAnimationSources" },

    { GrannyEndMember }
};

// state_machine is a concrete class, so we must create a slotted container
struct state_machine_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_OPAQUE_TOKEN_SLOT(container);
    DECL_TOKEN_SLOT(state_machine);
};

granny_data_type_definition state_machine::state_machineTokenType[] =
{
    DECL_UID_MEMBER(state_machine),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(container),
    DECL_TOKEN_MEMBER(state_machine),

    { GrannyEndMember }
};

bool GSTATE
state_machine::IsStateNode(node* Node)
{
    return ((GSTATE_DYNCAST(Node, parameters) == 0) &&
            (GSTATE_DYNCAST(Node, event_source) == 0));
}

static bool
IsParameterNode(node* Node)
{
    return !state_machine::IsStateNode(Node);
}

static edge_map&
EdgeMapForRawOutput(state_machine* Machine,
                    state_machine::state_machineImpl* Token,
                    granny_int32x RawOutputIndex)
{
    granny_int32x ExtOutput = Machine->WhichExternalOutput(RawOutputIndex);
    GStateAssert(ExtOutput != -1);
    GStateAssert(GS_InRange(ExtOutput, Token->OutputMapCount));

    return Token->OutputMaps[ExtOutput];
}


static granny_int32x
SampleIndexForNode(state_machine* Machine,
                   state_machine::state_machineImpl* Token,
                   node* Node,
                   granny_int32x RawOutputIdx)
{
    // Special case the pose output...
    if (RawOutputIdx == 0)
        return 0;

    granny_int32x ExtOutput   = Machine->WhichExternalOutput(RawOutputIdx);
    granny_int32x ExternalIdx = Node->GetNthExternalOutput(ExtOutput);

#if defined(DEBUG) && DEBUG
    //edge_map& EdgeMap = EdgeMapForRawOutput(Machine, Token, RawOutputIdx);
    //granny_int32x CachedIdx = EdgeMap.Entries[Machine->GetChildIdx(Node)];

    // // No mismatches allowed!
    // GStateAssert(CachedIdx == ExternalIdx);
#endif

    return ExternalIdx;
}

static granny_int32x
SampleIndexForTransition(state_machine* Machine,
                         state_machine::state_machineImpl* Token,
                         granny_int32x RawOutputIdx)
{
    return Machine->WhichExternalOutput(RawOutputIdx);
}



void GSTATE
state_machine::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(state_machine);

    GStateCloneArray(m_state_machineToken->OutputMaps,
                     OldToken->OutputMaps,
                     m_state_machineToken->OutputMapCount);
    {for (int Idx = 0; Idx < m_state_machineToken->OutputMapCount; ++Idx)
    {
        GStateCloneArray(m_state_machineToken->OutputMaps[Idx].Entries,
                         OldToken->OutputMaps[Idx].Entries,
                         m_state_machineToken->OutputMaps[Idx].EntryCount);
    }}

    GStateCloneArray(m_state_machineToken->Conditionals,
                     OldToken->Conditionals,
                     m_state_machineToken->ConditionalCount);

    GStateCloneArray(m_state_machineToken->MorphMeshNames,
        OldToken->MorphMeshNames,
        m_state_machineToken->MorphMeshNameCount);
    for (int Idx = 0; Idx < m_state_machineToken->MorphMeshNameCount; ++Idx)
    {
        GStateCloneString(m_state_machineToken->MorphMeshNames[Idx], OldToken->MorphMeshNames[Idx]);
    }
}

void GSTATE
state_machine::ReleaseOwnedToken_state_machine()
{
    for (int Idx = 0; Idx < m_state_machineToken->OutputMapCount; ++Idx)
    {
        GStateDeallocate(m_state_machineToken->OutputMaps[Idx].Entries);
    }

    GStateDeallocate(m_state_machineToken->OutputMaps);
    GStateDeallocate(m_state_machineToken->Conditionals);
    for (int Idx = 0; Idx < m_state_machineToken->MorphMeshNameCount; ++Idx)
    {
        GStateDeallocate(m_state_machineToken->MorphMeshNames[Idx]);
    }
    GStateDeallocate(m_state_machineToken->MorphMeshNames);
}

IMPL_CREATE_NODE_BOILERPLATE(state_machine);

void GSTATE
state_machine::DefaultStartState()
{
    TakeTokenOwnership();

    // All equally goofy, pick the non parameter node if possible, mark invalid if
    // not.
    m_state_machineToken->StartState = eInvalidChild;
    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        if (IsStateNode(GetChild(Idx)))
        {
            m_state_machineToken->StartState = Idx;
            break;
        }
    }
}

GSTATE
state_machine::state_machine(token_context*               Context,
                             granny_data_type_definition* TokenType,
                             void*                        TokenObject,
                             token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_state_machineToken(0),
    m_Active(0),
    m_Conditionals(0),
    m_PreferredExit(0),
    m_DynamicTransition(0),
    m_StartStateOverride(0),
    m_DTSourcePose(0),
    LastRandomStartStatesIndex(0)
{
    for( int i=0; i<sizeof(LastRandomStartStates)/sizeof(LastRandomStartStates[0]); i++ )
    {
        LastRandomStartStates[i] = -1;
    }

    m_DTSourceRootRotation[0] = m_DTSourceRootRotation[1] = m_DTSourceRootRotation[2] = 0.0f;

    // @@ complicated enough that this should be it's own function...
    IMPL_INIT_FROM_TOKEN(state_machine);

    {
        //Do nothing if creating a new state machine until it gets bound to the character. 
        //See BindtoCharacter() for that code. 
        bool CreatingNewStateMachineOrLoadingOldGSF = TokenOwnership == tokenized::eOwned2 || 
            m_state_machineToken->DynamicTransition.Object == 0;
        if (!CreatingNewStateMachineOrLoadingOldGSF)
        {
            tokenized* Product =
                GetTokenContext()->CreateFromToken(m_state_machineToken->DynamicTransition.Type,
                m_state_machineToken->DynamicTransition.Object);
            if (Product != 0)
            {
                m_DynamicTransition = GSTATE_DYNCAST(Product, tr_dynamic);
            }
        }

        m_DTSourceRootTranslation[0] = m_DTSourceRootTranslation[1] = m_DTSourceRootTranslation[2] = 0.0f;

        // Init our conditional cache...
        if (m_state_machineToken->ConditionalCount)
            GStateAllocZeroedArray(m_Conditionals, m_state_machineToken->ConditionalCount);

        // Now, create all of the conditionals.  Note that like
        // container::container we have to handle deleted node types.  See the
        // comment in that function for more info.
        for (int Idx = 0; Idx < m_state_machineToken->ConditionalCount;)
        {
            tokenized* Product =
                GetTokenContext()->CreateFromToken(m_state_machineToken->Conditionals[Idx].Type,
                m_state_machineToken->Conditionals[Idx].Object);
            if (Product != 0)
            {
                m_Conditionals[Idx] = GSTATE_DYNCAST(Product, conditional);
                GStateAssert(m_Conditionals[Idx]);
                GStateAssert(m_Conditionals[Idx]->GetOwner() == this);

                // Advance
                ++Idx;
            }
            else
            {
                GStateWarning("state_machine::state_machine: Found an invalid conditional, possibly obsolete class?\n");
                if ((Idx + 1) < m_state_machineToken->ConditionalCount)
                {
                    // Shift the rest of the array down
                    memmove(m_state_machineToken->Conditionals + Idx,
                        m_state_machineToken->Conditionals + (Idx+1),
                        (sizeof(*m_state_machineToken->Conditionals) *
                        (m_state_machineToken->ConditionalCount - (Idx+1))));
                }

                --m_state_machineToken->ConditionalCount;

                // This can cause ChildTokenCount to go to zero, if that occurs, NULL the
                // array pointer, we have no children.  Do NOT free.
                if (m_state_machineToken->ConditionalCount == 0)
                {
                    m_state_machineToken->Conditionals = 0;
                    GStateDeallocate(m_Conditionals);
                }
            }
        }

        // Note that there is a rare case in the container ctor in which tokens might not
        // be properly created, causing StartState to be out of range.  Detect and correct
        // here.  Subtlety: don't test against zero, because eInvalidChild (== -1) is a
        // valid entry for this field.
        if (m_state_machineToken->StartState >= GetNumChildren())
        {
            GStateWarning("state_machine::state_machine: StartState invalid, resetting to valid entry\n");
            DefaultStartState();
        }
    }

    // @@ don't let this get lost!
    if (EditorCreated())
    {
        AddOutput(PoseEdge, "Pose Output");
    }

    // Special case for upgrading old state_machines.  Remove this after a little bit.
    if (m_state_machineToken->OutputMapCount == 0)
    {
        // We can only do this in editable contexts.  The good news is that in
        // non-editable contexts, this shouldn't cause problems for sampling.
        if (IsEditable())
        {
            // Only this config is valid for upgrades...
            GStateAssert(GetNumOutputs() == 1 && GetOutputType(0) == PoseEdge);

            TakeTokenOwnership();

            edge_map& LastMap = QVecPushNewElement(m_state_machineToken->OutputMapCount,
                                                   m_state_machineToken->OutputMaps);
            LastMap.EntryCount = GetNumChildren();
            LastMap.Entries    = LastMap.EntryCount ? GStateAllocArray(granny_int32, LastMap.EntryCount) : 0;

            for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
            {
                node* Child = GetChild(Idx);
                LastMap.Entries[Idx] = Child->GetNthExternalOutput(0);
            }
        }
        else
        {
            GStateWarning("Detected an old-style state_machine Token(%s) use the studio to upgrade", GetName());
        }
    }

    if( GetParent() == NULL )
    {        
        LinkContainerChildren();
        for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Child = GetChild(Idx);
            GStateAssert(Child);

            Child->CaptureSiblingData();
        }
        
        /*
        LinkContainerChildren();
        for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Child = GetChild(Idx);
            GStateAssert(Child);

            container* ChildAsContainer = GSTATE_DYNCAST(Child, container);
            if( ChildAsContainer )
            {
                ChildAsContainer->LinkContainerChildren();
            }
            else
            {
                Child->CaptureSiblingData();
                Child->CaptureSiblingLinks();
            }
        }
        */

    }

    if (IsEditable())
    {
        TakeTokenOwnership();

        if( GetParent() == NULL && m_state_machineToken->MorphMeshNameCount <= GetNumOutputs() )
        {
            //Upgrading old state machine. Copy output names into morph mesh names. 
            for( int OutputIdx=m_state_machineToken->MorphMeshNameCount; OutputIdx<GetNumOutputs(); OutputIdx++ )
            {
                char *& NewMeshNameEntry = QVecPushNewElement(m_state_machineToken->MorphMeshNameCount, m_state_machineToken->MorphMeshNames);
                if( GetOutputType(OutputIdx) == MorphEdge )
                {
                    GStateCloneString(NewMeshNameEntry,GetOutputName(OutputIdx));
                }
                else
                {
                    NewMeshNameEntry = NULL;
                }
            }
        }
    }
}


GSTATE
state_machine::~state_machine()
{
    for (int Idx = 0; Idx < m_state_machineToken->ConditionalCount; ++ Idx)
    {
        GStateDelete<conditional>(m_Conditionals[Idx]);
        m_Conditionals[Idx] = 0;
    }
    GStateDeallocate(m_Conditionals);

    m_PreferredExit = 0;

    GStateDelete<tr_dynamic>(m_DynamicTransition);

    m_DTSourcePose = NULL;

    DTOR_RELEASE_TOKEN(state_machine);
}


bool GSTATE
state_machine::FillDefaultToken(granny_data_type_definition* TokenType,
                                    void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares state_machineImpl*& Slot = // member
    GET_TOKEN_SLOT(state_machine);

    // Our slot in this token should be empty.
    // Create a new state_machine token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    // Initialize to default (note that by default, we have no states)
    Slot->StartState = eInvalidChild;
    Slot->OutputMapCount = 0;
    Slot->OutputMaps = 0;
    Slot->ConditionalCount = 0;
    Slot->Conditionals = 0;
    Slot->AnyTransitionDuration = 0.25f;

    return true;
}

bool GSTATE
state_machine::BindToCharacter(gstate_character_instance* Instance)
{
    CreateDynamicTransition();

    if (!parent::BindToCharacter(Instance))
        return false;

    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        node* Child = GetChild(Idx);
        GStateAssert(Child);

        Child->CaptureSiblingData();
    }

    m_DTSourcePose = NULL; 

    return true;
}

void GSTATE
state_machine::NoteParameterNameChange(node* Param, int OutputIdx)
{
    GStateAssert(Param);
    GStateAssert(IsParameterNode(Param));
    GStateAssert(OutputIdx >= 0 && OutputIdx < Param->GetNumOutputs());

    char const* NewName = Param->GetOutputName(OutputIdx);

    // This one is actually really easy too.  Loop through, find connections, setinputname.  Done!
    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        node* Child = GetChild(Idx);

        for (int InputIndex = Child->GetNumInputs() - 1; InputIndex >= 0; --InputIndex)
        {
            if (Child->IsInputInternal(InputIndex))
                continue;

            node* OtherNode;
            granny_int32x OtherIdx;
            Child->GetInputConnection(InputIndex, &OtherNode, &OtherIdx);
            if (OtherNode == Param && OutputIdx == OtherIdx)
            {
                Child->SetInputName(InputIndex, NewName);
            }
        }
    }
}

bool GSTATE
state_machine::AddOutputToChildren(int NewOutputIdx)
{
    TakeTokenOwnership();

    edge_map& LastMap = QVecPushNewElement(m_state_machineToken->OutputMapCount,
                                           m_state_machineToken->OutputMaps);
    LastMap.EntryCount = GetNumChildren();
    LastMap.Entries = LastMap.EntryCount ? GStateAllocArray(granny_int32, LastMap.EntryCount) : 0;

    // We have to special case the 0th input, since pretty much everything pops out of
    // it's box with a pose output as it's 0th external, and we don't want to re-add it

    if (NewOutputIdx == 0)
    {
        GStateAssert(GetOutputType(NewOutputIdx) == PoseEdge);

        for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Child = GetChild(Idx);
            LastMap.Entries[Idx] = Child->GetNthExternalOutput(0);
        }
    }
    else
    {
        char const* Name = GetOutputName(NewOutputIdx);
        node_edge_type EdgeType = GetOutputType(NewOutputIdx);
        GStateAssert(EdgeType == ScalarEdge || EdgeType == EventEdge || EdgeType == MorphEdge);

        for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Child = GetChild(Idx);

            // Do *not* add outputs to parameter nodes, they handle their own stuff, and
            // we won't be (directly) querying them for scalar/event output
            if (IsParameterNode(Child))
            {
                LastMap.Entries[Idx] = -1;
                continue;
            }

            LastMap.Entries[Idx] = Child->AddOutput(EdgeType, Name);
        }
    }

    return true;
}

void GSTATE
state_machine::RefreshChildOutputName(granny_int32x OutputIdx)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    char const* OutputName = GetOutputName(OutputIdx);

    if (IsOutputInternal(OutputIdx))
    {
        for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Child = GetChild(Idx);
            for (int InputIdx = 0; InputIdx < Child->GetNumInputs(); InputIdx++)
            {
                INPUT_CONNECTION_ON(Child, InputIdx, Test);
                if (TestNode == this && TestEdge == OutputIdx)
                    Child->SetInputName(InputIdx, OutputName);
            }
        }
    }
    else
    {
        edge_map& EdgeMap = EdgeMapForRawOutput(this, m_state_machineToken, OutputIdx);

        for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Child = GetChild(Idx);

            if (IsParameterNode(Child))
            {
                GStateAssert(EdgeMap.Entries[Idx] == -1);
            }
            else
            {
                GStateAssert(GS_InRange(EdgeMap.Entries[Idx], Child->GetNumOutputs()));
                Child->SetOutputName(EdgeMap.Entries[Idx], OutputName);
            }
        }
    }
}



bool GSTATE
state_machine::RemoveOutputFromChildren(int DelOutputIndex)
{
    TakeTokenOwnership();

    if (IsOutputInternal(DelOutputIndex))
        return true;

    edge_map OldMap;
    {
        // Normally would use the EdgeMapForRawOutput, but we need that index in this case...
        granny_int32x ExtOutput = WhichExternalOutput(DelOutputIndex);
        GStateAssert(ExtOutput != -1 && ExtOutput != 0);
        GStateAssert(GS_InRange(ExtOutput-1, m_state_machineToken->OutputMapCount));

        OldMap = m_state_machineToken->OutputMaps[ExtOutput];
        QVecRemoveElement(ExtOutput,
                          m_state_machineToken->OutputMapCount,
                          m_state_machineToken->OutputMaps);
    }

    // We're going to make the following assumption.  When we remove an output from a
    // Child, all of its outputs *above* that index shift down by one.  This is true as of
    // right now, but it's very difficult to verify in general.  Note that
    // container::NoteOutputRemoval depends on this assumption as well, so there are
    // larger problems if that is ever not true.  It will get noticed.
    GStateAssert(GetNumChildren() == OldMap.EntryCount);
    {for (int Idx = 0; Idx < OldMap.EntryCount; ++Idx)
    {
        node* Child = GetChild(Idx);
        granny_int32x ChildOutputIdx = OldMap.Entries[Idx];

        // for whatever reason...
        if (ChildOutputIdx == -1)
            continue;

        GStateAssert(Child->IsOutputExternal(ChildOutputIdx));
        GStateAssert(Child->GetOutputType(ChildOutputIdx) == GetOutputType(DelOutputIndex));
        GStateAssert(strcmp(Child->GetOutputName(ChildOutputIdx), GetOutputName(DelOutputIndex)) == 0);

        // Delete that output from the child
        Child->DeleteOutput(ChildOutputIdx);

        // That will have cascaded through NoteOutputRemoval, all sibs are updated
        // already.
    }}

    // Free the arrays for OutputMap
    GStateDeallocate(OldMap.Entries);
    OldMap.EntryCount = -1;

    return true;
}


void GSTATE
state_machine::RemoveOutputFromConditionals(node* AffectedNode, int DelOutputIdx)
{
    // Substantially easier than the RemoveFromChildren method...

    // Shouldn't ever remove the pose output...
    GStateAssert(DelOutputIdx > 0 && DelOutputIdx < GetNumOutputs());

    {for (int Idx = 0; Idx < GetNumConditionals(); ++Idx)
    {
        conditional* Conditional = GetConditional(Idx);
        Conditional->Note_OutputEdgeDelete(AffectedNode, DelOutputIdx);
    }}
}

void GSTATE
state_machine::NoteOutputRemoval_Pre(node* AffectedNode, granny_int32x ToBeRemoved)
{
    TakeTokenOwnership();

    // We have to give any of our conditionals a change to see this change...
    {for (int Idx = 0; Idx < m_state_machineToken->ConditionalCount; ++Idx)
    {
        m_Conditionals[Idx]->Note_OutputEdgeDelete(AffectedNode, ToBeRemoved);
    }}

    // Adjust our output maps for this node...
    {
        int ChildIdx = GetChildIdx(AffectedNode);
        GStateAssert(ChildIdx != eInvalidChild);

        // Adjust the output maps...
        for (int Idx = 0; Idx < m_state_machineToken->OutputMapCount; ++Idx)
        {
            edge_map& OutputMap = m_state_machineToken->OutputMaps[Idx];
            GStateAssert(OutputMap.EntryCount == GetNumChildren());

            if (OutputMap.Entries[ChildIdx] == ToBeRemoved)
                OutputMap.Entries[ChildIdx] = -1;
            else if (OutputMap.Entries[ChildIdx] > ToBeRemoved)
                --OutputMap.Entries[ChildIdx];
        }
    }

    parent::NoteOutputRemoval_Pre(AffectedNode, ToBeRemoved);
}

void GSTATE
state_machine::NoteOutputRemoval_Post(node* AffectedNode, bool WasExternal)
{
    parent::NoteOutputRemoval_Post(AffectedNode, WasExternal);

    if (WasExternal)
        AdjustChildInputs();
}

void GSTATE
state_machine::NoteOutputAddition(node* AffectedNode, granny_int32x InsertionIndex)
{
    parent::NoteOutputAddition(AffectedNode, InsertionIndex);

    if (IsParameterNode(AffectedNode))
        AdjustChildInputs();
}

void GSTATE
state_machine::NoteDeleteTransition(transition* Transition)
{
    if (!Transition)
        return;

    // Clear this from the preferred state
    if (m_PreferredExit == Transition)
        m_PreferredExit = 0;

    // We actually have to zip through all of the transitions on our children to make sure
    // this isn't a preferred exit anywhere...
    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        node* Child = GetChild(Idx);
        for (int TransitionIdx = 0; TransitionIdx < Child->GetNumTransitions(); ++TransitionIdx)
        {
            transition* ChildTrans = Child->GetTransition(TransitionIdx);
            if (ChildTrans->GetPreferredExit() == Transition)
                ChildTrans->SetPreferredExit(0);
        }
    }
    
    // If the deleted transition isn't the active element, we don't care
    if (m_Active != Transition)
        return;

    // Just force this into the end state...
    ForceState(0, Transition->GetEndNode());
}


granny_uint32* GSTATE state_machine::CopyContext_CopiedItems = NULL;
granny_int32 GSTATE state_machine::CopyContext_NumCopiedItems = 0;
granny_uint32* GSTATE state_machine::CopyContext_MapKeys = NULL;
granny_uint32* GSTATE state_machine::CopyContext_MapValues = NULL;
granny_int32 GSTATE state_machine::CopyContext_NumMapItems = 0;

void GSTATE
state_machine::SetNodeCopyContext( granny_uint32 *CopiedItems, granny_int32 NumCopiedItems, granny_uint32* MapKeys, granny_uint32* MapValues, granny_int32 NumMapItems )
{
    CopyContext_CopiedItems = CopiedItems;
    CopyContext_NumCopiedItems = NumCopiedItems;
    CopyContext_MapKeys = MapKeys;
    CopyContext_MapValues = MapValues;
    CopyContext_NumMapItems = NumMapItems;
}

// Very close to AddNewChild, but we don't touch the inputs and outputs, which should be
// correct *already*.
int GSTATE
state_machine::AddCopiedChild(node* Child)
{
    // Ok, it passes, add it in.
    TakeTokenOwnership();

    //Clear out the parent. AddNewChild will set it. 
    Child->SetParent(NULL);

    int NewIndex = parent::AddNewChild(Child);
    GStateAssert(NewIndex == GetNumChildren() - 1);  // other cases not yet handled.

    // Modify the output edge maps...
    for (int Idx = 0; Idx < m_state_machineToken->OutputMapCount; ++Idx)
    {
        granny_int32& NewEntry = QVecPushNewElement(m_state_machineToken->OutputMaps[Idx].EntryCount,
            m_state_machineToken->OutputMaps[Idx].Entries);
        NewEntry = -1;
    }

    //Remove any outputs from the child that don't exist in this state_machine. 
    if(IsStateNode(Child))
    {
        //Remove any inputs to the child that don't exist in this state_machine. 
        for( int ChildInputIdx=0; ChildInputIdx < Child->GetNumInputs(); ChildInputIdx++ )
        {
            INPUT_CONNECTION_ON(Child, ChildInputIdx, Test);
            if (IsParameterNode(TestNode))
            {
                //Just leave it (actually need to hook it up to the copied version to be right)
                //Look up TestNode->GetUID() in the 
                //If TestNode is in CopiedItems, just leave it. 

                bool InputParameterWasCopied = false;
                granny_uint32 TestUID = TestNode->GetUID();
                for( int CopiedItemIdx=0; CopiedItemIdx<CopyContext_NumCopiedItems; CopiedItemIdx++ )
                {
                    if( CopyContext_CopiedItems[CopiedItemIdx] == TestUID )
                    {
                        InputParameterWasCopied = true;
                        break;
                    }
                }

                if( !InputParameterWasCopied )
                {
                    for( int MapIdx=0; MapIdx<CopyContext_NumMapItems; MapIdx++ )
                    {
                        if( CopyContext_MapValues[MapIdx] == TestUID )
                        {
                            granny_uint32 CopiedUID = CopyContext_MapKeys[MapIdx];
                            token_context* GlobalContext = token_context::GetGlobalContext();
                            node * CopiedNode = GSTATE_DYNCAST(GlobalContext->GetProductForUID(CopiedUID),node);
                            if( CopiedNode && CopiedNode->GetParent() == this )
                            {
                                Child->SetInputConnection(ChildInputIdx,CopiedNode,TestEdge);
                            }
                            else
                            {
                                Child->DeleteInput(ChildInputIdx);
                                ChildInputIdx--;
                            }
                            break;
                        }
                    }
                }
            }
            else if( Child->IsInputExternal(ChildInputIdx) )
            {
                node_edge_type ChildInputType = Child->GetInputType(ChildInputIdx);
                const char * ChildInputName = Child->GetInputName(ChildInputIdx);

                bool HadMatchingInput = false;
                for (int InputIdx= 0; InputIdx < GetNumInputs(); ++InputIdx)
                {
                    if( IsInputExternal(InputIdx) )
                    {
                        node_edge_type InputType = GetInputType(InputIdx);
                        const char * InputName = GetInputName(InputIdx);

                        if( ChildInputType == InputType && _stricmp(ChildInputName,InputName) == 0 )
                        {
                            HadMatchingInput = true;
                            Child->SetInputConnection(ChildInputIdx,this,InputIdx);
                            break;
                        }
                    }
                }

                if(!HadMatchingInput)
                {
                    if( Child->DeleteInput(ChildInputIdx) )
                    {
                        ChildInputIdx--;
                    }
                }
            }
        }

        for( int ChildOutputIdx=0; ChildOutputIdx < Child->GetNumOutputs(); ChildOutputIdx++ )
        {
            if( Child->IsOutputExternal(ChildOutputIdx) )
            {
                node_edge_type ChildOutputType = Child->GetOutputType(ChildOutputIdx);
                const char * ChildOutputName = Child->GetOutputName(ChildOutputIdx);

                bool HadMatchingOutput = false;
                for (int OutputIdx= 0; OutputIdx < GetNumOutputs(); ++OutputIdx)
                {
                    if( IsOutputExternal(OutputIdx) )
                    {
                        node_edge_type OutputType = GetOutputType(OutputIdx);
                        const char * OutputName = GetOutputName(OutputIdx);

                        if( ChildOutputType == OutputType && _stricmp(ChildOutputName,OutputName) == 0 )
                        {
                            HadMatchingOutput = true;
                            break;
                        }
                    }
                }

                if(!HadMatchingOutput)
                {
                    //It's possible someone renamed the zero-index pose output. Don't remove it in that case. That output is special. 
                    bool IsZeroIndexPoseOutput = ChildOutputIdx == 0 && ChildOutputType == PoseEdge;
                    if( !IsZeroIndexPoseOutput )
                    {
                        if( Child->DeleteOutput(ChildOutputIdx) )
                        {
                            ChildOutputIdx--;
                        }
                    }
                }
            }
        }

        // Add inputs so all names are represented, and hook up all child inputs to this state machine. 
        for (int OutputIdx= 0; OutputIdx < GetNumOutputs(); ++OutputIdx)
        {
            if( IsOutputInternal(OutputIdx) )
            {
                node_edge_type OutputType = GetOutputType(OutputIdx);
                const char * OutputName = GetOutputName(OutputIdx);

                int MatchingInputIdx = -1;
                for( int ChildInputIdx=0; ChildInputIdx< Child->GetNumInputs(); ChildInputIdx++ )
                {
                    if( Child->IsInputExternal(ChildInputIdx) )
                    {
                        node_edge_type ChildInputType = Child->GetInputType(ChildInputIdx);
                        const char * ChildInputName = Child->GetInputName(ChildInputIdx);
                        if( ChildInputType == OutputType && _stricmp(ChildInputName,OutputName) == 0 )
                        {
                            MatchingInputIdx = ChildInputIdx;                        
                        }
                        break;
                    }
                }

                if( MatchingInputIdx < 0 )
                {
                    int NewInputIdx = Child->AddInput(OutputType, OutputName);
                    if( NewInputIdx >= 0 )
                    {
                        Child->SetInputConnection(NewInputIdx, this, OutputIdx);
                    }
                }
                else
                {
                    Child->SetInputConnection(MatchingInputIdx, this, OutputIdx);
                }
            }
        }

        // Add outputs so all names are represented. 
        for (int OutputIdx= 0; OutputIdx < GetNumOutputs(); ++OutputIdx)
        {
            if( IsOutputExternal(OutputIdx) )
            {
                edge_map& EdgeMap = EdgeMapForRawOutput(this, m_state_machineToken, OutputIdx);

                node_edge_type OutputType = GetOutputType(OutputIdx);
                const char * OutputName = GetOutputName(OutputIdx);

                bool HadMatchingOutput = false;
                int MatchingOutputIndex = -1;
                for( int ChildOutputIdx=0; ChildOutputIdx< Child->GetNumOutputs(); ChildOutputIdx++ )
                {
                    if( Child->IsOutputExternal(ChildOutputIdx) )
                    {
                        //Don't re-use outputs more than once. 
                        bool EdgeUsed = false;
                        for(int EdgeMapIdx=0; EdgeMapIdx < EdgeMap.EntryCount; ++EdgeMapIdx)
                        {
                            if( EdgeMap.Entries[EdgeMapIdx] == ChildOutputIdx )
                            {
                                EdgeUsed = true;
                            }
                        }

                        if( EdgeUsed )
                        {
                            HadMatchingOutput = true;
                            MatchingOutputIndex = ChildOutputIdx;
                        }
                        else
                        {
                            node_edge_type ChildOutputType = Child->GetOutputType(ChildOutputIdx);
                            const char * ChildOutputName = Child->GetOutputName(ChildOutputIdx);
                            if( ChildOutputType == OutputType && _stricmp(ChildOutputName,OutputName) == 0 )
                            {
                                HadMatchingOutput = true;
                                MatchingOutputIndex = ChildOutputIdx;
                                break;
                            }
                            bool IsZeroIndexPoseOutput = OutputIdx == 0 && ChildOutputIdx == 0 && ChildOutputType == PoseEdge;
                            if( IsZeroIndexPoseOutput )
                            {
                                HadMatchingOutput = true;
                                MatchingOutputIndex = ChildOutputIdx;
                                break;
                            }
                        }
                    }
                }

                if( !HadMatchingOutput )
                {
                    EdgeMap.Entries[NewIndex] = Child->AddOutput(OutputType, OutputName);
                }
                else
                {
                    EdgeMap.Entries[NewIndex] = MatchingOutputIndex; //GetChildIdx(Child);
                }
            }
        }
    }

    // Validate the added node
    {
        // Inputs for complicated children should be correctly wired.
        if (GSTATE_DYNCAST(Child, blend_graph) || GSTATE_DYNCAST(Child, state_machine))
        {
            for (int Idx = 0; Idx < Child->GetNumInputs(); ++Idx)
            {
                if (Child->IsInputInternal(Idx))
                    continue;

                INPUT_CONNECTION_ON(Child, Idx, Test);
                if (TestNode == 0 ||
                    (TestNode != this && !IsParameterNode(TestNode)))
                {
                    GS_InvalidCodePath("Input Mismatch");
                    return -1;
                }
            }
        }

        // Check the outputs for all non-parameter node types...
        if( !IsParameterNode(Child) )
        {
            for (int Idx = 0; Idx < GetNumOutputs(); ++Idx)
            {
                if (IsOutputInternal(Idx))
                    continue;

                int ExternalIdx = WhichExternalOutput(Idx);
                if (ExternalIdx == 0)
                    continue;

                int ChildIdx = Child->GetNthExternalOutput(ExternalIdx);
                if (ChildIdx == -1)
                {
                    GS_InvalidCodePath("Output Mismatch");
                    return -1;
                }

                if (Child->GetOutputType(ChildIdx) != GetOutputType(Idx) ||
                    strcmp(Child->GetOutputName(ChildIdx), GetOutputName(Idx)) != 0)
                {
                    GS_InvalidCodePath("Output Mismatch");
                    return -1;
                }
            }
        }
    }

    for (int ChildIdx = 0; ChildIdx < GetNumChildren(); ++ChildIdx)
    {
        node* IntChild = GetChild(ChildIdx);
        if (IsParameterNode(IntChild))
            AddInputsForNode(IntChild);
    }

    // Hook up inputs and outputs with matching names
    if (IsParameterNode(Child))
    {
        AdjustChildInputs();
    }
    else
    {
        // Normal state...
        if (NewIndex != eInvalidChild && m_state_machineToken->StartState == eInvalidChild)
        {
            m_state_machineToken->StartState = NewIndex;

            // We can also just make this the active element...
            SetActive( Child );
        }
    }

    GStateAssert(CheckConnections());
    return NewIndex;

}

int GSTATE
state_machine::AddNewChild(node* Child)
{
    TakeTokenOwnership();

    int NewIndex = parent::AddNewChild(Child);

    // Modify the output edge maps...
    GStateAssert(NewIndex == GetNumChildren() - 1);  // other cases not yet handled.
    {for (int Idx = 0; Idx < m_state_machineToken->OutputMapCount; ++Idx)
    {
        granny_int32& NewEntry = QVecPushNewElement(m_state_machineToken->OutputMaps[Idx].EntryCount,
                                                    m_state_machineToken->OutputMaps[Idx].Entries);
        NewEntry = -1;
    }}

    if (IsParameterNode(Child))
    {
        AdjustChildInputs();

        // It doesn't alter the output map, the entry is set to -1 already, and we don't
        // add outputs to the parameter based on the state machine output.
    }
    else
    {
        // Add output edges other than the first pose.  This also alters the output map, of course...
        {
            for (int Idx = 0; Idx < GetNumOutputs(); ++Idx)
            {
                if (Idx == 0)
                {
                    continue;  // skip that first pose edge, arg.
                }

                if (IsOutputInternal(Idx))
                {
                    AddInputForNodeChild(this, Idx, Child);
                }
                else
                {
                    edge_map& EdgeMap = EdgeMapForRawOutput(this, m_state_machineToken, Idx);
                    EdgeMap.Entries[NewIndex] = Child->AddOutput(GetOutputType(Idx), GetOutputName(Idx));
                }
            }
        }

        // Normal state...
        if (NewIndex != eInvalidChild && m_state_machineToken->StartState == eInvalidChild)
        {
            m_state_machineToken->StartState = NewIndex;

            // We can also just make this the active element...
            SetActive( Child );
        }
    }

    GStateAssert(CheckConnections());

    return NewIndex;
}

bool GSTATE
state_machine::RemoveChildByIdx(int ChildIdx)
{
    TakeTokenOwnership();

    // We need to get this by pointer before removing to check against active state
    node* RemChild = GetChild(ChildIdx);

    // Further, if this is a parameter, we need to nuke the connected inputs of our children
    if (IsStateNode(RemChild))
    {
        // This is *technically* not strictly necessary, since removing the child right
        // now *only* means deleting it, but in case we add copy/paste later, we might be
        // stuffing this child back in somewhere *else*.  So be a good citizen.  Note that
        // normally we would have to jump through numerous hoops, but since we know that
        // we'll be peeling this down to just a pose output, this is easy.
        bool Removed;
        do
        {
            Removed = false;
            {for (int Idx = 0; Idx < RemChild->GetNumOutputs(); ++Idx)
            {
                if (Idx == RemChild->GetNthExternalOutput(0) || RemChild->IsOutputInternal(Idx))
                    continue;

                Removed = true;
                RemChild->DeleteOutput(Idx);
                break;
            }}
        } while (Removed);

        // Delete any parameter edges on this node
        {for (int Idx = RemChild->GetNumInputs() - 1; Idx >= 0; --Idx)
        {
            if (RemChild->IsInputInternal(Idx))
                continue;

            node* OtherNode;
            granny_int32x OtherIdx;
            RemChild->GetInputConnection(Idx, &OtherNode, &OtherIdx);
            if (IsParameterNode(OtherNode))
                RemChild->DeleteInput(Idx);
        }}
    }
    else
    {
        // It's a parameter or event node, let the conditionals know that it's going
        // away...
        for (int Idx = 0; Idx < GetNumConditionals(); ++Idx)
        {
            conditional* Conditional = GetConditional(Idx);
            Conditional->Note_NodeDelete(RemChild);
        }
    }

    // Notify any nodes that refer to conditionals in this node that they are going away. 
    // Delete our conditionals...
    // APTODO: What about external conditionals that point to nodes *inside* something we're deleting? FFS. 
    container * RemChildAsContainer = GSTATE_DYNCAST(RemChild, container);
    if( RemChildAsContainer )
    {
        RemChildAsContainer->NoteContainerDeleted();
    }


    // Remove the child from our output maps...
    {for (int Idx = 0; Idx < m_state_machineToken->OutputMapCount; ++Idx)
    {
        edge_map& Map = m_state_machineToken->OutputMaps[Idx];
        QVecRemoveElement(ChildIdx, Map.EntryCount, Map.Entries);
    }}

    bool RemoveSuccess = parent::RemoveChildByIdx(ChildIdx);

    // Make sure that we don't have to remove any inputs from our children
    AdjustChildInputs();

    // Either the start state went away, or the array will shift down, and we have to
    // account for that.
    if (m_state_machineToken->StartState == ChildIdx)
    {
        // Totally gone.  Find the first one that isn't a parameter node
        DefaultStartState();
    }
    else if (m_state_machineToken->StartState > ChildIdx)
    {
        // Just shift down...
        GStateAssert(GetNumChildren() != 0);
        m_state_machineToken->StartState -= 1;
    }

    if (m_Active == RemChild)
    {
        if (m_state_machineToken->StartState != eInvalidChild)
            SetActive( GetChild(m_state_machineToken->StartState) );
        else
            SetActive( 0 );
    }

    return RemoveSuccess;
}


int GSTATE
state_machine::GetStartStateIdx() const
{
    return m_state_machineToken->StartState;
}


void GSTATE
state_machine::SetStartStateIdx(int StartState)
{
    GStateAssert(StartState == eInvalidChild ||
                 (StartState >= 0 && StartState < GetNumChildren()));

    TakeTokenOwnership();
    m_state_machineToken->StartState = StartState;
}

void GSTATE
state_machine::SetStartState(node* State)
{
    if (IsStateNode(State) == false)
    {
        GS_InvalidCodePath("tried to set start state to a non-State class");
        return;
    }

    int ChildIdx = GetChildIdx(State);
    GStateAssert(ChildIdx != eInvalidChild);
    SetStartStateIdx(ChildIdx);
}

void GSTATE
state_machine::SetStartStateOverride(node* State)
{
    if (IsStateNode(State) == false)
    {
        GS_InvalidCodePath("tried to set start state to a non-State class");
        return;
    }

    m_StartStateOverride = State;
}

node* GSTATE
state_machine::GetStartStateOverride()
{
    return m_StartStateOverride;
}

bool GSTATE
state_machine::GetUseLastActive() const
{
    return (m_state_machineToken->UseLastActive != 0);
}

void GSTATE
state_machine::SetUseLastActive(bool UseLast)
{
    TakeTokenOwnership();

    m_state_machineToken->UseLastActive = (UseLast ? 1 : 0);
}

bool GSTATE
state_machine::GetRandomStartState() const
{
    return (m_state_machineToken->RandomStartState != 0);
}

void GSTATE
state_machine::SetRandomStartState(bool RandomStartState)
{
    TakeTokenOwnership();

    m_state_machineToken->RandomStartState = (RandomStartState ? 1 : 0);
}

bool GSTATE
state_machine::GetRetargetAnimationSources() const
{
    return (m_state_machineToken->RetargetAnimationSources != 0);
}

void GSTATE
state_machine::SetRetargetAnimationSources(bool RetargetAnimationSources)
{
    TakeTokenOwnership();

    m_state_machineToken->RetargetAnimationSources = (RetargetAnimationSources ? 1 : 0);
}

bool GSTATE
state_machine::GetAllowAnyTransition() const
{
    return (m_state_machineToken->AllowAnyTransition != 0);
}

void GSTATE
state_machine::SetAllowAnyTransition(bool Allow)
{
    TakeTokenOwnership();

    m_state_machineToken->AllowAnyTransition = (Allow ? 1 : 0);

    //Ensure that we have an any transition transition in our transition list. 
    if( Allow )
    {
        EnsureAnyTransition();
    }
}

granny_real32 GSTATE
state_machine::GetAnyTransitionDuration() const
{
    return m_state_machineToken->AnyTransitionDuration;
}

void GSTATE
state_machine::SetAnyTransitionDuration(granny_real32 Duration)
{
    TakeTokenOwnership();

    m_state_machineToken->AnyTransitionDuration = Duration;
}

bool GSTATE
state_machine::GetAllowTransitionExit() const
{
    return (m_state_machineToken->AllowTransitionExit != 0);
}

void GSTATE
state_machine::SetAllowTransitionExit(bool Allow)
{
    TakeTokenOwnership();

    m_state_machineToken->AllowTransitionExit = (Allow ? 1 : 0);
}

bool GSTATE
state_machine::GetAlwaysUseAnimEvents() const
{
    return (m_state_machineToken->AlwaysUseAnimEvents != 0);
}

void GSTATE
state_machine::SetAlwaysUseAnimEvents(bool Use)
{
    m_state_machineToken->AlwaysUseAnimEvents = (Use ? 1 : 0);
}

bool GSTATE
state_machine::GetAutoForwardTransitionRequests() const
{
    return (m_state_machineToken->AutoForwardTransitionRequests != 0);
}

void GSTATE
state_machine::SetAutoForwardTransitionRequests(bool Use)
{
    m_state_machineToken->AutoForwardTransitionRequests = (Use ? 1 : 0);
}

void GSTATE
state_machine::CreateDynamicTransition()
{
    if (m_DynamicTransition == NULL)
    {
        //Only write the token data back to the definition in the tool. Never at runtime.
        if (IsEditable())
        {
            m_DynamicTransition = tr_dynamic::DefaultInstance();

            m_DynamicTransition->GetTypeAndToken(&m_state_machineToken->DynamicTransition);
        }
    }
}

const char * GSTATE
state_machine::GetMorphMeshName(int MorphOutputIdx)
{
    GStateAssert(GetParent() == NULL);
    GStateAssert(MorphOutputIdx >= 0 && MorphOutputIdx < m_state_machineToken->MorphMeshNameCount );

    if(MorphOutputIdx < 0 || MorphOutputIdx >= m_state_machineToken->MorphMeshNameCount)
    {
        return "";
    }

    return m_state_machineToken->MorphMeshNames[MorphOutputIdx];
}

void GSTATE
state_machine::SetMorphMeshName(int MorphOutputIdx, const char * MeshName)
{
    GStateAssert(GetParent() == NULL);
    GStateAssert(MorphOutputIdx >= 0 && MorphOutputIdx < m_state_machineToken->MorphMeshNameCount);

    TakeTokenOwnership();

    GStateReplaceString(m_state_machineToken->MorphMeshNames[MorphOutputIdx],MeshName);
}

const char * GSTATE
state_machine::GetMorphMeshNameForOutputName(const char * OutputName)
{
    //GetMorphMeshNameForOutputName is only defined on the root state machine
    GStateAssert(GetParent() == NULL);

    for(int OutputIdx=0; OutputIdx<GetNumOutputs(); OutputIdx++)
    {
        if( GetOutputType(OutputIdx) == MorphEdge )
        {
            if( _stricmp(OutputName, GetOutputName(OutputIdx)) == 0 )
            {
                return GetMorphMeshName(OutputIdx);
            }
        }
    }
    return "";
}


tokenized* GSTATE
state_machine::GetActiveElement()
{
    return m_Active;
}

bool GSTATE
state_machine::CheckConnections()
{
    if (!parent::CheckConnections())
        return false;

    // Enforce the output map constraints
    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        node* Child = GetChild(Idx);
        if (IsParameterNode(Child))
            continue;

        for (int OutputIdx = 1; OutputIdx < GetNumOutputs(); ++OutputIdx)
        {
            if (IsOutputInternal(OutputIdx))
                continue;

            granny_int32x ChildOutputIdx = SampleIndexForNode(this, m_state_machineToken, Child, OutputIdx);

            if (!(Child->IsOutputExternal(ChildOutputIdx)) &&
                (Child->GetOutputType(ChildOutputIdx) == GetOutputType(OutputIdx)) &&
                (strcmp(Child->GetOutputName(ChildOutputIdx), GetOutputName(OutputIdx)) == 0))
            {
                GS_InvalidCodePath("Invalid s/m output connection");
                return false;
            }
        }
    }

    return true;
}

bool GSTATE
state_machine::IsDynamicTransitionActive()
{ 
    return m_Active == m_DynamicTransition && m_Active != NULL; 
}

bool GSTATE
state_machine::MoveNodeToFront(node* Node)
{
    int ChildIdx = GetChildIdx(Node);
    GStateAssert(ChildIdx != eInvalidChild);

    TakeTokenOwnership();

    if (m_state_machineToken->StartState == ChildIdx)
    {
        m_state_machineToken->StartState = 0;
    }
    else if (m_state_machineToken->StartState != eInvalidChild &&
             m_state_machineToken->StartState < ChildIdx)
    {
        ++m_state_machineToken->StartState;
    }

    bool RetVal = parent::MoveNodeToFront(Node);

    // Adjust the output maps...
    for (int Idx = 0; Idx < m_state_machineToken->OutputMapCount; ++Idx)
    {
        edge_map& OutputMap = m_state_machineToken->OutputMaps[Idx];
        GStateAssert(OutputMap.EntryCount == GetNumChildren());

        granny_int32 StoreOutput = OutputMap.Entries[ChildIdx];
        for (int EntryIdx = ChildIdx; EntryIdx >= 1; --EntryIdx)
        {
            OutputMap.Entries[EntryIdx] = OutputMap.Entries[EntryIdx-1];
        }
        OutputMap.Entries[0] = StoreOutput;
    }

    return RetVal;
}

node* GSTATE
state_machine::GetStateByName(const char* StateName)
{
    // Otherwise, look for a node that matches the name, and just switch to it.
    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        node* Child = GetChild(Idx);
        GStateAssert(Child);

        if (_stricmp(StateName, Child->GetName()) == 0)
            return Child;
    }

    return NULL;
}

bool GSTATE
state_machine::RequestChangeToState(granny_real32 AtT,
                                    granny_real32 DeltaT,
                                    char const* StateName)
{
    node * FoundState = GetStateByName(StateName);
    if( FoundState )
    {
        return RequestChangeToState(AtT, DeltaT, FoundState);
    }
    else
    {
        return false;
    }
}

bool GSTATE
state_machine::RequestChangeToState(granny_real32 AtT, 
                                    granny_real32 DeltaT, 
                                    node * State)
{
    if (m_Active == 0)
        return false;

    transition * CurrentTransition = GSTATE_DYNCAST(m_Active, transition);
    if (CurrentTransition)
    {
        //Don't allow transitioning to the same state
        if( State != CurrentTransition->GetEndNode() && 
            !(GetBoundCharacter() && GetBoundCharacter()->StateMachine->GetAutoForwardTransitionRequests() && CurrentTransition->GetEndNode()->IsNodeAnOnLoopTransitionEndNode(State)))
        {
            if( CurrentTransition->GetEndNode()->GetAllowTransitionExit() || 
                CurrentTransition->GetAllowTransitionExit() )
            {
                for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
                {
                    node* Child = GetChild(Idx);
                    GStateAssert(Child);

                    if (State == Child)
                    {
                        if( Child->GetAllowAnyTransition() )
                        {
                            StartDynamicTransition(AtT, DeltaT, State, -1.0f, Child->GetAnyTransition());
                            return true;
                        }
                    }
                }
            }
        }

        return false;
    }

    GStateAssert(GSTATE_DYNCAST(m_Active, node));
    node* ActiveNode = static_cast<node*>(m_Active);

    // Look for a preferred transition
    if (m_PreferredExit && State == m_PreferredExit->GetEndNode())
    {
        // That works.  It gets a priviledged check ahead of the automatics
        if (m_PreferredExit->ShouldActivate(0, Trigger_Requested, AtT, DeltaT))
        {
            if (StartTransition(AtT, m_PreferredExit))
            {
                return true;
            }
            else
            {
                // fall through below...
            }
        }
    }

    // Look for a transition we can use...
    for (int Pass = 0; Pass < 2; ++Pass)
    {
        for (int Idx = 0; Idx < ActiveNode->GetNumTransitions(); ++Idx)
        {
            transition* Transition = ActiveNode->GetTransition(Idx);
            if (State == Transition->GetEndNode() || (GetBoundCharacter() && GetBoundCharacter()->StateMachine->GetAutoForwardTransitionRequests() && Transition->GetEndNode()->IsNodeAnOnLoopTransitionEndNode(State)))
            {
                if (Transition->ShouldActivate(Pass, Trigger_Requested, AtT, DeltaT))
                {
                    return StartTransition(AtT, Transition);
                }
            }
        }
    }

    // No defined transitions in the data. See if a generic transition is allowed to this state
    GStateAssert(ActiveNode);
    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        node* Child = GetChild(Idx);
        GStateAssert(Child);

        //Don't allow transitioning to the same state
        if (Child->GetAllowAnyTransition() && 
            State == Child && ActiveNode != Child)
        {
            StartAnyTransition(Child->GetAnyTransition(), AtT, DeltaT, State, -1.0f);
            return true;
        }
    }

    return false;
}

bool GSTATE
state_machine::ForceChangeToState(granny_real32 AtT, char const* StateName)
{
    if (m_Active == 0)
        return false;

    if (GSTATE_DYNCAST(m_Active, transition))
        return false;

    GStateAssert(GSTATE_DYNCAST(m_Active, node));
    node* ActiveNode = static_cast<node*>(m_Active);
    ActivateConditionals(AtT);

    // Look for a transition we can use...
    {for (int Idx = 0; Idx < ActiveNode->GetNumTransitions(); ++Idx)
    {
        transition* Transition = ActiveNode->GetTransition(Idx);
        if (_stricmp(StateName, Transition->GetEndNode()->GetName()) == 0)
        {
            // No asking!
            return StartTransition(AtT, Transition);
        }
    }}

    // Otherwise, look for a node that matches the name, and just switch to it.
    {for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        node* Child = GetChild(Idx);
        GStateAssert(Child);

        if (_stricmp(StateName, Child->GetName()) == 0)
            return ForceState(AtT, Child);
    }}

    return false;
}


bool GSTATE
state_machine::StartTransitionByName(granny_real32 AtT,
                                     char const* TransitionName)
{
    if (TransitionName == 0)
        return false;

    node* ActiveNode = GSTATE_DYNCAST(m_Active, node);
    if (ActiveNode == 0)
        return false;

    // Look for a preferred transition
    if (m_PreferredExit &&
        (_stricmp(TransitionName, m_PreferredExit->GetName()) == 0))
    {
        // That works.  It gets a priviledged check ahead of the automatics
        if (m_PreferredExit->ShouldActivate(0, Trigger_Requested, AtT, 0))
        {
            if (StartTransition(AtT, m_PreferredExit))
            {
                return true;
            }
            else
            {
                // fall through below...
            }
        }
    }

    // Look for a transition we can use...
    {for (int Pass = 0; Pass < 2; ++Pass)
    {
        int const NumTransitions = ActiveNode->GetNumTransitions();
        {for (int Idx = 0; Idx < NumTransitions; ++Idx)
        {
            transition* Transition = ActiveNode->GetTransition(Idx);
            if (_stricmp(TransitionName, Transition->GetName()) == 0)
            {
                if (Transition->ShouldActivate(Pass, Trigger_Requested, AtT, 0.0f))
                {
                    return StartTransition(AtT, Transition);
                }
            }
        }}
    }}

    return false;
}

bool GSTATE
state_machine::StartTransition(granny_real32 AtT, transition* Transition)
{
    
    if (m_Active && Transition->GetStartNode() == m_Active)
    {
        // Any transition start clears the preferred exit
        m_PreferredExit = 0;

        SetActive( Transition );
        Transition->Activate(AtT);
        ActivateConditionals(AtT);
        return true;
    }

    return false;
}


bool GSTATE
state_machine::ForceStartTransition(granny_real32 AtT, transition* Transition)
{
    if (m_Active != Transition)
    {
        if( m_Active )
        {
            transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
            if( AsTransition )
            {
                AsTransition->Deactivate();
            }
        }

        // Any transition start clears the preferred exit
        m_PreferredExit = 0;

        SetActive( Transition );
        Transition->Activate(AtT);
        ActivateConditionals(AtT);
    }
    

    return true;
}

bool GSTATE
state_machine::StartDynamicTransition(granny_real32 AtT, granny_real32 DeltaT, const char * StateName, float EarlyExitTransitionDuration, transition * AnyTransitionSource)
{
    node * FoundState = GetStateByName(StateName);
    if( FoundState )
    {
        return StartDynamicTransition(AtT, DeltaT, FoundState, EarlyExitTransitionDuration, AnyTransitionSource);
    }
    else
    {
        return false;
    }
}

bool GSTATE
state_machine::StartDynamicTransition(granny_real32 AtT, granny_real32 DeltaT, node * State, float EarlyExitTransitionDuration, transition * AnyTransitionSource)
{
    granny_real32 AllowedError = 0.0f; //Have to pick some value here, so be conservative
    granny_int32x OutputIdx = 0; //For what we're caching (pose and root motion) there is only one output

    if( m_DynamicTransition )
    {
        for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Dest = GetChild(Idx);
            GStateAssert(Dest);

            if (State == Dest)
            {
                if (m_Active != 0)
                {
                    gstate_character_instance * Character = GetBoundCharacter();

                    granny_local_pose * OldSourcePose = m_DTSourcePose;

                    float Duration = 0.0f;
                    if( EarlyExitTransitionDuration >= 0.0f )
                    {
                        Duration = EarlyExitTransitionDuration;
                    }
                    else
                    {
                        Duration = Dest->GetAnyTransitionDuration();
                    }

                    //Cache pose
                    node* AsNode = GSTATE_DYNCAST(m_Active, node);
                    if (AsNode)
                    {
                        granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsNode, OutputIdx);

                        granny_real32 PoseWeight = 1.0f;
                        m_DTSourcePose = AsNode->SamplePoseOutput(SampleIdx, AtT, AllowedError, Character->PoseCache, &PoseWeight);

                        m_DynamicTransition->InitializeDynamicTransition(AsNode, Dest, false, Duration, AnyTransitionSource);
                    }
                    else
                    {
                        transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
                        granny_real32 PoseWeight = 1.0f;
                        m_DTSourcePose = AsTransition->SamplePose(AtT, AllowedError, Character->PoseCache, &PoseWeight);

                        m_DynamicTransition->InitializeDynamicTransition(AsTransition->GetStartNode(), Dest, true, Duration, AnyTransitionSource);
                    }

                    //Cache motion vectors
                    GetRootMotionVectors(OutputIdx,AtT,Character->DeltaT,m_DTSourceRootTranslation,m_DTSourceRootRotation,false);

                    //Start up the dynamic transition
                    if( !AsNode )
                    {
                        transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
                        AsTransition->Deactivate();
                    }

                    // Any transition start clears the preferred exit
                    m_PreferredExit = 0;

                    SetActive( m_DynamicTransition );

                    m_DynamicTransition->Activate(AtT);

                    if( OldSourcePose != NULL )
                    {
                        if( OldSourcePose != m_DTSourcePose )
                        {
                            gstate_character_instance * BoundCharacter = GetBoundCharacter();
                            GrannyReleaseCachePose(BoundCharacter->PoseCache, OldSourcePose);
                        }
                    }
                }

                return 1;
            }
        }
    }

    return 0;
}

void GSTATE
state_machine::StartAnyTransition(transition * AnyTransition, granny_real32 AtT, granny_real32 DeltaT, const char * StateName, float EarlyExitTransitionDuration)
{
    node * FoundState = GetStateByName(StateName);
    if( FoundState )
    {
        StartAnyTransition(AnyTransition, AtT, DeltaT, FoundState, EarlyExitTransitionDuration);
    }
}

void GSTATE
state_machine::StartAnyTransition(transition * AnyTransition, granny_real32 AtT, granny_real32 DeltaT, node * State, float EarlyExitTransitionDuration)
{
    transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
    if( AsTransition )
    {
        StartDynamicTransition(AtT, DeltaT, State, EarlyExitTransitionDuration, GetAnyTransition());
    }
    else
    {
        node* StartNode = GSTATE_DYNCAST(m_Active, node);
        GStateAssert(StartNode != NULL);

        // Any transition start clears the preferred exit
        m_PreferredExit = 0;

        AnyTransition->Deactivate();
        SetActive(AnyTransition);
        AnyTransition->Activate(AtT);

        AnyTransition->SetAnyTransitionStartNode(StartNode);
    }
}

void GSTATE
state_machine::SetActive( tokenized * Active )
{
    //Exiting the dynamic transition. Release pose cache. 
    if( m_DynamicTransition && (m_Active && m_Active == m_DynamicTransition) && Active != m_DynamicTransition )
    {
        gstate_character_instance * Character = GetBoundCharacter();
        GrannyReleaseCachePose(Character->PoseCache, m_DTSourcePose);

        m_DTSourcePose = NULL;
    }

    m_Active = Active;
}

bool GSTATE
state_machine::ForceState(granny_real32 AtT, node* State)
{
    GStateAssert(IsStateNode(State));

    // If there is currently an active transition, we need to forcibly deactivate it to
    // kill any superfluous resources
    transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
    if (AsTransition)
        AsTransition->Deactivate();

    if (State == 0)
    {
        // should we allow this?
        SetActive( 0 );
        return true;
    }
    
    GStateAssert(State->GetParent() == this);
    GStateAssert(State->GetNumOutputs() > 0);

    // check that it's a pose? technically, we need to pair up our externals with the
    // externals of the states...
    GStateAssert(State->GetOutputType(State->GetNthExternalOutput(0)) == PoseEdge);

    SetActive( State );

    State->Activate(AtT);
    ActivateConditionals(AtT);

    return true;
}

bool GSTATE
state_machine::ForceTransition(granny_real32 AtT, transition* Transition)
{
    // todo: code dupe!

    // If there is currently an active transition, we need to forcibly deactivate it to
    // kill any superfluous resources
    transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
    if (AsTransition)
        AsTransition->Deactivate();

    // Clear the preferred exit
    m_PreferredExit = 0;

    if (Transition == 0)
    {
        // should we allow this?
        SetActive( 0 );
        return true;
    }

    GStateAssert(Transition->GetStartNode() && Transition->GetStartNode()->GetParent() == this);
    GStateAssert(Transition->GetEndNode() && Transition->GetEndNode()->GetParent() == this);

    SetActive( Transition );
    Transition->Activate(AtT);
    ActivateConditionals(AtT);

    return true;
}

void GSTATE
state_machine::SetPreferredExit(transition* PreferredExit)
{
    m_PreferredExit = PreferredExit;
}

transition* GSTATE
state_machine::GetPreferredExit()
{
    return m_PreferredExit;
}

void GSTATE
state_machine::AdvanceT(granny_real32 AtT, granny_real32 DeltaT)
{
    GSTATE_AUTO_ZONE_FN();
    GStateAssert(DeltaT >= 0);

    node* ActiveNode = GSTATE_DYNCAST(m_Active, node);
    transition* Transition = GSTATE_DYNCAST(m_Active, transition);

    //TODO: Hierarchicial 'any transition check' (optional?) Leaf any-trans'es prevent parent any-transes? Not so sure...

    //Check the conditions for 'any transitions'. Do this first, because later code calls ActivateConditionals which 
    //blows everything up. 
    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        node* Child = GetChild(Idx);

        transition * AnyTransition = Child->GetAnyTransition();
        
        if( !AnyTransition || !Child->GetAllowAnyTransition() )
        {
            continue;
        }

        //Don't trigger AnyTransition to the already-active node 
        if( Child == ActiveNode )
        {
            continue;
        }

        //Don't trigger AnyTransition to a node that is the target of an already-running transition (this should maybe be an option). 
        if (Transition && Transition->GetEndNode() == Child )
        {
            continue;
        }

        //AnyTransition is a conditional transition, so most of these parameters are ignored...
        if((!Transition || Transition->GetAllowTransitionExit())&& AnyTransition->ShouldActivate(0,Trigger_Automatic,AtT,DeltaT))
        {
            StartAnyTransition(AnyTransition, AtT, DeltaT, Child->GetName(), AnyTransition->GetDuration());
            //StartDynamicTransition(AtT, DeltaT, Child->GetName(), AnyTransition->GetDuration());

            //Recompute these as they may have changed with StartDynamicTransition()
            ActiveNode = GSTATE_DYNCAST(m_Active, node);
            Transition = GSTATE_DYNCAST(m_Active, transition);
        }
    }

    if (ActiveNode)
    {
        ActiveNode->AdvanceT(AtT, DeltaT);

        // Let's take a look at this nodes transitions, and see if we want to activate any
        // of them.  Note that if we have a preferred transition, then it gets a
        // privileged check ahead of any of the others.
        transition* Activated = 0;

        // Preferred exit
        if (m_PreferredExit != 0)
        {
            if (m_PreferredExit->ShouldActivate(0, Trigger_Automatic, AtT, DeltaT))
            {
                Activated = m_PreferredExit;
                StartTransition(AtT, m_PreferredExit);
            }
        }
        
        for (int Pass = 0; Pass < 2 && Activated == 0; ++Pass)
        {
            for (int Idx = 0; Idx < ActiveNode->GetNumTransitions(); ++Idx)
            {
                transition* Possible = ActiveNode->GetTransition(Idx);
                if (Possible->ShouldActivate(Pass, Trigger_Automatic, AtT, DeltaT))
                {
                    //int External = ActiveNode->GetNthExternalOutput(0);
                    Activated = Possible;
                    SetActive( Activated );
                    Possible->Activate(AtT);
                    ActivateConditionals(AtT);
                    break;
                }
            }
        }
    }
    else if (Transition)
    {
        // Ticks start and end
        Transition->AdvanceT(AtT, DeltaT);

        if (Transition->IsActive() == false)
        {
            SetActive( Transition->GetEndNode() );
            ActivateConditionals(AtT);
            // m_Active->Activate(AtT);  // the end node is actually triggered by the transition itself...
        }
        else
        {
            //Optionally check end node's conditions and allow an early exit if they're met
            node * EndNode = Transition->GetEndNode();

            for (int Idx = 0; Idx < EndNode->GetNumTransitions(); ++Idx)
            {
                transition* EndTransition = EndNode->GetTransition(Idx);

                //Could allow these for onloop transitions too, but that pretty much always seems wrong. 
                if (EndTransition->GetTransitionType() == Transition_OnConditional ||
                    EndTransition->GetTransitionType() == Transition_OnRequest ||
                    EndTransition->GetTransitionType() == Transition_OnLoop ||
                    EndTransition->GetTransitionType() == Transition_OnSubLoop )
                {
                    if( EndNode->GetAllowTransitionExit() || 
                        Transition->GetAllowTransitionExit() )
                    {
                        if(EndTransition->ShouldActivate(0, Trigger_Automatic, AtT, DeltaT) )
                        {
                            StartDynamicTransition(AtT, DeltaT, EndTransition->GetEndNode()->GetName(), EndTransition->GetDuration(),NULL);
                        }
                    }
                }
            }
        }
    }
    else
    {
        // Well, shoot.  There's nothing active.  Try the start state?
        if (m_state_machineToken->StartState != eInvalidChild)
        {
            node* State = GetChild(m_state_machineToken->StartState);
            SetActive( State );
            if (State)
            {
                State->Activate(AtT);
                ActivateConditionals(AtT);
            }
        }
    }

    parent::AdvanceT(AtT, DeltaT);
}

granny_real32 GSTATE
state_machine::SampleScalarOutput(granny_int32x OutputIdx,
                                 granny_real32 AtT)
{
    return SampleScalarOutput(OutputIdx, AtT, NULL);
}

granny_real32 GSTATE
state_machine::SampleScalarOutput(granny_int32x OutputIdx,
                                  granny_real32 AtT, 
                                  gstate_scalar_track_entry * TrackEntry)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    if (IsOutputExternal(OutputIdx))
    {
        if (m_Active != 0)
        {
            node* AsNode = GSTATE_DYNCAST(m_Active, node);
            if (AsNode)
            {
                granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsNode, OutputIdx);
                return AsNode->SampleScalarOutput(SampleIdx, AtT, TrackEntry);
            }
            else
            {
                transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
                granny_int32x SampleIdx = SampleIndexForTransition(this, m_state_machineToken, OutputIdx);
                return AsTransition->SampleScalarOutput(SampleIdx, AtT, TrackEntry);
            }
        }

        // No active element, that's bad.  AdvanceT covers this though, not us...
        return 0;
    }
    else
    {
        // Sample the external scalar
        // Find the input that corresponds to this output.
        node* Node;
        granny_int32x EdgeIdx;
        if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
        {
            // todo: is this a warning?
            return 0;
        }

        return Node->SampleScalarOutput(EdgeIdx, AtT, TrackEntry);
    }
}

bool GSTATE
state_machine::GetScalarOutputRange(granny_int32x OutputIdx,
                                    float*        MinVal,
                                    float*        MaxVal)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    {
        // Sample the external scalar
        // Find the input that corresponds to this output.
        INPUT_CONNECTION(OutputIdx, Scalar);
        if (ScalarNode == 0)
            return false;

        return ScalarNode->GetScalarOutputRange(ScalarEdge, MinVal, MaxVal);
    }
}


bool GSTATE
state_machine::GetMorphChannelBindings(granny_int32x OutputIdx,
                                       char const**  MeshNames,
                                       granny_int32x MeshNameCount)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(GetOutputType(OutputIdx) == MorphEdge);
    GStateCheckPtrNotNULL(MeshNames, return false);
    GStateCheck(MeshNameCount > 0, return false);


    if (IsOutputExternal(OutputIdx))
    {
        if (m_Active != 0)
        {
            node* AsNode = GSTATE_DYNCAST(m_Active, node);
            if (AsNode)
            {
                granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsNode, OutputIdx);
                return AsNode->GetMorphChannelBindings(SampleIdx, MeshNames, MeshNameCount);
            }
            else
            {
                transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
                granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsTransition->GetStartNode(), OutputIdx);
                return AsTransition->GetStartNode()->GetMorphChannelBindings(SampleIdx, MeshNames, MeshNameCount);
            }
        }

        // No active element, that's bad.  AdvanceT covers this though, not us...
        return false;
    }
    else
    {
        // Sample the internal edge
        // Find the input that corresponds to this output.
        INPUT_CONNECTION(OutputIdx, Morph);
        if (!MorphNode)
            return false;

        return MorphNode->GetMorphChannelBindings(MorphEdge, MeshNames, MeshNameCount);
    }
}

granny_int32x GSTATE
state_machine::GetNumMorphChannels(granny_int32x OutputIdx)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(GetOutputType(OutputIdx) == MorphEdge);

    if (IsOutputExternal(OutputIdx))
    {
        if (m_Active != 0)
        {
            node* AsNode = GSTATE_DYNCAST(m_Active, node);
            if (AsNode)
            {
                granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsNode, OutputIdx);
                return AsNode->GetNumMorphChannels(SampleIdx);
            }
            else
            {
                transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
                granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsTransition->GetStartNode(), OutputIdx);
                return AsTransition->GetStartNode()->GetNumMorphChannels(SampleIdx);
            }
        }

        // No active element, that's bad.  AdvanceT covers this though, not us...
        return -1;
    }
    else
    {
        // Sample the internal scalar
        // Find the input that corresponds to this output.
        node* Node;
        granny_int32x EdgeIdx;
        if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
        {
            // todo: is this a warning?
            return false;
        }

        return Node->GetNumMorphChannels(EdgeIdx);
    }
}

bool GSTATE
state_machine::SampleMorphOutput(granny_int32x  OutputIdx,
                                 granny_real32  AtT,
                                 granny_real32* MorphWeights,
                                 granny_int32x  NumMorphWeights)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    if (IsOutputExternal(OutputIdx))
    {
        if (m_Active != 0)
        {
            node* AsNode = GSTATE_DYNCAST(m_Active, node);
            if (AsNode)
            {
                granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsNode, OutputIdx);
                return AsNode->SampleMorphOutput(SampleIdx, AtT, MorphWeights, NumMorphWeights);
            }
            else
            {
                transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
                granny_int32x SampleIdx = SampleIndexForTransition(this, m_state_machineToken, OutputIdx);
                return AsTransition->SampleMorphOutput(SampleIdx, AtT, MorphWeights, NumMorphWeights);
            }
        }

        // No active element, that's bad.  AdvanceT covers this though, not us...
        return false;
    }
    else
    {
        // Sample the internal scalar
        // Find the input that corresponds to this output.
        node* Node;
        granny_int32x EdgeIdx;
        if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
        {
            // todo: is this a warning?
            return false;
        }

        return Node->SampleMorphOutput(EdgeIdx, AtT, MorphWeights, NumMorphWeights);
    }
}

granny_real32 GSTATE
state_machine::GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(IsOutputExternal(OutputIdx));
    GStateAssert(OutputIdx == 0); // will have to handle this if this ever changes...

    if (m_Active != 0)
    {
        node* AsNode = GSTATE_DYNCAST(m_Active, node);
        if (AsNode)
        {
            granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsNode, OutputIdx);
            return AsNode->GetLocalTimeOffset(AtT, SampleIdx);
        }
        else
        {
            //This is horrible, but there's not much else we can do without being able to sample properly at the leaf animation
            return 0; //AsTransition->SamplePose(AtT, AllowedError, PoseCache, PoseWeight);
        }
    }

    // Return null, there's nothing here, so offset doesn't even make sense. 
    return 0;
}

// bool GSTATE state_machine::GetScalarOutputRange()
//    state machine doesn't support range queries, since range queries must be static
//    across state changes.  Theoretically we could query every subnode, but... no.

granny_local_pose* GSTATE
state_machine::SamplePoseOutput(granny_int32x      OutputIdx,
                                granny_real32      AtT,
                                granny_real32      AllowedError,
                                granny_pose_cache* PoseCache,
                                granny_real32*     PoseWeight)
{
    GSTATE_AUTO_ZONE_FN_OR_NAME();

    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(IsOutputExternal(OutputIdx));
    GStateAssert(OutputIdx == 0); // will have to handle this if this ever changes...
    GStateAssert(PoseCache);

    if (m_Active != 0)
    {
        node* AsNode = GSTATE_DYNCAST(m_Active, node);
        if (AsNode)
        {
            granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsNode, OutputIdx);
            return AsNode->SamplePoseOutput(SampleIdx, AtT, AllowedError, PoseCache, PoseWeight);
        }
        else
        {
            transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
            return AsTransition->SamplePose(AtT, AllowedError, PoseCache, PoseWeight);
        }
    }

    // Return null, caller is responsible for building rest pose if necessary
    return 0;
}

bool GSTATE
state_machine::SampleMaskOutput(granny_int32x      OutputIdx,
                                granny_real32      AtT,
                                granny_track_mask* ModelMask)
{
    GS_InvalidCodePath("state machine has no mask output!");
    return 0;
}

bool GSTATE
state_machine::SampleEventOutput(granny_int32x            OutputIdx,
                                 granny_real32            AtT,
                                 granny_real32            DeltaT,
                                 gstate_text_track_entry* EventBuffer,
                                 granny_int32x const      EventBufferSize,
                                 granny_int32x*           NumEvents)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    if (EventBuffer == 0 || EventBufferSize < 0 || NumEvents == 0)
    {
        GS_PreconditionFailed;
        return false;
    }

    //Make the root state machine initialize NumEvents. 
    if( GetParent() == 0 )
    {
        *NumEvents = 0;
    }

    if (IsOutputExternal(OutputIdx))
    {
        if (m_Active != 0)
        {
            node* AsNode = GSTATE_DYNCAST(m_Active, node);
            if (AsNode)
            {
                granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsNode, OutputIdx);
                return AsNode->SampleEventOutput(SampleIdx, AtT, DeltaT,
                                                 EventBuffer, EventBufferSize, NumEvents);
            }
            else
            {
                transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
                granny_int32x SampleIdx = SampleIndexForTransition(this, m_state_machineToken, OutputIdx);
                return AsTransition->SampleEventOutput(SampleIdx, AtT, DeltaT,
                                                       EventBuffer, EventBufferSize, NumEvents);
            }
        }

        return false;
    }
    else
    {
        // Sample the external scalar
        // Find the input that corresponds to this output.
        node* Node;
        granny_int32x EdgeIdx;
        if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
        {
            // todo: is this a warning?
            return 0;
        }

        return Node->SampleEventOutput(EdgeIdx, AtT, DeltaT,
                                       EventBuffer, EventBufferSize, NumEvents);
    }
}

bool GSTATE
state_machine::GetAllEvents(granny_int32x            OutputIdx,
                            gstate_text_track_entry*  EventBuffer,
                            granny_int32x const      EventBufferSize,
                            granny_int32x*           NumEvents)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));

    // This one is a bit tricky, since we have multiple sub-nodes that all export event
    // tracks.  We have to sample them and combine the results into the single output
    // buffer.

    // @@todo need to look at the transitions?

    if (IsOutputExternal(OutputIdx))
    {
        int EventPos = 0;
        {for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Child = GetChild(Idx);

            // Ignore parameter nodes
            if (IsParameterNode(Child))
                continue;

            granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, Child, OutputIdx);

            granny_int32x UsedByChild;
            if (Child->GetAllEvents(SampleIdx,
                                    EventBuffer + EventPos,
                                    EventBufferSize - EventPos,
                                    &UsedByChild) == false)
            {
                return false;
            }

            EventPos = FilterDuplicateEvents(EventBuffer, EventPos, EventPos + UsedByChild);
        }}

        *NumEvents = EventPos;
        return true;
    }
    else
    {
        // Sample the external scalar
        // Find the input that corresponds to this output.
        node* Node;
        granny_int32x EdgeIdx;
        if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
        {
            // todo: is this a warning?
            return false;
        }

        return Node->GetAllEvents(EdgeIdx,
                                  EventBuffer,
                                  EventBufferSize,
                                  NumEvents);
    }
}

bool GSTATE
state_machine::GetCloseEventTimes(granny_int32x  OutputIdx,
                                  granny_real32  AtT,
                                  char const*    TextToFind,
                                  granny_real32* PreviousTime,
                                  granny_real32* NextTime)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    if (IsOutputExternal(OutputIdx))
    {
        if (m_Active != 0)
        {
            node* AsNode = GSTATE_DYNCAST(m_Active, node);
            if (AsNode)
            {
                granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsNode, OutputIdx);
                return AsNode->GetCloseEventTimes(SampleIdx, AtT, TextToFind, PreviousTime, NextTime);
            }
            else
            {
                transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
                granny_int32x SampleIdx = SampleIndexForTransition(this, m_state_machineToken, OutputIdx);
                return AsTransition->GetCloseEventTimes(SampleIdx, AtT, TextToFind, PreviousTime, NextTime);
            }
        }

        // No active element, that's kinda bad.  AdvanceT covers this though, not us...
        return false;
    }
    else
    {
        // Sample the external scalar
        // Find the input that corresponds to this output.
        node* Node;
        granny_int32x EdgeIdx;
        if (!ConnectedInput(OutputIdx, &Node, &EdgeIdx))
        {
            // todo: is this a warning?
            return false;
        }

        return Node->GetCloseEventTimes(EdgeIdx, AtT, TextToFind, PreviousTime, NextTime);
    }
}


bool GSTATE
state_machine::GetRootMotionVectors(granny_int32x  OutputIdx,
                                    granny_real32  AtT,
                                    granny_real32  DeltaT,
                                    granny_real32* Translation,
                                    granny_real32* Rotation,
                                    bool Inverse)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(OutputIdx == 0); // will have to handle this if this ever changes...

    if (m_Active != 0)
    {
        node* AsNode = GSTATE_DYNCAST(m_Active, node);
        if (AsNode)
        {
            granny_int32x SampleIdx = SampleIndexForNode(this, m_state_machineToken, AsNode, OutputIdx);
            return AsNode->GetRootMotionVectors(SampleIdx, AtT, DeltaT, Translation, Rotation, Inverse);
        }
        else
        {
            transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
            GStateAssert(SampleIndexForTransition(this, m_state_machineToken, OutputIdx) == 0);
            return AsTransition->GetRootMotionVectors(AtT, DeltaT, Translation, Rotation, Inverse);
        }
    }

    // Return null, caller is resposible for building rest pose if necessary
    return false;
}

void GSTATE
state_machine::ActivateConditionals(granny_real32 AtT)
{
    {for (int Idx = 0; Idx < m_state_machineToken->ConditionalCount; ++Idx)
    {
        m_Conditionals[Idx]->Activate(AtT);
    }}
}


void GSTATE
state_machine::Activate(granny_real32 AtT)
{
    parent::Activate(AtT);

    if (m_state_machineToken->UseLastActive)
    {
        if (m_Active)
        {
            node* StartNode = 0;

            transition* AsTransition = GSTATE_DYNCAST(m_Active, transition);
            if (AsTransition)
            {
                // Use the end state of the last active transition
                StartNode = AsTransition->GetEndNode();
            }
            else
            {
                StartNode = GSTATE_DYNCAST(m_Active, node);
            }

            if (StartNode)
            {
                ForceState(AtT, StartNode);
                return;
            }
        }
    }
    
    if( m_StartStateOverride )
    {
        ForceState(AtT, m_StartStateOverride);
        m_StartStateOverride = NULL;
    }
    else if (m_state_machineToken->RandomStartState )
    {
        // Double check for valid state
        int NumStateNodes = 0;
        for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Node = GetChild(Idx);
            if (IsStateNode(Node))
                NumStateNodes++;
        }

        if( NumStateNodes > 0 )
        {
            //APTODO: Hide start state if random is on
            //APTODO: Switch to enum/option thing instead of having both random start state and use last state on activation
            //APTODO: Visual feedback for start state type (both for use last state and random start state, also for forced start state and any other types)

            //Try to avoid repeating the last random selection. 
            static int s_Tries = 10;
            int RandomStateNode = -1;
            int LastRandomStartStatesSize = sizeof(LastRandomStartStates)/sizeof(LastRandomStartStates[0]);
            for( int itry=0; itry<s_Tries; itry++ )
            {

                float Unit = RandomUnit();
                RandomStateNode = (int)Clamp(0, Unit * NumStateNodes, (granny_real32)NumStateNodes);
                bool RandomStartStateSeenRecently = false;
                for( int lrsi=0; lrsi<LastRandomStartStatesSize; lrsi++ )
                {
                    if(LastRandomStartStates[lrsi] == RandomStateNode)
                    {
                        RandomStartStateSeenRecently = true;
                        break;
                    }
                }

                if( !RandomStartStateSeenRecently )
                {
                    break;
                }
            }
            GStateAssert(RandomStateNode >= 0 && RandomStateNode < NumStateNodes);
            GStateAssert( LastRandomStartStatesIndex < LastRandomStartStatesSize);
            LastRandomStartStates[LastRandomStartStatesIndex] = RandomStateNode;
            LastRandomStartStatesIndex = (LastRandomStartStatesIndex+1) % LastRandomStartStatesSize;

            int FoundStateNode = 0;
            for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
            {
                node* Node = GetChild(Idx);
                if (IsStateNode(Node))
                {
                    if( FoundStateNode == RandomStateNode )
                    {
                        ForceState(AtT, Node);
                        break;
                    }
                    else
                    {
                        FoundStateNode++;
                    }
                }
            }
        }
    }
    else if (m_state_machineToken->StartState != eInvalidChild)
    {
        node* State = GetChild(m_state_machineToken->StartState);
        ForceState(AtT, State);
    }
    else
    {
        // Double check for valid state
        for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Node = GetChild(Idx);
            if (IsStateNode(Node))
            {
                GStateWarning("state machine '%s': No start state, but states present\n", GetName());
                ForceState(AtT, Node);
                return;
            }
        }
    }

    for( int ChildIdx=0; ChildIdx<GetNumChildren(); ChildIdx++ )
    {
        parameters*   AsParam = GSTATE_DYNCAST(GetChild(ChildIdx), parameters);
        if( AsParam )
        {
            AsParam->Activate(AtT);
        }
    }
}

bool GSTATE
state_machine::DidSubLoopOccur(node*         SubNode,
                               granny_int32  OnOutput,
                               granny_real32 AtT,
                               granny_real32 DeltaT)
{
    GStateAssert(SubNode != 0);

    // Only check if the node is active
    if (IsNodeActive(SubNode))
    {
        return SubNode->DidLoopOccur(OnOutput, AtT, DeltaT);
    }

    return false;
}

bool GSTATE
state_machine::DidLoopOccur(granny_int32x OnOutput,
                   granny_real32 AtT,
                   granny_real32 DeltaT)
{
    node* AsNode = GSTATE_DYNCAST(m_Active, node);
    if( AsNode != NULL )
    {
        return AsNode->DidLoopOccur(OnOutput, AtT, DeltaT);
    }
    return false;
}


// =============================================================================
// Published parameters (corresponds to edges other than 0)
// =============================================================================
granny_int32x GSTATE
state_machine::AddOutput(node_edge_type EdgeType, char const* EdgeName)
{
    GStateAssert(CheckConnections());

    TakeTokenOwnership();

    if (GetNumOutputs() != 0 && (EdgeType != ScalarEdge && EdgeType != EventEdge && EdgeType != MorphEdge))
    {
        return -1;
    }

    //Only the root state machine holds MorphMeshNames. 
    if( GetParent() == NULL )
    {
        char *& NewMeshNameEntry = QVecPushNewElement(m_state_machineToken->MorphMeshNameCount, m_state_machineToken->MorphMeshNames);

        if( EdgeType == MorphEdge )
        {
            GStateCloneString(NewMeshNameEntry,"");
        }
        else
        {
            NewMeshNameEntry = NULL;
        }
    }

    // Pass to the parent, we'll work with our children after this.
    // NOTE WELL! Most other leaf nodes pass to AddOutputImpl, this passes to AddOutput
    // because it needs to go through the container::AddOutput for correct edge
    // maintainence.
    granny_int32x NewEdgeIdx = parent::AddOutput(EdgeType, EdgeName);

    // We now need to add this output to all of our children except for parameter nodes.
    // Note that this can cascade down to sub-statemachines...
    AddOutputToChildren(NewEdgeIdx);

    GStateAssert(CheckConnections());
    return NewEdgeIdx;
}

bool GSTATE
state_machine::DeleteOutput(granny_int32x OutputIndex)
{
    GStateAssert(CheckConnections());

    TakeTokenOwnership();

    RemoveOutputFromChildren(OutputIndex);
    RemoveOutputFromConditionals(this, OutputIndex);

    if( GetParent() == NULL )
    {
        GStateAssert(OutputIndex >= 0 && OutputIndex < m_state_machineToken->MorphMeshNameCount);
        if(OutputIndex >= 0 && OutputIndex < m_state_machineToken->MorphMeshNameCount)
        {
            GStateDeallocate(m_state_machineToken->MorphMeshNames[OutputIndex]);
            QVecRemoveElement(OutputIndex, m_state_machineToken->MorphMeshNameCount, m_state_machineToken->MorphMeshNames);
        }
    }

    // NOTE WELL! Most other leaf nodes pass to DeleteOutputImpl, this passes to DeleteOutput
    // because it needs to go through the container::DeleteOutput for correct edge
    // maintenance.
    bool RetVal = parent::DeleteOutput(OutputIndex);

    GStateAssert(CheckConnections());
    return RetVal;
}

bool GSTATE
state_machine::SetOutputName(granny_int32x OutputIdx, char const* NewEdgeName)
{
    if (parent::SetOutputName(OutputIdx, NewEdgeName) == false)
        return false;

    // We now need to add this output to all of our children except for parameter nodes.
    // Note that this can cascade down to sub-statemachines...
    RefreshChildOutputName(OutputIdx);
    return true;
}


granny_int32x GSTATE
state_machine::AddInput(node_edge_type EdgeType, char const* EdgeName)
{
    GStateAssert(CheckConnections());

    // NOTE WELL: See above notes...
    granny_int32x RetVal = parent::AddInput(EdgeType, EdgeName);
    CheckConnections();

    AdjustChildInputs();

    GStateAssert(GetInputType(RetVal) == EdgeType);
    GStateAssert(strcmp(GetInputName(RetVal), EdgeName) == 0);

    GStateAssert(CheckConnections());
    return RetVal;
}

bool GSTATE
state_machine::DeleteInput(granny_int32x InputIndex)
{
    // Huge trouble if this fails...
    // NOTE WELL: See above notes...
    bool RetVal = parent::DeleteInput(InputIndex);
    AdjustChildInputs();
    return RetVal;
}


void GSTATE
state_machine::AddInputForNodeChild(node* Node, int OutputIdx, node * Child)
{
    // We only care about complicated children...
    if ((GSTATE_DYNCAST(Child, blend_graph) == 0) &&
        (GSTATE_DYNCAST(Child, state_machine) == 0))
    {
        return;
    }

    bool Found = false;
    int const NumChildInputs = Child->GetNumInputs();
    for (int ChildInputIdx = 0; ChildInputIdx < NumChildInputs && !Found; ++ChildInputIdx)
    {
        if (Child->IsInputExternal(ChildInputIdx) == false)
            continue;

        INPUT_CONNECTION_ON(Child, ChildInputIdx, Test);
        if (TestNode == Node && TestEdge == OutputIdx)
        {
            GStateAssert(Node->GetOutputType(OutputIdx) == Child->GetInputType(ChildInputIdx));
            Found = true;
        }
    }

    if (Found == false)
    {
        int NewInputIdx = Child->AddInput(Node->GetOutputType(OutputIdx),
            Node->GetOutputName(OutputIdx));
        Child->SetInputConnection(NewInputIdx, Node, OutputIdx);
    }
}

void GSTATE
state_machine::AddInputsForNode(node* Node)
{
    GStateAssert(Node == this || IsParameterNode(Node));

    int const NumChildren = GetNumChildren();
    int const NumOutputs  = Node->GetNumOutputs();

    for (int OutputIdx = 0; OutputIdx < NumOutputs; ++OutputIdx)
    {
        // We wire up our internal outputs, but parameter nodes wire their external outputs.
        if ((Node == this && Node->IsOutputExternal(OutputIdx)) ||
            (Node != this && Node->IsOutputInternal(OutputIdx)))
        {
            continue;
        }

        for (int ChildIdx = 0; ChildIdx < NumChildren; ++ChildIdx)
        {
            AddInputForNodeChild(Node, OutputIdx, GetChild(ChildIdx));
        }
    }
}

void GSTATE
state_machine::AdjustChildInputs()
{
    // First, loop through all children.  Look for external inputs on blend_graphs and
    // state_machines that are not connected to us or one of our children anymore.  Delete
    // those.
    int const NumChildren = GetNumChildren();
    for (int Idx = 0; Idx < NumChildren; ++Idx)
    {
        node* Child = GetChild(Idx);

        // We only care about complicated children...
        if ((GSTATE_DYNCAST(Child, blend_graph) == 0) &&
            (GSTATE_DYNCAST(Child, state_machine) == 0))
        {
            continue;
        }

        for (int InputIdx = Child->GetNumInputs() - 1; InputIdx >= 0; --InputIdx)
        {
            if (Child->IsInputExternal(InputIdx) == false)
                continue;

            INPUT_CONNECTION_ON(Child, InputIdx, Test);
            if (TestNode == 0)
            {
                Child->DeleteInput(InputIdx);
            }
            else
            {
                GStateAssert(TestNode->GetOutputType(TestEdge) == Child->GetInputType(InputIdx));
                GStateAssert(TestNode == this ||
                             (IsParameterNode(TestNode) && TestNode->GetParent() == this));
            }
        }
    }

    // Now.  Loop through all of our internal outputs.  Make sure each complicated child
    // has an input connected to these.  Then loop for all parameter nodes.  Similarly for
    // those.
    AddInputsForNode(this);
    for (int Idx = 0; Idx < NumChildren; ++Idx)
    {
        node* Child = GetChild(Idx);
        if (IsParameterNode(Child))
            AddInputsForNode(Child);
    }
}


// =============================================================================
// Conditionals for transitions, etc.
// =============================================================================
granny_int32x GSTATE
state_machine::GetNumConditionals()
{
    return m_state_machineToken->ConditionalCount;
}


conditional* GSTATE
state_machine::GetConditional(granny_int32x Index)
{
    GStateCheckIndex(Index, m_state_machineToken->ConditionalCount, return 0);

    return m_Conditionals[Index];
}

granny_int32x GSTATE
state_machine::AddConditional(conditional* NewCondition)
{
    GStateAssert(NewCondition->GetOwner() == 0);
    GStateAssert(NewCondition->GetTokenContext() == GetTokenContext());

    if (NewCondition == 0)
    {
        GS_InvalidCodePath("adding null condition?");
        return -1;
    }

    // Just make sure that this isn't already in the conditional array...
    {for (int Idx = 0; Idx < m_state_machineToken->ConditionalCount; ++Idx)
    {
        if (NewCondition == m_Conditionals[Idx])
        {
            GS_InvalidCodePath("adding existing conditional?");
            return Idx;
        }
    }}

    // Ok, add it.
    TakeTokenOwnership();

    // Do the m_NewConditionNodes first so we can use the token count, which isn't updated
    conditional*& NewEntry   = QVecPushNewElementNoCount(m_state_machineToken->ConditionalCount, m_Conditionals);
    granny_variant& NewToken = QVecPushNewElement(m_state_machineToken->ConditionalCount,
                                                  m_state_machineToken->Conditionals);
    // Set the new entries
    if (!NewCondition->GetTypeAndToken(&NewToken))
    {
        GS_InvalidCodePath("oooh, that's bad, couldn't get child token");
        return -1;
    }

    // Set the parent field
    NewCondition->SetOwner(this);

    // And our cache of created conditionals
    NewEntry = NewCondition;

    // We added it at the back...
    return (m_state_machineToken->ConditionalCount - 1);
}

bool GSTATE
state_machine::ReplaceConditional(granny_int32x ConditionalIndex, conditional* NewCondition)
{
    GStateCheckIndex(ConditionalIndex, GetNumConditionals(), return false);
    GStateCheckPtrNotNULL(NewCondition, return false);
    GStateCheck(NewCondition->GetOwner() == 0, return false);
    GStateAssert(NewCondition->GetTokenContext() == GetTokenContext());

    // Do this in one operation to avoid disturbing the indices...
    TakeTokenOwnership();

    granny_variant NewToken;
    if (NewCondition->GetTypeAndToken(&NewToken) == false)
    {
        GS_InvalidCodePath("Unable to get type/token, really bad.");
        return false;
    }

    m_state_machineToken->Conditionals[ConditionalIndex] = NewToken;

    // Remove the old entry...
    conditional* OldConditional = m_Conditionals[ConditionalIndex];
    OldConditional->SetOwner(0);

    // We need to flush out the conditional from any transitions that refer to it...
    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        node* Child = GetChild(Idx);

        for (int TransitionIdx = 0; TransitionIdx < Child->GetNumTransitions(); ++TransitionIdx)
        {
            transition* Transition = Child->GetTransition(TransitionIdx);

            for (int CondIdx = 0; CondIdx < Transition->GetNumConditionals(); ++CondIdx)
            {
                if (Transition->GetConditional(CondIdx) == OldConditional)
                    Transition->SetConditional(CondIdx, NewCondition);
            }
        }

        //Flush the conditional from the Any Transition
        {
            transition * Transition = Child->GetAnyTransition();
            if( Transition )
            {
                for (int CondIdx = 0; CondIdx < Transition->GetNumConditionals(); ++CondIdx)
                {
                    if (Transition->GetConditional(CondIdx) == OldConditional)
                        Transition->SetConditional(CondIdx, NewCondition);
                }
            }
        }
    }

    // Now we can nuke it
    GStateDelete<conditional>(OldConditional);

    // And in with the new...
    NewCondition->SetOwner(this);
    m_Conditionals[ConditionalIndex] = NewCondition;

    return true;
}

void GSTATE
state_machine::NoteContainerDeleted()
{
    node * RootNode = this;
    while(true)
    {
        node * ParentNode = RootNode->GetParent();
        if( ParentNode )
        {
            RootNode = ParentNode;
        }
        else
        {
            break;
        }
    }
    state_machine * RootNodeAsStateMachine = GSTATE_DYNCAST(RootNode, state_machine);

    for (int Idx = 0; Idx < GetNumConditionals(); ++ Idx)
    {
        RootNodeAsStateMachine->Note_ConditionalDelete(this,Idx);
    }

    parent::NoteContainerDeleted();
}

void GSTATE
state_machine::Note_ConditionalDelete(state_machine* Owner, int ConditionalIndex)
{
    
    if (Owner == this)
    {
        conditional* Remove = Owner->GetConditional(ConditionalIndex);

        // Remove the conditional from any transitions in our children
        for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
        {
            node* Child = GetChild(Idx);

            for (int TransitionIdx = 0; TransitionIdx < Child->GetNumTransitions(); ++TransitionIdx)
            {
                transition* Transition = Child->GetTransition(TransitionIdx);
                Transition->Note_ConditionalDelete(Remove);
            }

            transition * Transition = Child->GetAnyTransition();
            if( Transition )
            {
                Transition->Note_ConditionalDelete(Remove);
            }
        }
    }

    // Remove the conditional from any parent_condition references
    for (int Idx = 0; Idx < GetNumConditionals(); ++Idx)
    {
        conditional*      Cond       = GetConditional(Idx);
        parent_condition* ParentCond = GSTATE_SLOW_TYPE_CHECK(Cond, parent_condition);
        if (ParentCond)
        {
            state_machine* Ref = 0;
            int            RefIdx = -1;
            ParentCond->GetExternalCondition(&Ref, &RefIdx);
            if (Ref == Owner)
            {
                if (RefIdx == ConditionalIndex)
                    ParentCond->SetExternalCondition(0, -1);
                else if (RefIdx > ConditionalIndex)
                    ParentCond->SetExternalCondition(Owner, RefIdx-1);
            }
        }
    }

    // Pass it up to the parent so we can propogate it downwards...
    parent::Note_ConditionalDelete(Owner, ConditionalIndex);
}

void GSTATE
state_machine::DeleteConditional(granny_int32x ConditionalIndex)
{
    GStateAssert(GS_InRange(ConditionalIndex, GetNumConditionals()));

    TakeTokenOwnership();

    conditional* Remove = m_Conditionals[ConditionalIndex];

    node * RootNode = this;
    while(true)
    {
        node * ParentNode = RootNode->GetParent();
        if( ParentNode )
        {
            RootNode = ParentNode;
        }
        else
        {
            break;
        }
    }
    state_machine * RootNodeAsStateMachine = GSTATE_DYNCAST(RootNode, state_machine);
    RootNodeAsStateMachine->Note_ConditionalDelete(this, ConditionalIndex);


    QVecRemoveElementNoCount(ConditionalIndex, m_state_machineToken->ConditionalCount, m_Conditionals);
    QVecRemoveElement(ConditionalIndex,
                      m_state_machineToken->ConditionalCount,
                      m_state_machineToken->Conditionals);

    GStateDelete<conditional>(Remove);
}


CREATE_SNAPSHOT(state_machine)
{
    // m_Active can be:
    //  - NULL (no children)
    //  - a node
    //  - a transition
    granny_int32 ActiveNonNull = m_Active != 0 ? 1 : 0;
    CREATE_WRITE_INT32(ActiveNonNull);
    if (ActiveNonNull)
    {
        granny_uint32 UID = m_Active->GetUID();
        CREATE_WRITE_INT32(UID);
    }

    granny_int32 PreferredNonNull = m_PreferredExit != 0 ? 1 : 0;
    CREATE_WRITE_INT32(PreferredNonNull);
    if (PreferredNonNull)
    {
        granny_uint32 UID = m_PreferredExit->GetUID();
        CREATE_WRITE_INT32(UID);
    }
    
    CREATE_PASS_TO_PARENT();
}

RESET_FROMSNAPSHOT(state_machine)
{
    RESET_OFFSET_TRACKING();

    granny_int32 ActiveNonNull;
    RESET_READ_INT32(ActiveNonNull);
    if (ActiveNonNull)
    {
        granny_int32 UID;
        RESET_READ_INT32(UID);

        SetActive( GetTokenContext()->GetProductForUID(UID) );
    }
    else
    {
        SetActive( 0 );
    }

    granny_int32 PreferredNonNull;
    RESET_READ_INT32(PreferredNonNull);
    if (PreferredNonNull)
    {
        granny_int32 UID;
        RESET_READ_INT32(UID);

        tokenized* Exit = GetTokenContext()->GetProductForUID(UID);
        m_PreferredExit = GSTATE_DYNCAST(Exit, transition);
        GStateAssert(Exit);
        GStateAssert(m_PreferredExit);
    }
    else
    {
        m_PreferredExit = 0;
    }

    RESET_PASS_TO_PARENT();
}


