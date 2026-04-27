// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_linear_remap.cpp $
// $DateTime: 2013/02/11 15:03:10 $
// $Change: 41326 $
// $Revision: #4 $
//
// $Notice: $
// ========================================================================
#include "gstate_linear_remap.h"

#include "gstate_anim_utils.h"
#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_token_context.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE linear_remap::linear_remapImpl
{
    // Sorted on x
    granny_int32      RemapPointCount;
    linear_remap::pt* RemapPoints;
};

static granny_data_type_definition
RemapPointType[] =
{
    { GrannyUInt16Member, "x" },
    { GrannyUInt16Member, "y" },
    { GrannyEndMember }
};

granny_data_type_definition GSTATE
linear_remap::linear_remapImplType[] =
{
    { GrannyReferenceToArrayMember, "RemapPoints", RemapPointType },
    { GrannyEndMember },
};

// linear_remap is a concrete class, so we must create a slotted container
struct linear_remap_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(linear_remap);
};

granny_data_type_definition linear_remap::linear_remapTokenType[] =
{
    DECL_UID_MEMBER(linear_remap),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(linear_remap),

    { GrannyEndMember }
};


IMPL_CREATE_NODE_BOILERPLATE(linear_remap);

void GSTATE
linear_remap::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(linear_remap);

    // When we take token ownership, future code will assume that it needs to free our
    // owned pointers when they change.  Don't disappoint them.
    GStateCloneArray(m_linear_remapToken->RemapPoints,
                     OldToken->RemapPoints,
                     m_linear_remapToken->RemapPointCount);
}

void GSTATE
linear_remap::ReleaseOwnedToken_linear_remap()
{
    GStateDeallocate(m_linear_remapToken->RemapPoints);
}


GSTATE
linear_remap::linear_remap(token_context*               Context,
                                 granny_data_type_definition* TokenType,
                                 void*                        TokenObject,
                                 token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_linear_remapToken(0)
{
    IMPL_INIT_FROM_TOKEN(linear_remap);

    if (EditorCreated())
    {
        // Add our default input/output
        AddInputImpl(ScalarEdge, "Input");
        AddOutputImpl(ScalarEdge, "Output");
    }
}


GSTATE
linear_remap::~linear_remap()
{
    DTOR_RELEASE_TOKEN(linear_remap);
}

bool GSTATE
linear_remap::FillDefaultToken(granny_data_type_definition* TokenType,
                              void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares linear_remapImpl*& Slot = // member
    GET_TOKEN_SLOT(linear_remap);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}

granny_real32 GSTATE
linear_remap::SampleScalarOutput(granny_int32x OutputIdx, granny_real32 AtT, gstate_scalar_track_entry * TrackEntry)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(GetNumOutputs() == GetNumInputs());

    INPUT_CONNECTION(OutputIdx, Scalar);

    granny_real32 Value = 0.0f;
    if (ScalarNode)
    {
        Value = ScalarNode->SampleScalarOutput(ScalarEdge, AtT, TrackEntry);

        granny_real32 MinVal = 0;
        granny_real32 MaxVal = 1;
        ScalarNode->GetScalarOutputRange(ScalarEdge, &MinVal, &MaxVal);

        Value = Clamp(0, (Value - MinVal) / (MaxVal - MinVal), 1);
    }

    // Slightly slower than just checking numpoints == 0 for trivial case, but let's keep
    // things together, shall we?
    granny_int32x XAsInt = granny_int32x(Value * 65535.0f + 0.5f);

    // Handle two special cases.  If XAsInt == 0, then we want to use the first remap iff
    // it has an x coordinate of 0.  This allows the initial value to be set.
    //
    // Same thing for X = 1 << 16 - 1, but take the last remap point, natch.
    //
    // (If there aren't remap points that exactly match, fall through to the normal
    // handler...)
    if (XAsInt == 0)
    {
        if (m_linear_remapToken->RemapPointCount != 0 &&
            m_linear_remapToken->RemapPoints[0].x == 0)
        {
            return m_linear_remapToken->RemapPoints[0].y / 65535.0f;
        }
    }
    else if (XAsInt == 65535)
    {
        if (m_linear_remapToken->RemapPointCount != 0 &&
            m_linear_remapToken->RemapPoints[m_linear_remapToken->RemapPointCount - 1].x == 65535)
        {
            return m_linear_remapToken->RemapPoints[m_linear_remapToken->RemapPointCount - 1].y / 65535.0f;
        }
    }

    granny_int32x PrevX = 0;
    granny_int32x PrevY = 0;
    granny_int32x UseX = -1;
    granny_int32x UseY = -1;
    for (int Idx = 0; Idx < m_linear_remapToken->RemapPointCount; ++Idx)
    {
        if (m_linear_remapToken->RemapPoints[Idx].x >= XAsInt)
        {
            UseX = m_linear_remapToken->RemapPoints[Idx].x;
            UseY = m_linear_remapToken->RemapPoints[Idx].y;
            break;
        }

        PrevX = m_linear_remapToken->RemapPoints[Idx].x;
        PrevY = m_linear_remapToken->RemapPoints[Idx].y;
    }

    // We're at the last point, so we map using 1.0 as x/y
    if (UseX == -1)
    {
        UseX = 65535;
        UseY = 65535;
    }
    GStateAssert(PrevX <= XAsInt);  // <= handles 0 case...
    GStateAssert(UseX  >= XAsInt);  // >= handles dupe points and 0/65535 cases.
    GStateAssert(UseX >= PrevX);

    if (PrevX == UseX)
    {
        return PrevY / 65535.0f;
    }
    else
    {
        granny_real32 Param = (XAsInt - PrevX) / granny_real32(UseX - PrevX);

        return (PrevY + Param * (UseY - PrevY)) / 65535.0f;
    }
}

bool GSTATE
linear_remap::GetScalarOutputRange(granny_int32x OutputIdx,
                                   granny_real32* OutMin,
                                   granny_real32* OutMax)
{
    INPUT_CONNECTION(OutputIdx, Scalar);
    if (!ScalarNode)
        return false;

    *OutMin = 0;
    *OutMax = 1;

    return true;
}

granny_int32x GSTATE
linear_remap::GetNumRemapPoints()
{
    return m_linear_remapToken->RemapPointCount;
}

bool GSTATE
linear_remap::GetRemapPoints(pt* Buffer, granny_int32x BufferSize)
{
    if (BufferSize < m_linear_remapToken->RemapPointCount)
        return false;

    for (int Idx = 0; Idx < m_linear_remapToken->RemapPointCount; ++Idx)
    {
        Buffer[Idx] = m_linear_remapToken->RemapPoints[Idx];
    }

    return true;
}

static int SortOnX(void const* One, void const* Two)
{
    linear_remap::pt const* PtOne = (linear_remap::pt const*)One;
    linear_remap::pt const* PtTwo = (linear_remap::pt const*)Two;

    return ((granny_int32x)PtOne->x) - ((granny_int32x)PtTwo->x);
}

bool GSTATE
linear_remap::SetRemapPoints(pt const* Points, granny_int32x PointCount)
{
    GStateAssert(PointCount >= 0);
    
    TakeTokenOwnership();

    if (PointCount != m_linear_remapToken->RemapPointCount)
    {
        // Going to have to reallocate...
        GStateDeallocate(m_linear_remapToken->RemapPoints);

        if (PointCount != 0)
            m_linear_remapToken->RemapPoints = GStateAllocArray(pt, PointCount);
        else
            m_linear_remapToken->RemapPoints = 0;
        
        m_linear_remapToken->RemapPointCount = PointCount;
    }

    for (int Idx = 0; Idx < m_linear_remapToken->RemapPointCount; ++Idx)
        m_linear_remapToken->RemapPoints[Idx] = Points[Idx];

    // Sort the points in x order.
    qsort(m_linear_remapToken->RemapPoints,
          PointCount, sizeof(linear_remap::pt),
          SortOnX);

    return true;
}

granny_int32x GSTATE
linear_remap::GetOutputPassthrough(granny_int32x OutputIdx) const
{
    return OutputIdx;
}
