// ========================================================================
// $Notice: $
// ========================================================================
#include "gstate_parent_condition.h"

#include "gstate_node.h"
#include "gstate_state_machine.h"
#include "gstate_token_context.h"
#include "gstate_quick_vecs.h"
#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE parent_condition::parent_conditionImpl
{
    granny_int32 LevelsUp;
    granny_int32 ConditionIndex;
    granny_variant ExternalStateMachine;
};

granny_data_type_definition GSTATE
parent_condition::parent_conditionImplType[] =
{
    { GrannyInt32Member, "LevelsUp"       },
    { GrannyInt32Member, "ConditionIndex" },
    { GrannyVariantReferenceMember, "ExternalStateMachine" },
    { GrannyEndMember },
};


// parent_condition is a concrete class, so we must create a slotted container
struct parent_condition_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(conditional);
    DECL_TOKEN_SLOT(parent_condition);
};

granny_data_type_definition parent_condition::parent_conditionTokenType[] =
{
    DECL_UID_MEMBER(parent_condition),
    DECL_TOKEN_MEMBER(conditional),
    DECL_TOKEN_MEMBER(parent_condition),
    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(parent_condition);
IMPL_CREATE_DEFAULT(parent_condition);

GSTATE
parent_condition::parent_condition(token_context*                Context,
                                   granny_data_type_definition* TokenType,
                                   void*                        TokenObject,
                                   token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_parent_conditionToken(0),
    m_ExternalStateMachine(0)
{
    IMPL_INIT_FROM_TOKEN(parent_condition);
}


GSTATE
parent_condition::~parent_condition()
{
    DTOR_RELEASE_TOKEN(parent_condition);
}


conditional_type GSTATE
parent_condition::GetType()
{
    return Conditional_ExternalCondition;
}

void GSTATE
parent_condition::GetExternalCondition(state_machine** Machine, int* ConditionIndex)
{
    *Machine = m_ExternalStateMachine;
    *ConditionIndex = m_parent_conditionToken->ConditionIndex;
}


void GSTATE
parent_condition::SetExternalCondition(state_machine* Machine,
                                     int ConditionIndex)
{
    TakeTokenOwnership();

    // Clears the setting
    if (Machine == 0)
    {
        m_parent_conditionToken->ConditionIndex = -1;
        m_parent_conditionToken->ExternalStateMachine.Object = 0;
        m_parent_conditionToken->ExternalStateMachine.Type = 0;
        m_parent_conditionToken->ConditionIndex = -1;
        m_ExternalStateMachine = Machine;
        return;
    }

    m_ExternalStateMachine = Machine;
    m_ExternalStateMachine->GetTypeAndToken(&m_parent_conditionToken->ExternalStateMachine);
    m_parent_conditionToken->ConditionIndex = ConditionIndex;
}


bool GSTATE
parent_condition::FillDefaultToken(granny_data_type_definition* TokenType,
                                 void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares parent_conditionImpl*& Slot = // member
    GET_TOKEN_SLOT(parent_condition);

    // Our slot in this token should be empty.
    // Create a new parent_condition token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->LevelsUp = -1;
    Slot->ConditionIndex = -1;
    Slot->ExternalStateMachine.Object = 0;
    Slot->ExternalStateMachine.Type = 0;

    return true;
}


bool GSTATE
parent_condition::IsTrueImpl(granny_real32 AtT,
                             granny_real32 DeltaT)
{
    GStateAssert(GetOwner());

    if( m_ExternalStateMachine == 0 || m_parent_conditionToken->ConditionIndex < 0 )
    {
        return false;
    }

    if (m_parent_conditionToken->ConditionIndex >= m_ExternalStateMachine->GetNumConditionals())
    {
        GStateOnce(GStateWarning("Invalid parent count for parent_condition node"));
        return false;
    }
    
    return m_ExternalStateMachine->GetConditional(m_parent_conditionToken->ConditionIndex)->IsTrue(AtT);
}

bool GSTATE
parent_condition::CaptureNodeLinks()
{
    if (m_parent_conditionToken->ExternalStateMachine.Object)
    {
        tokenized* Tokenized = GetTokenContext()->GetProductForToken(m_parent_conditionToken->ExternalStateMachine.Object);

        // See transition::CaptureNodeLinks for reasons why this might not be present
        if (!Tokenized)
            return false;

        m_ExternalStateMachine = GSTATE_DYNCAST(Tokenized, state_machine);
        GStateAssert(m_ExternalStateMachine);
    }
    else
    {
        //For upgrading old state machines
        if (m_parent_conditionToken->LevelsUp > 0 )
        {
            container* Walk = GetOwner();
            for (int i = 0; i < m_parent_conditionToken->LevelsUp && Walk; ++i)
            {
                Walk = Walk->GetParent();
            }
            GStateAssert(Walk);

            m_ExternalStateMachine = GSTATE_DYNCAST(Walk, state_machine);
            GStateAssert(m_ExternalStateMachine);

            if( IsEditable() )
            {
                TakeTokenOwnership();
                m_parent_conditionToken->LevelsUp = -1;
                m_ExternalStateMachine->GetTypeAndToken(&m_parent_conditionToken->ExternalStateMachine);
            }
        }
    }

    return parent::CaptureNodeLinks();
}
