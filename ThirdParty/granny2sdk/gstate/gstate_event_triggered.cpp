// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_event_triggered.h"

#include "gstate_node.h"
#include "gstate_state_machine.h"
#include "gstate_token_context.h"
#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE event_triggered::event_triggeredImpl
{
    granny_variant OutputNodeToken;  // note that NULL indicates GetOwner() for back-compat
    granny_int32   OutputIndex;
    char*          EventName;
};


granny_data_type_definition GSTATE
event_triggered::event_triggeredImplType[] =
{
    { GrannyVariantReferenceMember, "OutputNodeToken" },
    { GrannyInt32Member,            "OutputIndex"    },
    { GrannyStringMember,           "EventName"  },
    { GrannyEndMember },
};


// event_triggered is a concrete class, so we must create a slotted container
struct event_triggered_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(conditional);
    DECL_TOKEN_SLOT(event_triggered);
};

granny_data_type_definition event_triggered::event_triggeredTokenType[] =
{
    DECL_UID_MEMBER(event_triggered),
    DECL_TOKEN_MEMBER(conditional),
    DECL_TOKEN_MEMBER(event_triggered),

    { GrannyEndMember }
};

void GSTATE
event_triggered::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(event_triggered);

    // When we take token ownership, future code will assume that it needs to free our
    // owned pointers when they change.  Don't disappoint them.
    GStateCloneString(m_event_triggeredToken->EventName, OldToken->EventName);
}

void GSTATE
event_triggered::ReleaseOwnedToken_event_triggered()
{
    GStateDeallocate(m_event_triggeredToken->EventName);
}


IMPL_CREATE_DEFAULT(event_triggered);

GSTATE
event_triggered::event_triggered(token_context*               Context,
                                 granny_data_type_definition* TokenType,
                                 void*                        TokenObject,
                                 token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_event_triggeredToken(0),
    m_OutputNode(0)
{
    IMPL_INIT_FROM_TOKEN(event_triggered);
}


GSTATE
event_triggered::~event_triggered()
{
    DTOR_RELEASE_TOKEN(event_triggered);
}


conditional_type GSTATE
event_triggered::GetType()
{
    return Conditional_EventTriggered;
}

bool GSTATE
event_triggered::FillDefaultToken(granny_data_type_definition* TokenType,
                                  void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares event_triggeredImpl*& Slot = // member
    GET_TOKEN_SLOT(event_triggered);

    // Our slot in this token should be empty.
    // Create a new event_triggered token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    // Token/Name zeroed by alloc
    Slot->OutputIndex  = -1;

    return true;
}

bool GSTATE
event_triggered::IsTrueImpl(granny_real32 AtT, granny_real32 DeltaT)
{
    // If we're not set, we can't activate
    if (m_OutputNode == 0 ||
        m_event_triggeredToken->OutputIndex == -1 ||
        m_event_triggeredToken->EventName == 0)
    {
        return false;
    }

    gstate_text_track_entry Entries[16];
    granny_int32x NumUsed = 0;
    if (!m_OutputNode->SampleEventOutput(m_event_triggeredToken->OutputIndex,
                                         AtT, DeltaT,
                                         Entries, GStateArrayLen(Entries),
                                         &NumUsed))
    {
        // warn
        return false;
    }

    {for (int Idx = 0; Idx < NumUsed; ++Idx)
    {
        if (strcmp(Entries[Idx].Text, m_event_triggeredToken->EventName) == 0)
            return true;
    }}

    return false;
}


void GSTATE
event_triggered::SetTriggerEvent(node*         OutputNode,
                                 granny_int32x OutputIndex,
                                 char const*   EventName)
{
    TakeTokenOwnership();

    m_OutputNode = OutputNode;
    if (OutputNode != 0)
    {
        OutputNode->GetTypeAndToken(&m_event_triggeredToken->OutputNodeToken);
    }
    else
    {
        m_event_triggeredToken->OutputNodeToken.Object = 0;
        m_event_triggeredToken->OutputNodeToken.Type   = 0;
    }

    m_event_triggeredToken->OutputIndex = OutputIndex;
    GStateReplaceString(m_event_triggeredToken->EventName, EventName);
}

void GSTATE
event_triggered::GetTriggerEvent(node**         OutputNode,
                                 granny_int32x* OutputIndex,
                                 char const**   EventName)
{
    *OutputNode  = m_OutputNode;
    *OutputIndex = m_event_triggeredToken->OutputIndex;
    *EventName   = m_event_triggeredToken->EventName;
}

void GSTATE
event_triggered::Note_OutputEdgeDelete(node*         AffectedNode,
                                       granny_int32x RemovedOutput)
{
    TakeTokenOwnership();

    // We only care if this is our watched node
    if (AffectedNode == m_OutputNode)
    {
        if (m_event_triggeredToken->OutputIndex == RemovedOutput)
            SetTriggerEvent(0, -1, 0);
        else if (m_event_triggeredToken->OutputIndex > RemovedOutput)
            --m_event_triggeredToken->OutputIndex;
    }
    
    parent::Note_OutputEdgeDelete(AffectedNode, RemovedOutput);
}

bool GSTATE
event_triggered::CaptureNodeLinks()
{
    granny_variant& Token = m_event_triggeredToken->OutputNodeToken;
    if (Token.Type == 0)
    {
        m_OutputNode = GetOwner();
    }
    else
    {
        tokenized* Node = GetTokenContext()->GetProductForToken(Token.Object);
        m_OutputNode = GSTATE_DYNCAST(Node, node);
    }

    return parent::CaptureNodeLinks();
}

void GSTATE
event_triggered::Note_NodeDelete(node* AffectedNode)
{
    TakeTokenOwnership();

    // The node is going away, clear it out...
    if (m_OutputNode == AffectedNode)
        SetTriggerEvent(0, -1, 0);

    parent::Note_NodeDelete(AffectedNode);
}

