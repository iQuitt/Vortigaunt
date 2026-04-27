// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_scalar_compare.h"

#include "gstate_node.h"
#include "gstate_state_machine.h"
#include "gstate_token_context.h"
#include "gstate_quick_vecs.h"
#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct scalar_compare_spec
{
    granny_variant OutputNodeToken;
    granny_int32   OutputIdx;
    granny_int32   Operation;
    granny_real32  ReferenceValue;
};

struct GSTATE scalar_compare::scalar_compareImpl
{
    granny_int32         CompareCount;
    scalar_compare_spec* Compares;

    granny_bool32        AndConditional;
};


granny_data_type_definition ScalarCompareSpecType[] =
{
    { GrannyVariantReferenceMember, "OutputNodeToken" },
    { GrannyInt32Member,            "OutputIdx"       },
    { GrannyInt32Member,            "Operation"       },
    { GrannyReal32Member,           "ReferenceValue"  },
    { GrannyEndMember }
};

granny_data_type_definition GSTATE
scalar_compare::scalar_compareImplType[] =
{
    { GrannyReferenceToArrayMember, "Compares", ScalarCompareSpecType },
    { GrannyBool32Member, "AndConditional" },
    { GrannyEndMember },
};


// scalar_compare is a concrete class, so we must create a slotted container
struct scalar_compare_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(conditional);
    DECL_TOKEN_SLOT(scalar_compare);
};

granny_data_type_definition scalar_compare::scalar_compareTokenType[] =
{
    DECL_UID_MEMBER(scalar_compare),
    DECL_TOKEN_MEMBER(conditional),
    DECL_TOKEN_MEMBER(scalar_compare),

    { GrannyEndMember }
};

void GSTATE
scalar_compare::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(scalar_compare);

    // When we take token ownership, future code will assume that it needs to free our
    // owned pointers when they change.  Don't disappoint them.
    GStateCloneArray(m_scalar_compareToken->Compares, OldToken->Compares,
                     m_scalar_compareToken->CompareCount);
}

void GSTATE
scalar_compare::ReleaseOwnedToken_scalar_compare()
{
    GStateDeallocate(m_scalar_compareToken->Compares);
}


static inline bool
Satisfied(granny_real32     Value,
          scalar_compare_op Operation,
          granny_real32     ReferenceValue)
{
    GStateAssert(Operation >= ScalarCompare_Less &&
                 Operation <= ScalarCompare_NotEqual);
    switch (Operation)
    {
        case ScalarCompare_Less:
            return Value < ReferenceValue;
        case ScalarCompare_Greater:
            return Value > ReferenceValue;
        case ScalarCompare_LEqual:
            return Value <= ReferenceValue;
        case ScalarCompare_GEqual:
            return Value >= ReferenceValue;
        case ScalarCompare_Equal:
            return Value == ReferenceValue;
        case ScalarCompare_NotEqual:
            return Value != ReferenceValue;
    }

    return false;
}

IMPL_CREATE_DEFAULT(scalar_compare);

GSTATE
scalar_compare::scalar_compare(token_context*                Context,
                               granny_data_type_definition* TokenType,
                               void*                        TokenObject,
                               token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_scalar_compareToken(0),
    m_OutputNodes(0)
{
    IMPL_INIT_FROM_TOKEN(scalar_compare);

    if (m_scalar_compareToken->CompareCount)
        GStateAllocZeroedArray(m_OutputNodes, m_scalar_compareToken->CompareCount);
}


GSTATE
scalar_compare::~scalar_compare()
{
    DTOR_RELEASE_TOKEN(scalar_compare);

    GStateDeallocate(m_OutputNodes);
}


conditional_type GSTATE
scalar_compare::GetType()
{
    return Conditional_ScalarCompare;
}

bool GSTATE
scalar_compare::FillDefaultToken(granny_data_type_definition* TokenType,
                                 void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares scalar_compareImpl*& Slot = // member
    GET_TOKEN_SLOT(scalar_compare);

    // Our slot in this token should be empty.
    // Create a new scalar_compare token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->CompareCount = 0;  // redundant, but let's be safe!
    Slot->Compares     = 0;
    Slot->AndConditional = false;

    return true;
}


bool GSTATE
scalar_compare::IsAndCondition()
{
    return (m_scalar_compareToken->AndConditional != 0);
}


void GSTATE
scalar_compare::SetIsAndCondition(bool AndCondition)
{
    TakeTokenOwnership();

    m_scalar_compareToken->AndConditional = AndCondition;
}


int GSTATE
scalar_compare::GetNumCompares()
{
    return m_scalar_compareToken->CompareCount;
}

bool GSTATE
scalar_compare::AddCompare()
{
    TakeTokenOwnership();
    
    node*& NewNode = QVecPushNewElementNoCount(m_scalar_compareToken->CompareCount,
                                               m_OutputNodes);
    NewNode = 0;

    scalar_compare_spec& NewSpec = QVecPushNewElement(m_scalar_compareToken->CompareCount,
                                                      m_scalar_compareToken->Compares);


    NewSpec.OutputNodeToken.Object = 0;
    NewSpec.OutputNodeToken.Type = 0;
    NewSpec.OutputIdx = -1;
    NewSpec.Operation = ScalarCompare_Greater;
    NewSpec.ReferenceValue = 0.5f;

    return true;
}

bool GSTATE
scalar_compare::RemoveCompare(granny_int32x CompareIndex)
{
    GStateAssert(CompareIndex >= 0 && CompareIndex < m_scalar_compareToken->CompareCount);

    TakeTokenOwnership();

    QVecRemoveElementNoCount(CompareIndex,
                             m_scalar_compareToken->CompareCount,
                             m_OutputNodes);

    QVecRemoveElement(CompareIndex,
                      m_scalar_compareToken->CompareCount,
                      m_scalar_compareToken->Compares);

    return true;
}

bool GSTATE
scalar_compare::GetCompareSpec(granny_int32x      CompareIndex,
                               node**             OutputNode,
                               granny_int32x*     OutputIdx,
                               scalar_compare_op* Operation,
                               granny_real32*     ReferenceValue)
{
    GStateAssert(CompareIndex >= 0 && CompareIndex < m_scalar_compareToken->CompareCount);

    // TODO: REMOVE ME!
    if (m_OutputNodes[CompareIndex] == 0)
        *OutputNode = GetOwner();
    else
        *OutputNode = m_OutputNodes[CompareIndex];


    *OutputIdx      = m_scalar_compareToken->Compares[CompareIndex].OutputIdx;
    *Operation      = (scalar_compare_op)m_scalar_compareToken->Compares[CompareIndex].Operation;
    *ReferenceValue = m_scalar_compareToken->Compares[CompareIndex].ReferenceValue;

    return true;
}

bool GSTATE
scalar_compare::SetCompareSpec(granny_int32x CompareIndex,
                               node*             OutputNode,
                               granny_int32x     OutputIdx,
                               scalar_compare_op Operation,
                               granny_real32     ReferenceValue)
{
    GStateAssert(CompareIndex >= 0 && CompareIndex < m_scalar_compareToken->CompareCount);

    TakeTokenOwnership();

    scalar_compare_spec& Spec = m_scalar_compareToken->Compares[CompareIndex];

    m_OutputNodes[CompareIndex] = OutputNode;
    if (OutputNode != 0)
    {
        OutputNode->GetTypeAndToken(&Spec.OutputNodeToken);
    }
    else
    {
        Spec.OutputNodeToken.Object = 0;
        Spec.OutputNodeToken.Type   = 0;
    }

    Spec.OutputIdx      = OutputIdx;
    Spec.Operation      = Operation;
    Spec.ReferenceValue = ReferenceValue;

    return true;
}

void GSTATE
scalar_compare::Note_NodeDelete(node* AffectedNode)
{
    TakeTokenOwnership();

    {for (int Idx = 0; Idx < m_scalar_compareToken->CompareCount; ++Idx)
    {
        if (m_OutputNodes[Idx] != AffectedNode)
            continue;

        // The node is going away, clear it out...
        scalar_compare_spec& Spec = m_scalar_compareToken->Compares[Idx];
        Spec.OutputNodeToken.Object = 0;
        Spec.OutputNodeToken.Type = 0;
        Spec.OutputIdx = -1;

        m_OutputNodes[Idx] = 0;
    }}

    parent::Note_NodeDelete(AffectedNode);
}

void GSTATE
scalar_compare::Note_OutputEdgeDelete(node*         AffectedNode,
                                      granny_int32x RemovedOutput)
{
    TakeTokenOwnership();

    {for (int Idx = 0; Idx < m_scalar_compareToken->CompareCount; ++Idx)
    {
        if (m_OutputNodes[Idx] != AffectedNode)
            continue;

        scalar_compare_spec& Spec = m_scalar_compareToken->Compares[Idx];
        if (Spec.OutputIdx == RemovedOutput)
        {
            Spec.OutputNodeToken.Object = 0;
            Spec.OutputNodeToken.Type = 0;
            Spec.OutputIdx = -1;

            m_OutputNodes[Idx] = 0;
        }
        else if (Spec.OutputIdx > RemovedOutput)
        {
            --Spec.OutputIdx;
        }
    }}

    parent::Note_OutputEdgeDelete(AffectedNode, RemovedOutput);
}


bool GSTATE
scalar_compare::IsTrueImpl(granny_real32 AtT,
                           granny_real32 DeltaT)
{
    GStateAssert(GetOwner());

    if (m_scalar_compareToken->CompareCount == 0)
        return false;

    {for (int Idx = 0; Idx < m_scalar_compareToken->CompareCount; ++Idx)
    {
        scalar_compare_spec& Compare = m_scalar_compareToken->Compares[Idx];

        bool Success = false;
        if (m_OutputNodes[Idx] && Compare.OutputIdx != -1)
        {
            GStateAssert(m_OutputNodes[Idx]->GetOutputType(Compare.OutputIdx) == ScalarEdge);

            granny_real32 Value = m_OutputNodes[Idx]->SampleScalarOutput(Compare.OutputIdx, AtT, NULL);
            Success = Satisfied(Value,
                                scalar_compare_op(Compare.Operation),
                                Compare.ReferenceValue);
        }

        if (Success && !m_scalar_compareToken->AndConditional)
            return true;
        else if (!Success && m_scalar_compareToken->AndConditional)
            return false;
    }}

    // Slightly subtle.  If this were an "or", then the first true value would have early
    // outed of the loop above.  If it's an "and", then all of the tests passed (since we
    // checked for 0 compares at the start, we can ignore that failure case.)
    return (m_scalar_compareToken->AndConditional != 0);
}

bool GSTATE
scalar_compare::CaptureNodeLinks()
{
    {for (int Idx = 0; Idx < m_scalar_compareToken->CompareCount; ++Idx)
    {
        scalar_compare_spec& Spec = m_scalar_compareToken->Compares[Idx];

        // TODO REMOVE ME!
        if (Spec.OutputNodeToken.Type == 0)
        {
            m_OutputNodes[Idx] = GetOwner();
        }
        else
        {
            tokenized* Node = GetTokenContext()->GetProductForToken(Spec.OutputNodeToken.Object);
            m_OutputNodes[Idx] = GSTATE_DYNCAST(Node, node);
        }
    }}

    return parent::CaptureNodeLinks();
}
