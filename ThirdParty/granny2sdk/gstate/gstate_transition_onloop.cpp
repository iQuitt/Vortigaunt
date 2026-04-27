// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_transition_onloop.h"
#include "gstate_token_context.h"
#include "gstate_node.h"
#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE tr_onloop::tr_onloopImpl
{
    granny_real32 Probability;
    granny_real32 LookAhead;
};

granny_data_type_definition GSTATE
tr_onloop::tr_onloopImplType[] =
{
    { GrannyReal32Member, "Probability" },
    { GrannyReal32Member, "LookAhead" },
    { GrannyEndMember },
};


// transition is a concrete class, so we must create a slotted container
struct tr_onloop_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(transition);
    DECL_TOKEN_SLOT(tr_onloop);
};

granny_data_type_definition tr_onloop::tr_onloopTokenType[] =
{
    DECL_UID_MEMBER(tr_onloop),
    DECL_TOKEN_MEMBER(transition),
    DECL_TOKEN_MEMBER(tr_onloop),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(tr_onloop);
IMPL_CREATE_DEFAULT(tr_onloop);

GSTATE
tr_onloop::tr_onloop(token_context*               Context,
                     granny_data_type_definition* TokenType,
                     void*                        TokenObject,
                     token_ownership              TokenIsOwned)
  : parent(Context, TokenType, TokenObject, TokenIsOwned),
    m_tr_onloopToken(0)
{
    IMPL_INIT_FROM_TOKEN(tr_onloop);
}


GSTATE
tr_onloop::~tr_onloop()
{
    DTOR_RELEASE_TOKEN(tr_onloop);
}


transition_type GSTATE
tr_onloop::GetTransitionType() const
{
    return Transition_OnLoop;
}

bool GSTATE
tr_onloop::FillDefaultToken(granny_data_type_definition* TokenType,
                                void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares tr_onloopImpl*& Slot = // member
    GET_TOKEN_SLOT(tr_onloop);

    // Our slot in this token should be empty.
    // Create a new tr_onloop token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->Probability = 1.0f;
    Slot->LookAhead   = 0.0f;

    return true;
}


granny_real32 GSTATE
tr_onloop::GetProbability() const
{
    return m_tr_onloopToken->Probability;
}

void GSTATE
tr_onloop::SetProbability(granny_real32 Probability)
{
    GStateAssert(Probability >= 0 && Probability <= 1.0f);

    TakeTokenOwnership();
    m_tr_onloopToken->Probability = Probability;
}


granny_real32 GSTATE
tr_onloop::GetLookAhead() const
{
    return m_tr_onloopToken->LookAhead;
}

void GSTATE
tr_onloop::SetLookAhead(granny_real32 LookAhead)
{
    GStateAssert(LookAhead >= 0);

    TakeTokenOwnership();
    m_tr_onloopToken->LookAhead = LookAhead;
}


bool GSTATE
tr_onloop::ShouldActivate(int PassNumber,
                          activate_trigger Trigger,
                          granny_real32 AtT,
                          granny_real32 DeltaFromLastCheck)
{
    if (PassNumber != 0 || Trigger == Trigger_Requested)
        return false;
    
    // Never activate if the conditionals prevent it...
    if (!CheckConditionals(AtT))
        return false;

    // We need to query the start node to see if a loop occurred in the window of
    // interest
    granny_int32x StartExt = m_StartNode->GetNthExternalOutput(0);
    granny_real32 TestT = AtT + m_tr_onloopToken->LookAhead;
    bool LoopOccurred = GetStartNode()->DidLoopOccur(StartExt, TestT, DeltaFromLastCheck);

    if (LoopOccurred)
    {
        granny_real32 Prob = m_tr_onloopToken->Probability;
        if (Prob == 1.0f)
            return true;
        else if (Prob == 0.0f)
            return false;

        else
        {
            return RandomUnit() <= Prob;
        }
    }

    return false;
}


