// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_clamp_value.cpp $
// $DateTime: 2013/02/11 15:03:10 $
// $Change: 41326 $
// $Revision: #4 $
//
// $Notice: $
// ========================================================================
#include "gstate_clamp_value.h"
#include "gstate_anim_utils.h"

#include "gstate_node_visitor.h"
#include "gstate_character_instance.h"
#include "gstate_token_context.h"

#include <string.h>
#include <math.h>

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE clamp_value::clamp_valueImpl
{
    granny_real32 MinValue;
    granny_real32 MaxValue;
};

granny_data_type_definition GSTATE
clamp_value::clamp_valueImplType[] =
{
    { GrannyReal32Member, "MinValue" },
    { GrannyReal32Member, "MaxValue" },
    { GrannyEndMember },
};

// clamp_value is a concrete class, so we must create a slotted container
struct clamp_value_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(clamp_value);
};

granny_data_type_definition clamp_value::clamp_valueTokenType[] =
{
    DECL_UID_MEMBER(clamp_value),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(clamp_value),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(clamp_value);
IMPL_CREATE_NODE_BOILERPLATE(clamp_value);


GSTATE
clamp_value::clamp_value(token_context*               Context,
                         granny_data_type_definition* TokenType,
                         void*                        TokenObject,
                         token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_clamp_valueToken(0)
{
    IMPL_INIT_FROM_TOKEN(clamp_value);

    if (EditorCreated())
    {
        // Add our default input/output
        AddInputImpl(ScalarEdge, "Unclamped");
        AddOutputImpl(ScalarEdge, "Clamped");
    }
}


GSTATE
clamp_value::~clamp_value()
{
    DTOR_RELEASE_TOKEN(clamp_value);
}


bool GSTATE
clamp_value::FillDefaultToken(granny_data_type_definition* TokenType,
                              void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares clamp_valueImpl*& Slot = // member
    GET_TOKEN_SLOT(clamp_value);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->MinValue = 0.0f;
    Slot->MaxValue = 1.0f;

    return true;
}


void GSTATE
clamp_value::SetBounds(granny_real32 Min, granny_real32 Max)
{
    TakeTokenOwnership();

    m_clamp_valueToken->MinValue = Min;
    m_clamp_valueToken->MaxValue = Max;
}

void GSTATE
clamp_value::GetBounds(granny_real32* MinOutput,
                       granny_real32* MaxOutput)
{
    if (MinOutput)
        *MinOutput = m_clamp_valueToken->MinValue;

    if (MaxOutput)
        *MaxOutput = m_clamp_valueToken->MaxValue;
}


granny_real32 GSTATE
clamp_value::SampleScalarOutput(granny_int32x OutputIdx, granny_real32 AtT, gstate_scalar_track_entry * TrackEntry)
{
    INPUT_CONNECTION(OutputIdx, Scalar);

    granny_real32 Value = 0.0f;
    if (ScalarNode)
    {
        Value = ScalarNode->SampleScalarOutput(ScalarEdge, AtT, TrackEntry);
    }

    if (m_clamp_valueToken->MinValue <= m_clamp_valueToken->MaxValue)
    {
        return Clamp(m_clamp_valueToken->MinValue,
                     Value,
                     m_clamp_valueToken->MaxValue);
    }
    else
    {
        return Value;
    }
}

bool GSTATE
clamp_value::GetScalarOutputRange(granny_int32x OutputIdx,
                                  granny_real32* OutMin,
                                  granny_real32* OutMax)
{
    if (m_clamp_valueToken->MinValue <= m_clamp_valueToken->MaxValue)
    {
        *OutMin = m_clamp_valueToken->MinValue;
        *OutMax = m_clamp_valueToken->MaxValue;
        return true;
    }
    else
    {
        return false;
    }
}


granny_int32x GSTATE
clamp_value::GetOutputPassthrough(granny_int32x OutputIdx) const
{
    return OutputIdx;
}
