// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_custom_remap.cpp $
// $DateTime: 2013/02/11 15:03:10 $
// $Change: 41326 $
// $Revision: #4 $
//
// $Notice: $
// ========================================================================
#include "gstate_custom_remap.h"

#include "gstate_anim_utils.h"
#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_token_context.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE custom_remap::custom_remapImpl
{
    granny_int32      RemapPointCount;
    custom_remap::pt* RemapPoints;             //these points are shown in the editor
    granny_int32      SortedRemapPointCount;
    custom_remap::pt* SortedRemapPoints;       //these are sorted for runtime search efficiency
    granny_int32      RemapType;
    granny_real32     Min;
    granny_real32     Max;
    granny_real32     Fallback;
};

static granny_data_type_definition
RemapPointType[] =
{
    { GrannyReal32Member, "x" },
    { GrannyReal32Member, "y" },
    { GrannyEndMember }
};

granny_data_type_definition GSTATE
custom_remap::custom_remapImplType[] =
{
    { GrannyReferenceToArrayMember, "RemapPoints", RemapPointType },
    { GrannyReferenceToArrayMember, "SortedRemapPoints", RemapPointType },
    { GrannyInt32Member, "RemapType" },
    { GrannyReal32Member, "Min" },
    { GrannyReal32Member, "Max" },
    { GrannyReal32Member, "Fallback" },
    { GrannyEndMember },
};

// custom_remap is a concrete class, so we must create a slotted container
struct custom_remap_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(custom_remap);
};

granny_data_type_definition custom_remap::custom_remapTokenType[] =
{
    DECL_UID_MEMBER(custom_remap),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(custom_remap),

    { GrannyEndMember }
};


IMPL_CREATE_NODE_BOILERPLATE(custom_remap);

void GSTATE
custom_remap::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(custom_remap);

    // When we take token ownership, future code will assume that it needs to free our
    // owned pointers when they change.  Don't disappoint them.
    GStateCloneArray(m_custom_remapToken->RemapPoints,
        OldToken->RemapPoints,
        m_custom_remapToken->RemapPointCount);
    GStateCloneArray(m_custom_remapToken->SortedRemapPoints,
        OldToken->SortedRemapPoints,
        m_custom_remapToken->SortedRemapPointCount);
}

void GSTATE
custom_remap::ReleaseOwnedToken_custom_remap()
{
    GStateDeallocate(m_custom_remapToken->RemapPoints);
    GStateDeallocate(m_custom_remapToken->SortedRemapPoints);
}


GSTATE
custom_remap::custom_remap(token_context*               Context,
                                 granny_data_type_definition* TokenType,
                                 void*                        TokenObject,
                                 token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_custom_remapToken(0),
    LastEvalInput(0.0f),
    LastEvalOutput(0.0f),
    LastEvalTime(0.0f),
    LastStrictFailureValue(FLT_MAX)
{
    IMPL_INIT_FROM_TOKEN(custom_remap);

    if (EditorCreated())
    {
        // Add our default input/output
        AddInputImpl(ScalarEdge, "Input");
        AddOutputImpl(ScalarEdge, "Output");
    }
}


GSTATE
custom_remap::~custom_remap()
{
    DTOR_RELEASE_TOKEN(custom_remap);
}


bool GSTATE
custom_remap::FillDefaultToken(granny_data_type_definition* TokenType,
                              void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares custom_remapImpl*& Slot = // member
    GET_TOKEN_SLOT(custom_remap);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}

granny_real32 GSTATE
custom_remap::SampleScalarOutput(granny_int32x OutputIdx, granny_real32 AtT, gstate_scalar_track_entry * TrackEntry)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(GetNumOutputs() == GetNumInputs());

    INPUT_CONNECTION(OutputIdx, Scalar);
    
    LastEvalTime = GetBoundCharacter() ? GetBoundCharacter()->CurrentTime : AtT;

    granny_real32 Param = 0.0f;
    if (ScalarNode)
    {
        Param = ScalarNode->SampleScalarOutput(ScalarEdge, AtT, TrackEntry);

        granny_real32 MinVal = 0;
        granny_real32 MaxVal = 1;
        ScalarNode->GetScalarOutputRange(ScalarEdge, &MinVal, &MaxVal);
    }

    //Passthrough if there are no remap points. 
    if( m_custom_remapToken->SortedRemapPointCount == 0 )
    {
        LastEvalInput = Param;
        LastEvalOutput = Param;
        return Param;
    }

    granny_int32x CurrIdx = 0; //If Param is smaller than all the X values, we'll just use the first one (see clamping later)

    //Set CurrIdx to the entry with the largest X value less than or equal to Param. 
    bool ExactMatch = false;
    for( granny_int32x Idx = 0; Idx < m_custom_remapToken->SortedRemapPointCount; Idx++ )
    {
        if( Param >= m_custom_remapToken->SortedRemapPoints[Idx].x )
        {
            CurrIdx = Idx;
        }

        if( Param == m_custom_remapToken->SortedRemapPoints[Idx].x )
        {
            ExactMatch = true;
        }
    }


    granny_real32 Result = 0.0f;
    switch( m_custom_remapToken->RemapType )
    {
    case CustomRemap_Direct:
        {
            Result = m_custom_remapToken->SortedRemapPoints[CurrIdx].y;
        }
        break;
    case CustomRemap_DirectStrict:
        {
            if( ExactMatch )
            {
                Result = m_custom_remapToken->SortedRemapPoints[CurrIdx].y;
            }
            else
            {
                //Don't spam the output with this error message. 
                if( Param != LastStrictFailureValue )
                {
                    GStateWarning("[SCREENERROR]CustomRemap [%s] node set to Direct (Strict) failed to match input %f.", GetName(), Param);
                    LastStrictFailureValue = Param;
                }
                Result = 0;
            }
        }
        break;
    case CustomRemap_IntegerInterpolated:
    case CustomRemap_FloatInterpolated:
        {
            //Handle the endpoints
            if( Param < m_custom_remapToken->SortedRemapPoints[0].x )
            {
                Result = m_custom_remapToken->SortedRemapPoints[0].y;
            }
            else if( Param >= m_custom_remapToken->SortedRemapPoints[m_custom_remapToken->SortedRemapPointCount-1].x )
            {
                Result = m_custom_remapToken->SortedRemapPoints[m_custom_remapToken->SortedRemapPointCount-1].y;
            }
            else
            {
                granny_int32x NextIdx = CurrIdx+1;
                GStateAssert(NextIdx < m_custom_remapToken->SortedRemapPointCount);

                granny_real32 CurrX = m_custom_remapToken->SortedRemapPoints[CurrIdx].x;
                granny_real32 CurrY = m_custom_remapToken->SortedRemapPoints[CurrIdx].y;
                granny_real32 NextX = m_custom_remapToken->SortedRemapPoints[NextIdx].x;
                granny_real32 NextY = m_custom_remapToken->SortedRemapPoints[NextIdx].y;
 
                granny_real32 DistX = NextX - CurrX;
                granny_real32 SegmentParam = DistX < 0.00001f ? 0.0f : (Param - CurrX) / DistX;
                granny_real32 InterpolatedResult = (1.0f - SegmentParam) * CurrY + SegmentParam * NextY;

                if( CurrY < NextY )
                {
                    InterpolatedResult = Clamp(CurrY, InterpolatedResult, NextY);
                }
                else
                {
                    InterpolatedResult = Clamp(NextY, InterpolatedResult, CurrY);
                }
 
                switch( m_custom_remapToken->RemapType )
                {
                case CustomRemap_IntegerInterpolated:
                    Result = (granny_real32)((granny_int32x)InterpolatedResult);
                    break;
                case CustomRemap_FloatInterpolated:
                    Result = InterpolatedResult;
                    break;
                }
            }

        }
        break;
    case CustomRemap_DirectWithFallback:
        {
            if( ExactMatch )
            {
                Result = m_custom_remapToken->SortedRemapPoints[CurrIdx].y;
            }
            else
            {
                Result = m_custom_remapToken->Fallback;
            }
        }
        break;
    default:
        GStateAssert(false && "Unknown remap type for Custom Remap node!");
    }

    LastEvalInput = Param;
    LastEvalOutput = Result;

    return Result; 
}

bool GSTATE
custom_remap::GetScalarOutputRange(granny_int32x OutputIdx,
                                   granny_real32* OutMin,
                                   granny_real32* OutMax)
{
    INPUT_CONNECTION(OutputIdx, Scalar);
    if (!ScalarNode)
        return false;

    //Passthrough the input parameter's range if there are no remap points
    if( m_custom_remapToken->SortedRemapPointCount == 0 )
    {
        *OutMin = 0;
        *OutMax = 1;
        return ScalarNode->GetScalarOutputRange(ScalarEdge, OutMin, OutMax);
    }

    *OutMin = m_custom_remapToken->Min;
    *OutMax = m_custom_remapToken->Max;

    if( m_custom_remapToken->RemapType == CustomRemap_DirectWithFallback )
    {
        if( m_custom_remapToken->Fallback < *OutMin )
            *OutMin = m_custom_remapToken->Fallback;

        if( m_custom_remapToken->Fallback > *OutMax )
            *OutMax = m_custom_remapToken->Fallback;
    }

    return true;
}

granny_int32x GSTATE
custom_remap::GetNumRemapPoints()
{
    return m_custom_remapToken->RemapPointCount;
}

bool GSTATE
custom_remap::GetRemapPoints(pt* Buffer, granny_int32x BufferSize)
{
    if (BufferSize < m_custom_remapToken->RemapPointCount)
        return false;

    for (int Idx = 0; Idx < m_custom_remapToken->RemapPointCount; ++Idx)
    {
        Buffer[Idx] = m_custom_remapToken->RemapPoints[Idx];
    }

    return true;
}

static int SortOnX(void const* One, void const* Two)
{
    custom_remap::pt const* PtOne = (custom_remap::pt const*)One;
    custom_remap::pt const* PtTwo = (custom_remap::pt const*)Two;

    return PtOne->x > PtTwo->x;
}

static int SortOnY(void const* One, void const* Two)
{
    custom_remap::pt const* PtOne = (custom_remap::pt const*)One;
    custom_remap::pt const* PtTwo = (custom_remap::pt const*)Two;

    return PtOne->y > PtTwo->y;
}

bool GSTATE
custom_remap::SetRemapPoints(pt const* Points, granny_int32x PointCount)
{
    GStateAssert(PointCount >= 0);
    
    TakeTokenOwnership();

    if (PointCount != m_custom_remapToken->RemapPointCount)
    {
        // Going to have to reallocate...
        GStateDeallocate(m_custom_remapToken->RemapPoints);
        GStateDeallocate(m_custom_remapToken->SortedRemapPoints);

        if (PointCount != 0)
        {
            m_custom_remapToken->RemapPoints = GStateAllocArray(pt, PointCount);
            m_custom_remapToken->SortedRemapPoints = GStateAllocArray(pt, PointCount);
        }
        else
        {
            m_custom_remapToken->RemapPoints = 0;
            m_custom_remapToken->SortedRemapPoints = 0;
        }
        
        m_custom_remapToken->RemapPointCount = PointCount;
        m_custom_remapToken->SortedRemapPointCount = PointCount;
    }

    if( PointCount == 0 )
    {
        m_custom_remapToken->Min = 0.0f;
        m_custom_remapToken->Max = 0.0f;

    }
    else
    {
        m_custom_remapToken->Min = FLT_MAX;
        m_custom_remapToken->Max = -FLT_MAX;

        for (int Idx = 0; Idx < m_custom_remapToken->RemapPointCount; ++Idx)
        {
            m_custom_remapToken->RemapPoints[Idx] = Points[Idx];
            m_custom_remapToken->SortedRemapPoints[Idx] = Points[Idx];

            if( m_custom_remapToken->RemapPoints[Idx].y < m_custom_remapToken->Min )
            {
                m_custom_remapToken->Min = m_custom_remapToken->RemapPoints[Idx].y;
            }

            if( m_custom_remapToken->RemapPoints[Idx].y > m_custom_remapToken->Max )
            {
                m_custom_remapToken->Max = m_custom_remapToken->RemapPoints[Idx].y;
            }
        }

        // Sort the points in x order.
        qsort(m_custom_remapToken->SortedRemapPoints,
              PointCount, sizeof(custom_remap::pt),
              SortOnX);
    }

    return true;
}

void GSTATE
custom_remap::SortPointsByX()
{
    // Sort the points in x order.
    qsort(m_custom_remapToken->RemapPoints,
        m_custom_remapToken->RemapPointCount, sizeof(custom_remap::pt),
        SortOnX);
}

void GSTATE
custom_remap::SortPointsByY()
{
    // Sort the points in x order.
    qsort(m_custom_remapToken->RemapPoints,
        m_custom_remapToken->RemapPointCount, sizeof(custom_remap::pt),
        SortOnY);
}

granny_real32 GSTATE
custom_remap::GetFallback()
{
    return m_custom_remapToken->Fallback;
}

void GSTATE
custom_remap::SetFallback(granny_real32 Fallback)
{
    TakeTokenOwnership();
    m_custom_remapToken->Fallback = Fallback;
}


custom_remap_type GSTATE
custom_remap::GetRemapType() const
{
    return (custom_remap_type)m_custom_remapToken->RemapType;
}

void GSTATE
custom_remap::SetRemapType(custom_remap_type Type)
{
    TakeTokenOwnership();
    m_custom_remapToken->RemapType = Type;
}

granny_int32x GSTATE
custom_remap::GetOutputPassthrough(granny_int32x OutputIdx) const
{
    return OutputIdx;
}
