// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_conditional.h"

#include "gstate_container.h"
#include "gstate_node.h"
#include "gstate_snapshotutils.h"
#include "gstate_state_machine.h"
#include "gstate_token_context.h"

#include "gstate_scalar_compare.h"
#include "gstate_event_triggered.h"


#include <stdlib.h>
#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

char const* GSTATE
NameForConditionalType(conditional_type Type)
{
    switch (Type)
    {
        case Conditional_ScalarCompare:   return "Scalar Compare";
        case Conditional_EventTriggered:  return "Event Triggered";
        case Conditional_ExternalCondition: return "External Condition";
        case Conditional_NodeState: return "Node State";
        default:
            GS_InvalidCodePath("Unknown conditional type");
            return "Unknown";
    }
}

struct GSTATE conditional::conditionalImpl
{
    char*         Name;
    granny_bool32 InitialState;

    granny_variant OwnerToken;
};

granny_data_type_definition GSTATE
conditional::conditionalImplType[] =
{
    { GrannyStringMember, "Name" },
    { GrannyBool32Member, "InitialState" },
    { GrannyVariantReferenceMember, "Owner" },
    { GrannyEndMember },
};


void GSTATE
conditional::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(conditional);
    GStateCloneString(m_conditionalToken->Name, OldToken->Name);
}


void GSTATE
conditional::ReleaseOwnedToken_conditional()
{
    GStateDeallocate(m_conditionalToken->Name);
}


GSTATE
conditional::conditional(token_context*               Context,
                         granny_data_type_definition* TokenType,
                         void*                        TokenObject,
                         token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_conditionalToken(0),
    m_Owner(0),
    m_LastValid(-1)
{
    IMPL_INIT_FROM_TOKEN(conditional);
    {
        if (m_conditionalToken->OwnerToken.Object)
        {
            // This really isn't optional, if our parent was never created, we should not have
            // been created either.
            tokenized* Owner = GetTokenContext()->GetProductForToken(m_conditionalToken->OwnerToken.Object);
            m_Owner = GSTATE_DYNCAST(Owner, state_machine);
            GStateAssert(m_Owner != 0);
        }
    }
}

GSTATE
conditional::~conditional()
{
    DTOR_RELEASE_TOKEN(conditional);
}


char const* GSTATE
conditional::GetName() const
{
    if (m_conditionalToken->Name == 0)
        return "<unnamed>";

    return m_conditionalToken->Name;
}

bool GSTATE
conditional::SetName(char const* NewName)
{
    TakeTokenOwnership();

    if (NewName == 0)
    {
        return false;
    }

    // replace with copy of NewName
    GStateReplaceString(m_conditionalToken->Name, NewName);

    return true;
}

bool GSTATE
conditional::GetInitialState() const
{
    return (m_conditionalToken->InitialState != 0);
}

void GSTATE
conditional::SetInitialState(bool InitialState)
{
    TakeTokenOwnership();
    m_conditionalToken->InitialState = InitialState;
}


bool GSTATE
conditional::FillDefaultToken(granny_data_type_definition* TokenType,
                              void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares conditionalImpl*& Slot = // member
    GET_TOKEN_SLOT(conditional);

    // Our slot in this token should be empty.
    // Create a new conditional token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}

state_machine* GSTATE
conditional::GetOwner()
{
    return m_Owner;
}

void GSTATE
conditional::SetOwner(state_machine* NewOwner)
{
    // You're not supposed to set this if you're in a non-editable context
    GStateAssert(IsEditable());
    TakeTokenOwnership();

    if (NewOwner)
    {
        GStateAssert(m_Owner == 0);

        m_Owner = NewOwner;

        // Set the token slot
        if (!m_Owner->GetTypeAndToken(&m_conditionalToken->OwnerToken))
        {
            GS_InvalidCodePath("That's very bad.");
        }
    }
    else
    {
        // This is a request to clear the owner (probably being deleted)
        m_Owner = 0;
        m_conditionalToken->OwnerToken.Type   = 0;
        m_conditionalToken->OwnerToken.Object = 0;
    }
}

void GSTATE
conditional::Activate(granny_real32 AtT)
{
    m_LastValid = AtT;
    m_LastObservation = (m_conditionalToken->InitialState != 0);
}

bool GSTATE
conditional::IsTrue(granny_real32 AtT)
{
    if (m_LastValid == AtT)
        return m_LastObservation;

    //GStateAssert(AtT > m_LastValid);
    m_LastObservation = IsTrueImpl(AtT, AtT - m_LastValid);
    m_LastValid = AtT;

    return m_LastObservation;
}

bool GSTATE
conditional::CaptureNodeLinks()
{
    return true;
}

void GSTATE
conditional::Note_OutputEdgeDelete(node*         AffectedNode,
                                   granny_int32x RemovedOutput)
{
    // nothing
}

void GSTATE
conditional::Note_NodeDelete(node* DeletedNode)
{
    // nothing
}


CREATE_SNAPSHOT(conditional)
{
    CREATE_WRITE_REAL32(m_LastValid);
    CREATE_WRITE_BOOL(m_LastObservation);
    CREATE_PASS_TO_PARENT();
}

RESET_FROMSNAPSHOT(conditional)
{
    RESET_OFFSET_TRACKING();

    RESET_READ_REAL32(m_LastValid);
    RESET_READ_BOOL(m_LastObservation);
    RESET_PASS_TO_PARENT();
}
