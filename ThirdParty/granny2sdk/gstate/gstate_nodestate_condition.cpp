// ========================================================================
// $Notice: $
// ========================================================================
#include "gstate_nodestate_condition.h"

#include "gstate_node.h"
#include "gstate_state_machine.h"
#include "gstate_token_context.h"
#include "gstate_transition.h"
//#include "gstate_quick_vecs.h"
#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE nodestate_condition::nodestate_conditionImpl
{
    granny_variant Node;
    granny_bool32 TransitioningTo;
    granny_bool32 Active;
    granny_bool32 TransitioningFrom;
};

granny_data_type_definition GSTATE
nodestate_condition::nodestate_conditionImplType[] =
{
    { GrannyVariantReferenceMember, "Node" },
    { GrannyBool32Member, "TransitioningTo" },
    { GrannyBool32Member, "Active" },
    { GrannyBool32Member, "TransitioningFrom" },
    { GrannyEndMember },
};


// nodestate_condition is a concrete class, so we must create a slotted container
struct nodestate_condition_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(conditional);
    DECL_TOKEN_SLOT(nodestate_condition);
};

granny_data_type_definition nodestate_condition::nodestate_conditionTokenType[] =
{
    DECL_UID_MEMBER(nodestate_condition),
    DECL_TOKEN_MEMBER(conditional),
    DECL_TOKEN_MEMBER(nodestate_condition),
    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(nodestate_condition);
IMPL_CREATE_DEFAULT(nodestate_condition);

GSTATE
nodestate_condition::nodestate_condition(token_context*                Context,
                                   granny_data_type_definition* TokenType,
                                   void*                        TokenObject,
                                   token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_nodestate_conditionToken(0),
    m_Node(0)
{
    IMPL_INIT_FROM_TOKEN(nodestate_condition);
}


GSTATE
nodestate_condition::~nodestate_condition()
{
    DTOR_RELEASE_TOKEN(nodestate_condition);
}


conditional_type GSTATE
nodestate_condition::GetType()
{
    return Conditional_NodeState;
}


bool GSTATE
nodestate_condition::GetCheckTransitioningTo()
{
    return m_nodestate_conditionToken->TransitioningTo ? true : false;
}

void GSTATE
nodestate_condition::SetCheckTransitioningTo(bool TransitioningTo)
{
    TakeTokenOwnership();
    m_nodestate_conditionToken->TransitioningTo = TransitioningTo;
}

bool GSTATE
nodestate_condition::GetCheckActive()
{
    return m_nodestate_conditionToken->Active ? true : false;
}

void GSTATE
nodestate_condition::SetCheckActive(bool Active)
{
    TakeTokenOwnership();
    m_nodestate_conditionToken->Active = Active;
}

bool GSTATE
nodestate_condition::GetCheckTransitioningFrom()
{
    return m_nodestate_conditionToken->TransitioningFrom ? true : false;
}

void GSTATE
nodestate_condition::SetCheckTransitioningFrom(bool TransitioningFrom )
{
    TakeTokenOwnership();
    m_nodestate_conditionToken->TransitioningFrom = TransitioningFrom;
}

node* GSTATE
nodestate_condition::GetNode()
{
    return m_Node;
}


void GSTATE
nodestate_condition::SetNode(node* Node)
{
    TakeTokenOwnership();

    m_Node = 0;
    m_nodestate_conditionToken->Node.Object = 0;
    m_nodestate_conditionToken->Node.Type   = 0;

    if (Node != 0)
    {
        if (!Node->GetTypeAndToken(&m_nodestate_conditionToken->Node))
        {
            GS_InvalidCodePath("Unable to obtain type and token for node");
        }
        else
        {
            m_Node = Node;
        }
    }
}

bool GSTATE
nodestate_condition::CaptureNodeLinks()
{
    if (m_nodestate_conditionToken->Node.Object)
    {
        tokenized* Tokenized = GetTokenContext()->GetProductForToken(m_nodestate_conditionToken->Node.Object);

        // See transition::CaptureNodeLinks for reasons why this might not be present
        if (!Tokenized)
            return false;

        m_Node = GSTATE_DYNCAST(Tokenized, node);
        GStateAssert(m_Node);
    }
    else
    {
        m_Node = 0;
    }

    return parent::CaptureNodeLinks();
}

void GSTATE
nodestate_condition::Note_NodeDelete(node* DeletedNode)
{
    // If this is the node that we're tracking, clear out the field.
    if (DeletedNode == m_Node)
        SetNode(0);

    // Send it up the chain for syncs and conditionals...
    parent::Note_NodeDelete(DeletedNode);
}


bool GSTATE
nodestate_condition::FillDefaultToken(granny_data_type_definition* TokenType,
                                 void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares nodestate_conditionImpl*& Slot = // member
    GET_TOKEN_SLOT(nodestate_condition);

    // Our slot in this token should be empty.
    // Create a new nodestate_condition token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->TransitioningTo = true;
    Slot->Active = true;
    Slot->TransitioningFrom = false;

    return true;
}


bool GSTATE
nodestate_condition::IsTrueImpl(granny_real32 AtT,
                             granny_real32 DeltaT)
{
    GStateAssert(GetOwner());

    if( m_Node == NULL )
        return false;

    if( m_nodestate_conditionToken->Active )
    {
        if(GetOwner()->IsNodeActive(m_Node))
            return true;
    }

    if( m_nodestate_conditionToken->TransitioningTo ||
        m_nodestate_conditionToken->TransitioningFrom )
    {
        container * NodeContainer = m_Node->GetParent();
        state_machine* Machine = GSTATE_DYNCAST(NodeContainer, state_machine);
        if (Machine)
        {
            tokenized * Active = Machine->GetActiveElement();
            if( Active )
            {
                transition * CurrentTransition = GSTATE_DYNCAST(Active, transition);
                if(CurrentTransition)
                {
                    if( m_nodestate_conditionToken->TransitioningTo )
                    {
                        if( CurrentTransition->GetEndNode() == m_Node )
                        {
                            return true;
                        }
                    }

                    if( m_nodestate_conditionToken->TransitioningFrom )
                    {
                        if( CurrentTransition->GetStartNode() == m_Node )
                        {
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}
