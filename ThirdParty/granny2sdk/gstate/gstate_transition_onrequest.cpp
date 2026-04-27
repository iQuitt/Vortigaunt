// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_transition_onrequest.h"
#include "gstate_token_context.h"
#include "gstate_node.h"
#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE tr_onrequest::tr_onrequestImpl
{
    granny_int32 Placeholder;
};

granny_data_type_definition GSTATE
tr_onrequest::tr_onrequestImplType[] =
{
    { GrannyInt32Member, "Placeholder" },
    { GrannyEndMember },
};


// transition is a concrete class, so we must create a slotted container
struct tr_onrequest_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(transition);
    DECL_TOKEN_SLOT(tr_onrequest);
};

granny_data_type_definition tr_onrequest::tr_onrequestTokenType[] =
{
    DECL_UID_MEMBER(tr_onrequest),
    DECL_TOKEN_MEMBER(transition),
    DECL_TOKEN_MEMBER(tr_onrequest),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(tr_onrequest);
IMPL_CREATE_DEFAULT(tr_onrequest);


GSTATE
tr_onrequest::tr_onrequest(token_context*               Context,
                           granny_data_type_definition* TokenType,
                           void*                        TokenObject,
                           token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_tr_onrequestToken(0)
{
    IMPL_INIT_FROM_TOKEN(tr_onrequest);
}


GSTATE
tr_onrequest::~tr_onrequest()
{
    DTOR_RELEASE_TOKEN(tr_onrequest);
}


transition_type GSTATE
tr_onrequest::GetTransitionType() const
{
    return Transition_OnRequest;
}

bool GSTATE
tr_onrequest::FillDefaultToken(granny_data_type_definition* TokenType,
                                   void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares tr_onrequestImpl*& Slot = // member
    GET_TOKEN_SLOT(tr_onrequest);

    // Our slot in this token should be empty.
    // Create a new tr_onrequest token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}


bool GSTATE
tr_onrequest::ShouldActivate(int PassNumber,
                             activate_trigger Trigger,
                             granny_real32 AtT,
                             granny_real32 DeltaFromLastCheck)
{
    if (!CheckConditionals(AtT))
        return false;
    
    return Trigger == Trigger_Requested;
}
