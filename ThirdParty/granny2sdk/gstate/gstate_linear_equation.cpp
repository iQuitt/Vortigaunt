// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_linear_equation.cpp $
// $DateTime: 2013/02/11 15:03:10 $
// $Change: 41326 $
// $Revision: #4 $
//
// $Notice: $
// ========================================================================
#include "gstate_linear_equation.h"

#include "gstate_node_visitor.h"
#include "gstate_character_instance.h"
#include "gstate_token_context.h"

#include <string.h>
#include <math.h>

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE linear_equation::linear_equationImpl
{
    granny_real32 AValue;
    granny_real32 BValue;
};

#define APPLY_EQ(Val) m_linear_equationToken->AValue * (Val) + m_linear_equationToken->BValue


granny_data_type_definition GSTATE
linear_equation::linear_equationImplType[] =
{
    { GrannyReal32Member, "AValue" },
    { GrannyReal32Member, "BValue" },
    { GrannyEndMember },
};

// linear_equation is a concrete class, so we must create a slotted container
struct linear_equation_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(linear_equation);
};

granny_data_type_definition linear_equation::linear_equationTokenType[] =
{
    DECL_UID_MEMBER(linear_equation),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(linear_equation),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(linear_equation);
IMPL_CREATE_NODE_BOILERPLATE(linear_equation);


GSTATE
linear_equation::linear_equation(token_context*               Context,
                                 granny_data_type_definition* TokenType,
                                 void*                        TokenObject,
                                 token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_linear_equationToken(0)
{
    IMPL_INIT_FROM_TOKEN(linear_equation);

    if (EditorCreated())
    {
        // Add our default input/output
        AddInputImpl(ScalarEdge, "Input");
        AddOutputImpl(ScalarEdge, "Output");
    }
}


GSTATE
linear_equation::~linear_equation()
{
    DTOR_RELEASE_TOKEN(linear_equation);
}


bool GSTATE
linear_equation::FillDefaultToken(granny_data_type_definition* TokenType,
                              void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares linear_equationImpl*& Slot = // member
    GET_TOKEN_SLOT(linear_equation);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->AValue = 1.0f;
    Slot->BValue = 0.0f;

    return true;
}

void GSTATE
linear_equation::SetEquation(granny_real32 A,
                             granny_real32 B)
{
    TakeTokenOwnership();

    m_linear_equationToken->AValue = A;
    m_linear_equationToken->BValue = B;
}

void GSTATE
linear_equation::GetEquation(granny_real32* AOutput,
                             granny_real32* BOutput)
{
    if (AOutput)
        *AOutput = m_linear_equationToken->AValue;

    if (BOutput)
        *BOutput = m_linear_equationToken->BValue;
}


granny_real32 GSTATE
linear_equation::SampleScalarOutput(granny_int32x OutputIdx, granny_real32 AtT, gstate_scalar_track_entry * TrackEntry)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(GetNumOutputs() == GetNumInputs());

    INPUT_CONNECTION(OutputIdx, Scalar);

    granny_real32 Value = 0.0f;
    if (ScalarNode)
    {
        Value = ScalarNode->SampleScalarOutput(ScalarEdge, AtT, TrackEntry);
    }

    return APPLY_EQ(Value);
}

bool GSTATE
linear_equation::GetScalarOutputRange(granny_int32x OutputIdx,
                                      granny_real32* OutMin,
                                      granny_real32* OutMax)
{
    INPUT_CONNECTION(OutputIdx, Scalar);
    if (!ScalarNode)
        return false;

    granny_real32 BaseMin = 0;
    granny_real32 BaseMax = 0;
    if (!ScalarNode->GetScalarOutputRange(ScalarEdge, &BaseMin, &BaseMax))
        return false;
    
    granny_real32 LinearOne = APPLY_EQ(BaseMin);
    granny_real32 LinearTwo = APPLY_EQ(BaseMax);

    // Can get flipped...
    if (LinearOne <= LinearTwo)
    {
        *OutMin = LinearOne;
        *OutMax = LinearTwo;
    }
    else
    {
        *OutMax = LinearOne;
        *OutMin = LinearTwo;
    }

    return true;
}


granny_int32x GSTATE
linear_equation::GetOutputPassthrough(granny_int32x OutputIdx) const
{
    return OutputIdx;
}
