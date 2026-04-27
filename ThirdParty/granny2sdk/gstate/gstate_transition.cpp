// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_transition.h"

#include "gstate_anim_utils.h"
#include "gstate_conditional.h"
#include "gstate_container.h"
#include "gstate_node.h"
#include "gstate_quick_vecs.h"
#include "gstate_snapshotutils.h"
#include "gstate_state_machine.h"
#include "gstate_blend_graph.h"
#include "gstate_telemetry.h"
#include "gstate_token_context.h"

#include "gstate_transition_lastresort.h"
#include "gstate_transition_onconditional.h"
#include "gstate_transition_onloop.h"
#include "gstate_transition_onrequest.h"
#include "gstate_transition_onsubloop.h"


#include <stdlib.h>
#include <string.h>
#include <limits>
#include <float.h>


#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;


// Sync type definition
static granny_data_type_definition SynchronizeSpecType[] =
{
    { GrannyVariantReferenceMember, "ReferenceNode" },
    { GrannyVariantReferenceMember, "SyncedNode"    },

    { GrannyInt16Member,  "ReferenceEventEdge" },
    { GrannyInt16Member,  "SyncedEventEdge"    },
    { GrannyStringMember, "EventName" },

    { GrannyEndMember }
};



char const* GSTATE
NameForTransitionType(transition_type Type)
{
    switch (Type)
    {
    case Transition_LastResort:    return "Last Resort (On loop)";
    case Transition_OnRequest:     return "On Request";
    case Transition_OnLoop:        return "On Loop";
    case Transition_OnSubLoop:     return "On Subloop";
    case Transition_OnConditional: return "On Conditional";
    default:
        GS_InvalidCodePath("Unknown transition type");
        return "Unknown";
    }
}

char const* GSTATE
NameForBlendType(transition_blend_type Type)
{
    switch (Type)
    {
    case Transition_BlendLinear:    return "Linear";
    case Transition_BlendSmooth:     return "Smooth";
    default:
        GS_InvalidCodePath("Unknown transition blend type");
        return "Unknown";
    }
}

char const* GSTATE
NameForEventHandlingScheme(transition_event_handling_scheme Scheme)
{
    switch (Scheme)
    {
        case Event_SwitchAtMidpoint: return "Switch at Midpoint";
        case Event_SampleBoth:       return "Sample Both";
        case Event_StartOnly:        return "Start Only";
        case Event_EndOnly:          return "End Only";
        case Event_Neither:          return "Neither";
        default:
            GS_InvalidCodePath("Unknown handling scheme");
            return "Unknown";
    }
}



bool GSTATE
TransitionTypeValidForNodes(transition_type Type, node* StartNode, node* EndNode)
{
    if (Type == Transition_Dynamic )
    {
        //Prevent dynamic transition type from showing up in the editor
        return false;
    }
    else if (Type == Transition_LastResort) //APTODO: Should probably allow lastrestort for these too!!!
    {
        // State machines don't support these...
        return GSTATE_DYNCAST(StartNode, state_machine) == 0;
    }
    else if (Type == Transition_OnSubLoop)
    {
        return GSTATE_DYNCAST(StartNode, container) != 0;
    }

    return true;
}

transition* GSTATE
TransferTransitionToType(transition_type Type, transition* OldTransition)
{
    tokenized* NewTokenized = 0;
    switch (Type)
    {
        case Transition_OnRequest:     NewTokenized = tr_onrequest::DefaultInstance();     break;
        case Transition_OnLoop:        NewTokenized = tr_onloop::DefaultInstance();        break;
        case Transition_OnSubLoop:     NewTokenized = tr_onsubloop::DefaultInstance();     break;
        case Transition_LastResort:    NewTokenized = tr_lastresort::DefaultInstance();    break;
        case Transition_OnConditional: NewTokenized = tr_onconditional::DefaultInstance(); break;

        default:
            GS_InvalidCodePath("Invalid transition type");
            return 0;
    }
    transition* NewTransition = GSTATE_DYNCAST(NewTokenized, transition);

    granny_int32x StartX, StartY, EndX, EndY;
    OldTransition->GetDrawingHints(StartX, StartY, EndX, EndY);

    NewTransition->SetName(OldTransition->GetName());
    NewTransition->SetNodes(OldTransition->GetStartNode(), StartX, StartY,
                            OldTransition->GetEndNode(), EndX, EndY,
                            OldTransition->GetDestinationStartNode());
    NewTransition->SetPreferredExit(OldTransition->GetPreferredExit());

    NewTransition->SetBlendType(OldTransition->GetBlendType());

    // Conditionals
    NewTransition->SetAndConditions(OldTransition->GetAndConditions());
    {for (int Idx = 0; Idx < OldTransition->GetNumConditionals(); ++Idx)
    {
        granny_int32x NewIdx = NewTransition->AddConditional();
        GStateAssert(NewIdx == Idx);
        GStateUnused(NewIdx);

        NewTransition->SetConditional(Idx, OldTransition->GetConditional(Idx));
    }}

    // Duration
    NewTransition->SetDuration(OldTransition->GetDuration());

    // Sync list
    {for (int Idx = 0; Idx < OldTransition->GetNumSyncs(); ++Idx)
    {
        granny_int32x NewIdx = NewTransition->AddSync();
        GStateAssert(NewIdx == Idx);
        GStateUnused(NewIdx);

        node* FromIgnored;
        node* ToIgnored;
        synchronize_spec Spec;
        OldTransition->GetSyncParameters(Idx, &Spec, &FromIgnored, &ToIgnored);
        NewTransition->SetSyncParameters(Idx, Spec);
    }}

    return NewTransition;
}


struct GSTATE transition::transitionImpl
{
    char*          Name;

    granny_variant StartNode;     // type/token
    granny_real32  StartX;
    granny_real32  StartY;

    granny_variant EndNode;       // type/token
    granny_real32  EndX;
    granny_real32  EndY;

    granny_real32  Duration;

    granny_bool32  AndConditions;

    granny_int32    ConditionalCount;
    granny_variant* Conditionals;

    granny_int32    InvertConditionalCount;
    granny_uint8*   InvertConditionals;

    granny_int32      SyncCount;
    synchronize_spec* Syncs;

    granny_variant    PreferredExit;

    granny_int32      EventHandlingScheme;

    char* UserKeyValues;

    granny_bool32     AllowTransitionExit;

    granny_variant DestinationStartNode;     // Overridden start node in the desination state machine

    granny_bool32 IsAnyTransition;

    granny_int32      BlendType;
};

granny_data_type_definition GSTATE
transition::transitionImplType[] =
{
    { GrannyStringMember,           "Name" },
    { GrannyVariantReferenceMember, "StartNode" },
    { GrannyReal32Member,           "StartX" },
    { GrannyReal32Member,           "StartY" },
    { GrannyVariantReferenceMember, "EndNode" },
    { GrannyReal32Member,           "EndX" },
    { GrannyReal32Member,           "EndY" },
    { GrannyReal32Member,           "Duration" },
    { GrannyBool32Member,           "AndConditions" },
    { GrannyReferenceToArrayMember, "Conditionals", GrannyVariantType },
    { GrannyReferenceToArrayMember, "InvertConditionals", GrannyUInt8Type },
    { GrannyReferenceToArrayMember, "Syncs",        SynchronizeSpecType },
    { GrannyVariantReferenceMember, "PreferredExit" },
    { GrannyInt32Member, "EventHandlingScheme" },
    { GrannyStringMember, "UserKeyValues" },
    { GrannyBool32Member, "AllowTransitionExit" },
    { GrannyVariantReferenceMember, "DestinationStartNode" },
    { GrannyBool32Member, "IsAnyTransition" },
    { GrannyInt32Member, "BlendType" },

    { GrannyEndMember },
};



void GSTATE
transition::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(transition);

    GStateCloneString(m_transitionToken->Name, OldToken->Name);

    GStateCloneArray(m_transitionToken->Conditionals,
                     OldToken->Conditionals,
                     m_transitionToken->ConditionalCount);

    // Temp fix for older tokens missing InvertConditionals
    if (OldToken->ConditionalCount != 0 && OldToken->InvertConditionalCount == 0)
    {
        m_transitionToken->InvertConditionalCount = OldToken->ConditionalCount;
        GStateAllocZeroedArray(m_transitionToken->InvertConditionals, m_transitionToken->InvertConditionalCount);
    }
    else
    {
        GStateCloneArray(m_transitionToken->InvertConditionals,
                         OldToken->InvertConditionals,
                         m_transitionToken->InvertConditionalCount);
    }
    GStateAssert(m_transitionToken->ConditionalCount == m_transitionToken->InvertConditionalCount);
    
    GStateCloneArray(m_transitionToken->Syncs,
                     OldToken->Syncs,
                     m_transitionToken->SyncCount);
    {for (int Idx = 0; Idx < m_transitionToken->SyncCount; ++Idx)
    {
        GStateCloneString(m_transitionToken->Syncs[Idx].EventName,
                          OldToken->Syncs[Idx].EventName);
    }}
}


void GSTATE
transition::ReleaseOwnedToken_transition()
{
    GStateDeallocate(m_transitionToken->Name);
    GStateDeallocate(m_transitionToken->Conditionals);
    GStateDeallocate(m_transitionToken->InvertConditionals);

    {for (int Idx = 0; Idx < m_transitionToken->SyncCount; ++Idx)
    {
        GStateDeallocate(m_transitionToken->Syncs[Idx].EventName);
    }}
    GStateDeallocate(m_transitionToken->Syncs);
}


GSTATE
transition::transition(token_context*               Context,
                       granny_data_type_definition* TokenType,
                       void*                        TokenObject,
                       token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_transitionToken(0),
    m_StartNode(0), m_EndNode(0), m_DestinationStartNode(0),
    m_Conditionals(0),
    m_PreferredExit(0),
    m_ReferenceNodes(0),
    m_SyncedNodes(0),
    m_Active(false),
    m_ActiveTimer(0)
{
    IMPL_INIT_FROM_TOKEN(transition);

    if (m_transitionToken->ConditionalCount)
        GStateAllocZeroedArray(m_Conditionals, m_transitionToken->ConditionalCount);

    if (m_transitionToken->SyncCount)
    {
        GStateAllocZeroedArray(m_ReferenceNodes, m_transitionToken->SyncCount);
        GStateAllocZeroedArray(m_SyncedNodes,    m_transitionToken->SyncCount);
    }
}


GSTATE
transition::~transition()
{
    // We don't own the conditionals...
    GStateDeallocate(m_Conditionals);
    GStateDeallocate(m_ReferenceNodes);
    GStateDeallocate(m_SyncedNodes);

    // We don't own our preferred exit either
    m_PreferredExit = 0;

    DTOR_RELEASE_TOKEN(transition);
}


CREATE_SNAPSHOT(transition)
{
    CREATE_WRITE_REAL32(m_Active);
    CREATE_WRITE_REAL32(m_ActiveTimer);
    CREATE_PASS_TO_PARENT();
}

RESET_FROMSNAPSHOT(transition)
{
    RESET_OFFSET_TRACKING();
    RESET_READ_REAL32(m_Active);
    RESET_READ_REAL32(m_ActiveTimer);
    RESET_PASS_TO_PARENT();
}

transition_event_handling_scheme GSTATE
transition::GetEventHandlingScheme() const
{
    return (transition_event_handling_scheme)m_transitionToken->EventHandlingScheme;
}

void GSTATE
transition::SetEventHandlingScheme(transition_event_handling_scheme NewScheme)
{
    TakeTokenOwnership();

    RuntimeOverrideEventHandlingScheme(NewScheme);
}

transition_blend_type GSTATE
transition::GetBlendType() const
{
    return (transition_blend_type)m_transitionToken->BlendType;
}

void GSTATE
transition::SetBlendType(transition_blend_type NewBlendType)
{
    TakeTokenOwnership();

    m_transitionToken->BlendType = NewBlendType;
}

void GSTATE
transition::RuntimeOverrideEventHandlingScheme(transition_event_handling_scheme NewScheme)
{
    GStateAssert(NewScheme >= Event_SwitchAtMidpoint && NewScheme <= Event_Neither);
    m_transitionToken->EventHandlingScheme = NewScheme;
}

granny_real32 GSTATE
transition::GetDuration()
{
    return m_transitionToken->Duration;
}

void GSTATE
transition::SetDuration(granny_real32 Duration)
{
    GStateAssert(Duration >= 0.0f);

    TakeTokenOwnership();
    m_transitionToken->Duration = Duration;
}


char const* GSTATE
transition::GetName() const
{
    if (m_transitionToken->Name == 0)
        return "<unnamed>";

    return m_transitionToken->Name;
}

node* GSTATE
transition::GetDestinationStartNode()
{
    return m_DestinationStartNode;
}

void GSTATE
transition::SetDestinationStartNode(node * DestStartNode)
{
    TakeTokenOwnership();

    m_DestinationStartNode = DestStartNode;

    if( DestStartNode == NULL )
    {
        m_transitionToken->DestinationStartNode.Type = 0;
        m_transitionToken->DestinationStartNode.Object = 0;
    }
    else
    {
        if( !DestStartNode->GetTypeAndToken(&m_transitionToken->DestinationStartNode) )
        {
            GStateWarning("Failed to update destination start node. Start must have been removed");
            m_transitionToken->DestinationStartNode.Type = 0;
            m_transitionToken->DestinationStartNode.Object = 0;
            m_DestinationStartNode = NULL;
        }
    }
}

bool GSTATE
transition::SetName(char const* NewName)
{
    TakeTokenOwnership();

    if (NewName == 0)
    {
        return false;
    }

    // replace with copy of NewName
    GStateReplaceString(m_transitionToken->Name, NewName);

    return true;
}

void GSTATE
transition::SetIsAnyTransition(bool Any)
{
    TakeTokenOwnership();

    m_transitionToken->IsAnyTransition = Any;
}

bool GSTATE
transition::GetIsAnyTransition()
{
    return m_transitionToken->IsAnyTransition ? true : false;
}


void GSTATE
transition::SetKeyValueString(char const* KeyValues)
{
    TakeTokenOwnership();

    // Todo: validate input string
    GStateReplaceString(m_transitionToken->UserKeyValues, KeyValues);
}

char const* GSTATE
transition::GetKeyValueString()
{
    return m_transitionToken->UserKeyValues;
}

void GSTATE
transition::SetNodes(node* StartNode, granny_int32x StartX, granny_int32x StartY,
                     node* EndNode,   granny_int32x EndX,   granny_int32x EndY,
                     node* DestinationStartNode)
{
    GStateAssert(GetIsAnyTransition() || StartNode && EndNode);
    GStateAssert(GetIsAnyTransition() || StartNode != EndNode);

    TakeTokenOwnership();

    if (StartNode->GetTypeAndToken(&m_transitionToken->StartNode) == false ||
        EndNode->GetTypeAndToken(&m_transitionToken->EndNode) == false)
    {
        GStateError("Unable to get type and token for node in Transition::SetNodes.");
        GS_InvalidCodePath("Unable to get type and token for node");
        return;
    }

    SetDestinationStartNode(DestinationStartNode);

    m_transitionToken->StartX = granny_real32(StartX);
    m_transitionToken->StartY = granny_real32(StartY);
    m_transitionToken->EndX   = granny_real32(EndX);
    m_transitionToken->EndY   = granny_real32(EndY);

    // We need to clear out the sync specs in this case, since it's not really feasible to
    // rematch them automatically (Think ReplaceNodeWith in anim studio...)
    while (GetNumSyncs())
    {
        DeleteSync(0);
    }

    m_StartNode = StartNode;
    m_EndNode   = EndNode;
    m_DestinationStartNode = DestinationStartNode;
    m_Active    = false;
}

void GSTATE
transition::SetStartNode(node* StartNode)
{
    GStateAssert(GetIsAnyTransition() || StartNode != m_EndNode);

    TakeTokenOwnership();

    if (StartNode->GetTypeAndToken(&m_transitionToken->StartNode) == false)
    {
        GStateError("Unable to get type and token for start node in Transition::SetEndNode.");
        GS_InvalidCodePath("Unable to get type and token for node");
        return;
    }

    // We need to clear out the sync specs in this case, since it's not really feasible to
    // rematch them automatically (Think ReplaceNodeWith in anim studio...)
    while (GetNumSyncs())
    {
        DeleteSync(0);
    }
    m_StartNode  = StartNode;
    m_Active    = false;
}

void GSTATE
transition::SetEndNode(node* EndNode, node* DestinationStartNode)
{
    GStateAssert(GetIsAnyTransition() || m_StartNode != EndNode);

    TakeTokenOwnership();

    if (EndNode->GetTypeAndToken(&m_transitionToken->EndNode) == false)
    {
        GStateError("Unable to get type and token for end node in Transition::SetEndNode.");
        GS_InvalidCodePath("Unable to get type and token for node");
        return;
    }

    // Set the destination start node. 
    SetDestinationStartNode(DestinationStartNode);

    // We need to clear out the sync specs in this case, since it's not really feasible to
    // rematch them automatically (Think ReplaceNodeWith in anim studio...)
    while (GetNumSyncs())
    {
        DeleteSync(0);
    }
    m_EndNode   = EndNode;
    m_Active    = false;
}

void GSTATE
transition::SetAnyTransitionStartNode(node* StartNode)
{
    if( IsEditable() )
    {
        TakeTokenOwnership();

        while (GetNumSyncs())
        {
            DeleteSync(0);
        }

        granny_int32x StartX;
        granny_int32x StartY;
        StartNode->GetPosition(StartX, StartY);
        m_transitionToken->StartX = granny_real32(StartX);
        m_transitionToken->StartY = granny_real32(StartY);

        granny_int32x EndX;
        granny_int32x EndY;
        GetEndNode()->GetPosition(EndX, EndY);
        m_transitionToken->EndX = granny_real32(EndX);
        m_transitionToken->EndY = granny_real32(EndY);
    }

    m_StartNode = StartNode;
}

bool GSTATE
transition::SetPreferredExit(transition* PreferredExit)
{
    TakeTokenOwnership();

    if (PreferredExit == 0)
    {
        m_transitionToken->PreferredExit.Type   = 0;
        m_transitionToken->PreferredExit.Object = 0;

        m_PreferredExit = 0;
        return true;
    }
    else
    {
        if (PreferredExit->GetStartNode() != GetEndNode())
        {
            GStateError("Preferred exit must start at the transition we end on\n");
            return false;
        }

        granny_variant TypeAndToken;
        if (PreferredExit->GetTypeAndToken(&TypeAndToken) == false)
        {
            GS_InvalidCodePath("missing token for transition");
            return false;
        }

        m_transitionToken->PreferredExit = TypeAndToken;
        m_PreferredExit = PreferredExit;

        return true;
    }
}

transition* GSTATE
transition::GetPreferredExit()
{
    return m_PreferredExit;
}

node* GSTATE
transition::GetStartNode()
{

    return m_StartNode;
}

node* GSTATE
transition::GetEndNode()
{
    return m_EndNode;
}

// =============================================================================
// ========== Conditional Interface
// =============================================================================
granny_int32x GSTATE
transition::AddConditional()
{
    TakeTokenOwnership();

    // Do the m_ChildNodes first so we can use the token count, which isn't updated
    conditional*& NewTransition = QVecPushNewElementNoCount(m_transitionToken->ConditionalCount, m_Conditionals);

    GStateAssert(m_transitionToken->InvertConditionalCount == m_transitionToken->ConditionalCount);
    granny_uint8&   NewInvert   = QVecPushNewElement(m_transitionToken->InvertConditionalCount, m_transitionToken->InvertConditionals);
    granny_variant& NewToken    = QVecPushNewElement(m_transitionToken->ConditionalCount, m_transitionToken->Conditionals);

    NewTransition = 0;
    NewToken.Object = 0;
    NewToken.Type   = 0;

    NewInvert = 0;

    return m_transitionToken->ConditionalCount - 1;
}

void GSTATE
transition::DeleteConditional(granny_int32x Index)
{
    GStateAssert(GS_InRange(Index, m_transitionToken->ConditionalCount));

    TakeTokenOwnership();

    // Do the m_ChildNodes first so we can use the token count, which isn't updated
    QVecRemoveElementNoCount(Index, m_transitionToken->ConditionalCount, m_Conditionals);

    GStateAssert(m_transitionToken->InvertConditionalCount == m_transitionToken->ConditionalCount);
    QVecRemoveElement(Index, m_transitionToken->ConditionalCount, m_transitionToken->Conditionals);
    QVecRemoveElement(Index, m_transitionToken->InvertConditionalCount, m_transitionToken->InvertConditionals);
}

granny_int32x GSTATE
transition::GetNumConditionals()
{
    return m_transitionToken->ConditionalCount;
}

conditional* GSTATE
transition::GetConditional(granny_int32x Index)
{
    return m_Conditionals[Index];
}

void GSTATE
transition::SetConditional(granny_int32x Index, conditional* Conditional)
{
    GStateAssert(GS_InRange(Index, m_transitionToken->ConditionalCount));

    TakeTokenOwnership();

    if (Conditional)
    {
        if (Conditional->GetTypeAndToken(&m_transitionToken->Conditionals[Index]) == false)
        {
            GS_InvalidCodePath("Unable to get type and token for conditional");
            return;
        }
    }
    else
    {
        m_transitionToken->Conditionals[Index].Type = 0;
        m_transitionToken->Conditionals[Index].Object = 0;
    }

    m_Conditionals[Index] = Conditional;
}

void GSTATE
transition::SetInvertConditional(granny_int32x Index, bool Inverted)
{
    TakeTokenOwnership();
    GStateAssert(GS_InRange(Index, m_transitionToken->InvertConditionalCount));

    m_transitionToken->InvertConditionals[Index] = Inverted ? 1 : 0;
}

bool GSTATE
transition::GetInvertConditional(granny_int32x Index)
{
    // This can be out of range in one case, when InvertConditionalCount is 0, and ConditionalCount is not.  (Old token)
    if (Index >= m_transitionToken->InvertConditionalCount)
    {
        GStateAssert(GS_InRange(Index, m_transitionToken->ConditionalCount));
        GStateAssert(m_transitionToken->InvertConditionalCount == 0);

        return false;
    }
    else
    {
        GStateAssert(GS_InRange(Index, m_transitionToken->InvertConditionalCount));
        return m_transitionToken->InvertConditionals[Index] ? true : false;
    }
}


bool GSTATE
transition::GetAndConditions()
{
    return m_transitionToken->AndConditions != 0;
}

void GSTATE
transition::SetAndConditions(bool And)
{
    TakeTokenOwnership();

    m_transitionToken->AndConditions = And;
}

bool GSTATE
transition::CheckConditionals(granny_real32 AtT)
{
    if (!m_Conditionals)
        return true;

    {for (int Idx = 0; Idx < m_transitionToken->ConditionalCount; ++Idx)
    {
        bool Truth = m_Conditionals[Idx] ? m_Conditionals[Idx]->IsTrue(AtT) : false;
        if (GetInvertConditional(Idx))
            Truth = !Truth;

        if (Truth)
        {
            // In an "or" conditional, we're done
            if (!m_transitionToken->AndConditions)
                return true;
        }
        else
        {
            // In an "and" transition, this means we're done
            if (m_transitionToken->AndConditions)
                return false;
        }
    }}

    // If we got here, then if:
    //  and condition: true
    //  or  condition: false
    return (m_transitionToken->AndConditions != 0);
}

bool GSTATE
transition::GetAllowTransitionExit() const
{
    return (m_transitionToken->AllowTransitionExit != 0);
}

void GSTATE
transition::SetAllowTransitionExit(bool Allow)
{
    TakeTokenOwnership();

    m_transitionToken->AllowTransitionExit = (Allow ? 1 : 0);
}


// =============================================================================
// ========== Sync interface
// =============================================================================
granny_int32x GSTATE
transition::GetNumSyncs()
{
    return m_transitionToken->SyncCount;
}

granny_int32x GSTATE
transition::AddSync()
{
    TakeTokenOwnership();

    // Do the m_ChildNodes first so we can use the token count, which isn't updated
    node*& RefNode            = QVecPushNewElementNoCount(m_transitionToken->SyncCount, m_ReferenceNodes);
    node*& SyncedNode         = QVecPushNewElementNoCount(m_transitionToken->SyncCount, m_SyncedNodes);
    synchronize_spec& NewSpec = QVecPushNewElement(m_transitionToken->SyncCount, m_transitionToken->Syncs);

    RefNode    = 0;
    SyncedNode = 0;
    memset(&NewSpec, 0, sizeof(NewSpec));

    return m_transitionToken->SyncCount - 1;
}

void GSTATE
transition::DeleteSync(granny_int32x Index)
{
    GStateAssert(GS_InRange(Index, GetNumSyncs()));

    TakeTokenOwnership();

    // Do the m_ChildNodes first so we can use the token count, which isn't updated
    QVecRemoveElementNoCount(Index, m_transitionToken->SyncCount, m_ReferenceNodes);
    QVecRemoveElementNoCount(Index, m_transitionToken->SyncCount, m_SyncedNodes);
    QVecRemoveElement(Index, m_transitionToken->SyncCount, m_transitionToken->Syncs);
}

void GSTATE
transition::GetSyncParameters(granny_int32x     Index,
                              synchronize_spec* OutParam,
                              node**            ReferenceNode,
                              node**            SyncedNode)
{
    GStateAssert(GS_InRange(Index, GetNumSyncs()));

    *OutParam = m_transitionToken->Syncs[Index];
    *ReferenceNode = m_ReferenceNodes[Index];
    *SyncedNode    = m_SyncedNodes[Index];
}

void GSTATE
transition::SetSyncParameters(granny_int32x          Index,
                              synchronize_spec const& NewParam)
{
    GStateAssert(GS_InRange(Index, GetNumSyncs()));

    TakeTokenOwnership();

    // Free the old string, if any
    GStateDeallocate(m_transitionToken->Syncs[Index].EventName);

    m_transitionToken->Syncs[Index] = NewParam;
    GStateCloneString(m_transitionToken->Syncs[Index].EventName,
                      NewParam.EventName);

    // Since this happens at edit-time, it's OK to just run through this again...
    CaptureNodeLinks();
}


void GSTATE
transition::ExecuteSyncPlan(granny_real32 SyncT)
{
    {for (int Idx = 0; Idx < m_transitionToken->SyncCount; ++Idx)
    {
        synchronize_spec& Spec = m_transitionToken->Syncs[Idx];

        // First, look to see if we have valid nodes for this.
        node* RefNode    = m_ReferenceNodes[Idx];
        node* SyncedNode = m_SyncedNodes[Idx];
        if (RefNode == 0 || SyncedNode == 0)
            continue;

        granny_real32 RefBegin, RefEnd;
        granny_real32 SyncBegin, SyncEnd;
        granny_int32x SyncOutput = RefNode->GetNthExternalOutput(0);

        if (Spec.EventName != 0)
        {
            GStateAssert(RefNode->GetOutputType(Spec.ReferenceEventEdge) == EventEdge);
            GStateAssert(SyncedNode->GetOutputType(Spec.SyncedEventEdge) == EventEdge);

            // We're going to sync based on the event range.  If that fails, warning?
            // todo: warning?
            if (RefNode->GetCloseEventTimes(Spec.ReferenceEventEdge,
                                            SyncT,
                                            Spec.EventName,
                                            &RefBegin, &RefEnd) == false ||
                SyncedNode->GetCloseEventTimes(Spec.SyncedEventEdge,
                                               SyncT,
                                               Spec.EventName,
                                               &SyncBegin, &SyncEnd) == false)
            {
                continue;
            }
        }
        else
        {
            // Cache!
            granny_int32x RefOutput  = RefNode->GetNthExternalOutput(0);

            // Sync based on phase on output 0

            if (RefNode->GetPhaseBoundaries(RefOutput, SyncT, &RefBegin, &RefEnd) == false ||
                SyncedNode->GetPhaseBoundaries(SyncOutput, SyncT, &SyncBegin, &SyncEnd) == false)
            {
                continue;
            }
        }

        // Everything checks out, sync!
        SyncedNode->Synchronize(SyncOutput, SyncT,
                                RefBegin, RefEnd,
                                SyncBegin, SyncEnd);
    }}
}



void GSTATE
transition::GetDrawingHints(granny_int32x& StartX,
                            granny_int32x& StartY,
                            granny_int32x& EndX,
                            granny_int32x& EndY) const
{

    StartX = granny_int32x(m_transitionToken->StartX);
    StartY = granny_int32x(m_transitionToken->StartY);
    EndX   = granny_int32x(m_transitionToken->EndX);
    EndY   = granny_int32x(m_transitionToken->EndY);
}

void GSTATE
transition::SetDrawingHints(granny_int32x StartX, granny_int32x StartY,
                     granny_int32x EndX, granny_int32x EndY)
{
    TakeTokenOwnership();

    m_transitionToken->StartX = (granny_real32)StartX;
    m_transitionToken->StartY = (granny_real32)StartY;
    m_transitionToken->EndX = (granny_real32)EndX;
    m_transitionToken->EndY = (granny_real32)EndY;

}

bool GSTATE
transition::FillDefaultToken(granny_data_type_definition* TokenType,
                                 void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares transitionImpl*& Slot = // member
    GET_TOKEN_SLOT(transition);

    // Our slot in this token should be empty.
    // Create a new transition token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->Duration = 0.25f;

    return true;
}


bool GSTATE
transition::CaptureNodeLinks()
{
    // These should never be null in a transition that is loading from disk.
    GStateAssert(m_transitionToken->StartNode.Type && m_transitionToken->StartNode.Object);
    GStateAssert(m_transitionToken->EndNode.Type && m_transitionToken->EndNode.Object);

    tokenized* Start = GetTokenContext()->GetProductForToken(m_transitionToken->StartNode.Object);
    tokenized* End   = GetTokenContext()->GetProductForToken(m_transitionToken->EndNode.Object);
    tokenized* DestStart   = GetTokenContext()->GetProductForToken(m_transitionToken->DestinationStartNode.Object);
    // This can happen if the node is not created due to obsolescence.
    if (Start == 0 || End == 0)
        return false;

    m_StartNode = GSTATE_DYNCAST(Start, node);
    m_EndNode   = GSTATE_DYNCAST(End,   node);
    GStateAssert(m_StartNode && m_EndNode);

    //Ok for this to go away, though a notification might be nice...
    m_DestinationStartNode = GSTATE_DYNCAST(DestStart,   node);

    // Capture the preferred exit, if present
    if (m_transitionToken->PreferredExit.Object)
    {
        tokenized* Exit = GetTokenContext()->GetProductForToken(m_transitionToken->PreferredExit.Object);
        m_PreferredExit = GSTATE_DYNCAST(Exit, transition);
        GStateAssert(m_PreferredExit);
    }
    else
    {
        m_PreferredExit = 0;
    }

    // Grab the conditionals
    for (int Idx = 0; Idx < m_transitionToken->ConditionalCount; ++Idx)
    {
        if (m_transitionToken->Conditionals[Idx].Object != 0)
        {
            tokenized* Conditional = GetTokenContext()->GetProductForToken(m_transitionToken->Conditionals[Idx].Object);
            m_Conditionals[Idx] = GSTATE_DYNCAST(Conditional, conditional);
            GStateAssert(m_Conditionals[Idx]);
        }
        else
        {
            m_Conditionals[Idx] = 0;
        }
    }

    for (int Idx = 0; Idx < m_transitionToken->SyncCount; ++Idx)
    {
        if (m_transitionToken->Syncs[Idx].ReferenceNode.Object != 0)
        {
            tokenized* Node = GetTokenContext()->GetProductForToken(m_transitionToken->Syncs[Idx].ReferenceNode.Object);
            m_ReferenceNodes[Idx] = GSTATE_DYNCAST(Node, node);
            GStateAssert(m_ReferenceNodes[Idx]);
        }
        else
        {
            m_ReferenceNodes[Idx] = 0;
        }

        if (m_transitionToken->Syncs[Idx].SyncedNode.Object != 0)
        {
            tokenized* Node = GetTokenContext()->GetProductForToken(m_transitionToken->Syncs[Idx].SyncedNode.Object);
            m_SyncedNodes[Idx] = GSTATE_DYNCAST(Node, node);
            GStateAssert(m_SyncedNodes[Idx]);
        }
        else
        {
            m_SyncedNodes[Idx] = 0;
        }
    }

    return true;
}


granny_real32 GSTATE
transition::ComputeDestFactor()
{
    GStateAssert(IsActive());

    granny_real32 Dest = 1.0f;
    if (m_transitionToken->Duration > 0)
    {
        Dest = 1.0f - (m_ActiveTimer / m_transitionToken->Duration);
        if( GetBlendType() == Transition_BlendSmooth )
        {
            Dest = HermiteCurve(Dest);
        }
        Dest = Clamp(0, Dest, 1);
    }

    return Dest;
}

granny_real32 GSTATE
transition::GetTimeRunning()
{
    GStateAssert(IsActive());

    granny_real32 Progress = Clamp(0.0f, m_transitionToken->Duration - m_ActiveTimer, FLT_MAX);
    return Progress;
}


granny_local_pose* GSTATE
transition::SamplePose(granny_real32 AtT,
                       granny_real32 AllowedError,
                       granny_pose_cache* PoseCache,
                       granny_real32 * PoseWeight)
{
    GSTATE_AUTO_ZONE_FN();
    GStateAssert(IsActive());

    float const Dest = ComputeDestFactor();

    granny_int32x StartExt = m_StartNode->GetNthExternalOutput(0);
    granny_int32x EndExt   = m_EndNode->GetNthExternalOutput(0);

    if (Dest == 0)
    {
        GSTATE_AUTO_ZONE(StartNode);
        granny_real32 SampledPoseWeight = 1.0f;
        return m_StartNode->SamplePoseOutput(StartExt, AtT, AllowedError, PoseCache, &SampledPoseWeight);
    }
    else if (Dest == 1)
    {
        GSTATE_AUTO_ZONE(EndNode);
        granny_real32 SampledPoseWeight = 1.0f;
        return m_EndNode->SamplePoseOutput(EndExt, AtT, AllowedError, PoseCache, &SampledPoseWeight);
    }
    else
    {
        // Have to do the work of blending these together
        granny_real32 StartPoseWeight = 1.0f;
        granny_local_pose* StartPose = 0;
        {
            GSTATE_AUTO_ZONE(StartNode);
            StartPose = m_StartNode->SamplePoseOutput(StartExt, AtT, AllowedError, PoseCache, &StartPoseWeight);
        }

        granny_real32 EndPoseWeight = 1.0f;
        granny_local_pose* EndPose = 0;
        {
            GSTATE_AUTO_ZONE(EndNode);
            EndPose = m_EndNode->SamplePoseOutput(EndExt, AtT, AllowedError, PoseCache, &EndPoseWeight);
        }

        if (StartPose && EndPose)
        {
            GrannyLinearBlend(StartPose, StartPose, EndPose, Dest);

            // Release endpose before we return
            GrannyReleaseCachePose(PoseCache, EndPose);

            *PoseWeight = Clamp(0.0f,1.0f - Dest,1.0f) * StartPoseWeight + Dest * EndPoseWeight;
            return StartPose;
        }
        else if (StartPose)
        {
            *PoseWeight = StartPoseWeight * Clamp(0.0f,1.0f - Dest,1.0f);
            return StartPose;
        }
        else if (EndPose)
        {
            
            *PoseWeight = EndPoseWeight * Dest;
            return EndPose;
        }
        else
        {
            // Well, shoot. rest pose, I guess
            return 0;
        }
    }
}

bool GSTATE
transition::SampleMorphOutput(granny_int32x  OutputIdx,
                              granny_real32  AtT,
                              granny_real32* MorphWeights,
                              granny_int32x  NumMorphWeights)
{
    GStateAssert(IsActive());

    granny_int32x StartExt = m_StartNode->GetNthExternalOutput(OutputIdx);
    granny_int32x EndExt   = m_EndNode->GetNthExternalOutput(OutputIdx);
    GStateAssert(m_StartNode->GetOutputType(StartExt) == MorphEdge);
    GStateAssert(m_EndNode->GetOutputType(EndExt) == MorphEdge);

    granny_int32x StartChannels = m_StartNode->GetNumMorphChannels(StartExt);
    granny_int32x EndChannels   = m_EndNode->GetNumMorphChannels(EndExt);
    if (StartChannels != EndChannels || StartChannels > NumMorphWeights)
        return false;

    float const Dest = ComputeDestFactor();
    if (Dest == 0)
    {
        return m_StartNode->SampleMorphOutput(StartExt, AtT, MorphWeights, NumMorphWeights);
    }
    else if (Dest == 1)
    {
        return m_EndNode->SampleMorphOutput(EndExt, AtT, MorphWeights, NumMorphWeights);
    }
    else
    {
        // Try to use the stack when possible
        granny_real32  Buffer[16] = {0};
        granny_real32* BufferPtr = &Buffer[0];
        granny_real32* DynBuffer = 0;
        if (StartChannels > 16)
        {
            GStateAllocZeroedArray(DynBuffer, StartChannels);
            if (!DynBuffer)
                return false;

            BufferPtr = DynBuffer;
        }

        if (!m_StartNode->SampleMorphOutput(StartExt, AtT, BufferPtr, StartChannels) ||
            !m_EndNode->SampleMorphOutput(EndExt, AtT, MorphWeights, EndChannels))
        {
            return false;
        }

        // Blend them.
        for (int Idx = 0; Idx < EndChannels; ++Idx)
            MorphWeights[Idx] = MorphWeights[Idx] * Dest + (BufferPtr[Idx] * (1 - Dest));

        GStateDeallocate(DynBuffer);
        return true;
    }
}


bool GSTATE
transition::GetRootMotionVectors(granny_real32  AtT,
                                 granny_real32  DeltaT,
                                 granny_real32* ResultTranslation,
                                 granny_real32* ResultRotation,
                                 bool           Inverse)
{
    GStateAssert(IsActive());
    GStateAssert(m_StartNode);
    GStateAssert(m_EndNode);

    // For now, this is always just a lerp
    float Dest = 1.0f;
    if (m_transitionToken->Duration > 0)
    {
        Dest = 1.0f - (m_ActiveTimer / m_transitionToken->Duration);
        if (Dest < 0)
            Dest = 0;
        else if (Dest > 1)
            Dest = 1;
    }

    granny_int32x StartExt = m_StartNode->GetNthExternalOutput(0);
    granny_int32x EndExt   = m_EndNode->GetNthExternalOutput(0);

    if (Dest == 0)
    {
        return m_StartNode->GetRootMotionVectors(StartExt, AtT, DeltaT, ResultTranslation, ResultRotation, Inverse);
    }
    else if (Dest == 1)
    {
        return m_EndNode->GetRootMotionVectors(EndExt, AtT, DeltaT, ResultTranslation, ResultRotation, Inverse);
    }
    else
    {
        granny_real32 StartRot[3];
        granny_real32 StartTrans[3];

        granny_real32 EndRot[3];
        granny_real32 EndTrans[3];

        bool StartValid = m_StartNode->GetRootMotionVectors(StartExt, AtT, DeltaT, StartTrans, StartRot, Inverse);
        bool EndValid   = m_EndNode->GetRootMotionVectors(EndExt, AtT, DeltaT, EndTrans, EndRot, Inverse);

        if (StartValid && EndValid)
        {
            {for (int Idx = 0; Idx < 3; ++Idx)
            {
                ResultTranslation[Idx] = StartTrans[Idx] * (1 - Dest) + EndTrans[Idx] * Dest;
                ResultRotation[Idx]    = StartRot[Idx]   * (1 - Dest) + EndRot[Idx]   * Dest;
            }}
            return true;
        }
        else if (StartValid)
        {
            memcpy(ResultTranslation, StartTrans, sizeof(granny_real32)*3);
            memcpy(ResultRotation,    StartRot,   sizeof(granny_real32)*3);
            return true;
        }
        else if (EndValid)
        {
            memcpy(ResultTranslation, EndTrans, sizeof(granny_real32)*3);
            memcpy(ResultRotation,    EndRot,   sizeof(granny_real32)*3);
            return true;
        }
        else
        {
            // Well, shoot. Nothing, I guess
            return false;
        }
    }
}

granny_real32 GSTATE
transition::SampleScalarOutput(granny_int32x OutputIdx,
                               granny_real32 AtT, 
                               gstate_scalar_track_entry * TrackEntry)
{
    GStateAssert(IsActive());

    granny_int32x StartExt = m_StartNode->GetNthExternalOutput(OutputIdx);
    granny_int32x EndExt   = m_EndNode->GetNthExternalOutput(OutputIdx);
    GStateAssert(m_StartNode->GetOutputType(StartExt) == ScalarEdge);
    GStateAssert(m_EndNode->GetOutputType(EndExt) == ScalarEdge);

    float const Dest = ComputeDestFactor();
    if (Dest == 0)
    {
        return m_StartNode->SampleScalarOutput(StartExt, AtT, TrackEntry);
    }
    else if (Dest == 1)
    {
        return m_EndNode->SampleScalarOutput(EndExt, AtT, TrackEntry);
    }
    else
    {
        //Return track entry values for the start until the transition is half over, then 
        //return track entry values for the end
        gstate_scalar_track_entry * StartTrackEntry = NULL;
        gstate_scalar_track_entry * EndTrackEntry = NULL;
        if( Dest < 0.5f )
        {
            StartTrackEntry = TrackEntry;
        }
        else
        {
            EndTrackEntry = TrackEntry;
        }
        granny_real32 StartScalar = m_StartNode->SampleScalarOutput(StartExt, AtT, StartTrackEntry);
        granny_real32 EndScalar   = m_EndNode->SampleScalarOutput(EndExt, AtT, EndTrackEntry);

        return StartScalar * (1 - Dest) + EndScalar * Dest;
    }
}

bool GSTATE
transition::SampleEventOutput(granny_int32x            OutputIdx,
                              granny_real32            AtT,
                              granny_real32            DeltaT,
                              gstate_text_track_entry* EventBuffer,
                              granny_int32x const      EventBufferSize,
                              granny_int32x*           NumEvents)
{
    GStateAssert(IsActive());

    node*         SampleNode0 = 0;
    granny_int32x SampleEdge0 = -1;
    node*         SampleNode1 = 0;
    granny_int32x SampleEdge1 = -1;

    float const Dest = ComputeDestFactor();

    //Also sample both if it's a zero-duration Event_SwitchAtMidpoint transition, since otherwise there's no 'switch' in SwitchAtMidpoint
    if( (m_transitionToken->EventHandlingScheme == Event_SwitchAtMidpoint && m_transitionToken->Duration == 0.0f) || 
        m_transitionToken->EventHandlingScheme == Event_SampleBoth )
    {
        {
            granny_int32x StartExt = m_StartNode->GetNthExternalOutput(OutputIdx);
            GStateAssert(m_StartNode->GetOutputType(StartExt) == EventEdge);
            SampleNode0 = m_StartNode;
            SampleEdge0 = StartExt;
        }
        {
            granny_int32x EndExt   = m_EndNode->GetNthExternalOutput(OutputIdx);
            GStateAssert(m_EndNode->GetOutputType(EndExt) == EventEdge);
            SampleNode1 = m_EndNode;
            SampleEdge1 = EndExt;
        }
    }
    else
    {
        // Default is switch to mid
        switch (m_transitionToken->EventHandlingScheme)
        {
        case Event_StartOnly:
            {
                granny_int32x StartExt = m_StartNode->GetNthExternalOutput(OutputIdx);
                GStateAssert(m_StartNode->GetOutputType(StartExt) == EventEdge);
                SampleNode0 = m_StartNode;
                SampleEdge0 = StartExt;
            } break;

        case Event_EndOnly:
            {
                granny_int32x EndExt   = m_EndNode->GetNthExternalOutput(OutputIdx);
                GStateAssert(m_EndNode->GetOutputType(EndExt) == EventEdge);
                SampleNode0 = m_EndNode;
                SampleEdge0 = EndExt;
            } break;

        case Event_SwitchAtMidpoint:
        default:
            {
                if (Dest <= 0.5f)
                {
                    granny_int32x StartExt = m_StartNode->GetNthExternalOutput(OutputIdx);
                    GStateAssert(m_StartNode->GetOutputType(StartExt) == EventEdge);
                    SampleNode0 = m_StartNode;
                    SampleEdge0 = StartExt;
                }
                else
                {
                    granny_int32x EndExt   = m_EndNode->GetNthExternalOutput(OutputIdx);
                    GStateAssert(m_EndNode->GetOutputType(EndExt) == EventEdge);
                    SampleNode0 = m_EndNode;
                    SampleEdge0 = EndExt;
                }
            } break;        
        }
    }    

    bool Success = false;
    if (SampleNode0)
    {
        Success = SampleNode0->SampleEventOutput(SampleEdge0, AtT, DeltaT, EventBuffer, EventBufferSize, NumEvents);

        if( m_transitionToken->EventHandlingScheme == Event_SampleBoth )
        {
            for( int EventIdx=0; EventIdx < *NumEvents; EventIdx++ )
            {
                EventBuffer[EventIdx].Weight = Clamp(0.0f, EventBuffer[EventIdx].Weight * (1.0f - Dest), 1.0f);
            }
        }

        if (Success && SampleNode1)
        {
            granny_int32x TotalEvents = 0;
            Success = SampleNode1->SampleEventOutput(SampleEdge1, AtT, DeltaT,
                                                     EventBuffer + *NumEvents,
                                                     EventBufferSize - *NumEvents,
                                                     &TotalEvents);
            *NumEvents += TotalEvents;

            if( m_transitionToken->EventHandlingScheme == Event_SampleBoth )
            {
                for( int EventIdx=*NumEvents; EventIdx < TotalEvents; EventIdx++ )
                {
                    EventBuffer[EventIdx].Weight = Clamp(0.0f, EventBuffer[EventIdx].Weight * (Dest), 1.0f);
                }
            }
        }
    }

    return Success;
}

bool GSTATE
transition::GetCloseEventTimes(granny_int32x  OutputIdx,
                               granny_real32  AtT,
                               char const*    TextToFind,
                               granny_real32* PreviousTime,
                               granny_real32* NextTime)
{
    GStateAssert(IsActive());

    float const Dest = ComputeDestFactor();
    if (Dest <= 0.5f)
    {
        granny_int32x StartExt = m_StartNode->GetNthExternalOutput(OutputIdx);
        GStateAssert(m_StartNode->GetOutputType(StartExt) == EventEdge);
        return m_StartNode->GetCloseEventTimes(StartExt, AtT, TextToFind, PreviousTime, NextTime);
    }
    else
    {
        granny_int32x EndExt   = m_EndNode->GetNthExternalOutput(OutputIdx);
        GStateAssert(m_EndNode->GetOutputType(EndExt) == EventEdge);
        return m_EndNode->GetCloseEventTimes(EndExt, AtT, TextToFind, PreviousTime, NextTime);
    }
}


void GSTATE
transition::Activate(granny_real32 AtT)
{
    // todo: note att?
    GStateAssert(!IsActive());

    m_Active = true;
    m_ActiveTimer = m_transitionToken->Duration;
    GStateAssert(m_ActiveTimer >= 0);

    // Notify the state machine about the preferred exit, or clear it if we have none
    state_machine* Machine = GSTATE_DYNCAST(m_EndNode->GetParent(), state_machine);
    Machine->SetPreferredExit(m_PreferredExit);

    // If the end node is a state machine and we're overriding it's start state, slam it in now. 
    if( m_DestinationStartNode )
    {
        state_machine* EndMachine = GSTATE_DYNCAST(m_EndNode, state_machine);
        if(EndMachine)
        {
            EndMachine->SetStartStateOverride(m_DestinationStartNode);
        }

        // Todo: this could maybe be considered slow. If so, we could cache the state machine
        // that we care about in the transition too. 
        blend_graph* BlendGraph = GSTATE_DYNCAST(m_EndNode, blend_graph);
        if( BlendGraph )
        {
            for( int ChildIdx=0; ChildIdx<BlendGraph->GetNumChildren(); ChildIdx++)
            {
                state_machine* AsStateMachine = GSTATE_DYNCAST(BlendGraph->GetChild(ChildIdx), state_machine);
                if( AsStateMachine )
                {
                    for (int StateMachineChildIdx = 0; StateMachineChildIdx < AsStateMachine->GetNumChildren(); ++StateMachineChildIdx)
                    {
                        node* StateMachineChild = AsStateMachine->GetChild(StateMachineChildIdx);
                        if( StateMachineChild == m_DestinationStartNode )
                        {
                            AsStateMachine->SetStartStateOverride(m_DestinationStartNode);
                        }
                    }
                }
            }
        }
    }
    m_EndNode->Activate(AtT);

    // Try to sync for now
    granny_real32 MidPoint = AtT + m_transitionToken->Duration * 0.5f;
    ExecuteSyncPlan(MidPoint);
}

void GSTATE
transition::Deactivate()
{

    m_Active = false;
    m_ActiveTimer = -1;
}


void  GSTATE
transition::AdvanceT(granny_real32 AtT, granny_real32 DeltaT)
{
    GStateAssert(IsActive());
    GStateAssert(DeltaT >= 0);
    GStateAssert(m_StartNode && m_EndNode);

    m_StartNode->AdvanceT(AtT, DeltaT);
    m_EndNode->AdvanceT(AtT, DeltaT);

    bool WasActive = m_ActiveTimer > 0;
    m_ActiveTimer -= DeltaT;
    if (!WasActive && m_ActiveTimer <= 0)
    {
        m_Active = false;
    }
}

bool GSTATE
transition::IsActive() const
{

    return m_Active != 0;  // silence!
}

bool GSTATE
transition::ShouldActivate(int PassNumber,
                           activate_trigger Trigger,
                           granny_real32 AtT,
                           granny_real32 DeltaT)
{
    // Just check the conditional here.
    return CheckConditionals(AtT);
}

void GSTATE
transition::Note_ConditionalDelete(conditional* Conditional)
{
    TakeTokenOwnership();

    for (int Idx = 0; Idx < GetNumConditionals(); ++Idx)
    {
        if (m_Conditionals[Idx] != Conditional)
            continue;

        m_Conditionals[Idx] = 0;
        m_transitionToken->Conditionals[Idx].Object = 0;
        m_transitionToken->Conditionals[Idx].Type   = 0;
    }
}

void GSTATE
transition::Note_NodeDelete(node* DeletedNode)
{
    if (DeletedNode == 0)
        return;

    TakeTokenOwnership();

    for (int Idx = 0; Idx < GetNumSyncs(); ++Idx)
    {
        // Look at both the reference node and the sync node.  If either matches, then
        // remove it from the conditional spec as well.
        if (m_ReferenceNodes[Idx] == DeletedNode)
        {
            m_transitionToken->Syncs[Idx].ReferenceNode.Object = 0;
            m_transitionToken->Syncs[Idx].ReferenceNode.Type = 0;
            m_transitionToken->Syncs[Idx].ReferenceEventEdge = -1;
            m_transitionToken->Syncs[Idx].EventName = 0;

            m_ReferenceNodes[Idx] = 0;
        }
        else if (m_SyncedNodes[Idx] == DeletedNode)
        {
            m_transitionToken->Syncs[Idx].SyncedNode.Object = 0;
            m_transitionToken->Syncs[Idx].SyncedNode.Type = 0;
            m_transitionToken->Syncs[Idx].SyncedEventEdge = -1;
            m_transitionToken->Syncs[Idx].EventName = 0;

            m_SyncedNodes[Idx] = 0;
        }
    }
}

void GSTATE
transition::Note_OutputEdgeDelete(node*         Node,
                                  granny_int32x DeleteEdge)
{
    TakeTokenOwnership();

    for (int Idx = 0; Idx < GetNumSyncs(); ++Idx)
    {
        // Look at both the reference node and the sync node.  If either matches, then
        // remove it from the conditional spec as well.
        if (m_ReferenceNodes[Idx] == Node && m_transitionToken->Syncs[Idx].ReferenceEventEdge >= 0)
        {
            GStateAssert(GS_InRange(m_transitionToken->Syncs[Idx].ReferenceEventEdge, Node->GetNumOutputs()));
            GStateAssert(Node->GetOutputType(m_transitionToken->Syncs[Idx].ReferenceEventEdge) == EventEdge);

            if (DeleteEdge == m_transitionToken->Syncs[Idx].ReferenceEventEdge)
            {
                // It's the event edge that's actually going away.
                m_transitionToken->Syncs[Idx].ReferenceEventEdge = -1;
                m_transitionToken->Syncs[Idx].EventName = 0;
            }
            else if (DeleteEdge > m_transitionToken->Syncs[Idx].ReferenceEventEdge)
            {
                // It's an edge below our event edge
                --m_transitionToken->Syncs[Idx].ReferenceEventEdge;
            }
        }
        if (m_SyncedNodes[Idx] == Node && m_transitionToken->Syncs[Idx].SyncedEventEdge >= 0)
        {
            GStateAssert(GS_InRange(m_transitionToken->Syncs[Idx].SyncedEventEdge, Node->GetNumOutputs()));
            GStateAssert(Node->GetOutputType(m_transitionToken->Syncs[Idx].SyncedEventEdge) == EventEdge);

            if (DeleteEdge == m_transitionToken->Syncs[Idx].SyncedEventEdge)
            {
                // It's the event edge that's actually going away.
                m_transitionToken->Syncs[Idx].SyncedEventEdge = -1;
                m_transitionToken->Syncs[Idx].EventName = 0;
            }
            else if (DeleteEdge > m_transitionToken->Syncs[Idx].SyncedEventEdge)
            {
                // It's an edge below our event edge
                --m_transitionToken->Syncs[Idx].SyncedEventEdge;
            }
        }
    }
}

void GSTATE
transition::RuntimeOverrideTransitionDuration(float Duration)
{
    m_transitionToken->Duration = Duration;
}
