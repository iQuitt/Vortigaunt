// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_transition_onconditional.h"

#include "gstate_conditional.h"
#include "gstate_node.h"
#include "gstate_token_context.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE tr_onconditional::tr_onconditionalImpl
{
    granny_int32 Placeholder;
};

granny_data_type_definition GSTATE
tr_onconditional::tr_onconditionalImplType[] =
{
    { GrannyInt32Member, "Placeholder" },
    { GrannyEndMember },
};

// transition is a concrete class, so we must create a slotted container
struct tr_onconditional_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(transition);
    DECL_TOKEN_SLOT(tr_onconditional);
};

granny_data_type_definition tr_onconditional::tr_onconditionalTokenType[] =
{
    DECL_UID_MEMBER(tr_onconditional),
    DECL_TOKEN_MEMBER(transition),
    DECL_TOKEN_MEMBER(tr_onconditional),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(tr_onconditional);
IMPL_CREATE_DEFAULT(tr_onconditional);


GSTATE
tr_onconditional::tr_onconditional(token_context*               Context,
                                   granny_data_type_definition* TokenType,
                                   void*                        TokenObject,
                                   token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_tr_onconditionalToken(0)
{
    IMPL_INIT_FROM_TOKEN(tr_onconditional);
}


GSTATE
tr_onconditional::~tr_onconditional()
{
    DTOR_RELEASE_TOKEN(tr_onconditional);
}


transition_type GSTATE
tr_onconditional::GetTransitionType() const
{
    return Transition_OnConditional;
}

bool GSTATE
tr_onconditional::FillDefaultToken(granny_data_type_definition* TokenType,
                                       void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares tr_onconditionalImpl*& Slot = // member
    GET_TOKEN_SLOT(tr_onconditional);

    // Our slot in this token should be empty.
    // Create a new tr_onconditional token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}

bool GSTATE
tr_onconditional::ShouldActivate(int PassNumber,
                                 activate_trigger Trigger,
                                 granny_real32 AtT,
                                 granny_real32 DeltaFromLastCheck)
{
    // Check the conditionals, but only if they are non-null
    if (m_Conditionals && CheckConditionals(AtT))
        return true;
    
    return false;
}
