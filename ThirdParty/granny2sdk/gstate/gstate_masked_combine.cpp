// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_masked_combine.h"
#include "gstate_node_visitor.h"
#include "gstate_character_instance.h"
#include "gstate_token_context.h"
#include "gstate_anim_utils.h"
#include "gstate_telemetry.h"

#include <string.h>
#include <math.h>

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE masked_combine::masked_combineImpl
{
    granny_int32 Dummy;
    granny_bool32 HasAmountInput;
};

granny_data_type_definition GSTATE
masked_combine::masked_combineImplType[] =
{
    { GrannyInt32Member, "Dummy" },
    { GrannyBool32Member, "HasAmountInput" },
    { GrannyEndMember },
};

// masked_combine is a concrete class, so we must create a slotted container
struct masked_combine_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(masked_combine);
};

granny_data_type_definition masked_combine::masked_combineTokenType[] =
{
    DECL_UID_MEMBER(masked_combine),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(masked_combine),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(masked_combine);
IMPL_CREATE_NODE_BOILERPLATE(masked_combine);

//Masked Combine inputs look like: POSE, POSE, MASK, MORPH, MORPH, MORPH, MORPH, MORPH, ...
//Masked Combine outputs look like: POSE, MORPH, MORPH, MORPH, MORPH, MORPH, ...
//So, we use an offset of 2 below to get to the morph channels (it looks like it should be 3, but 
//actually it's 2 because the offsets start 1 later already because the first MORPH is after POSE)
IMPL_PASSTHROUGH_SAMPLEMORPHINTERFACE(masked_combine, (m_masked_combineToken->HasAmountInput ? 3 : 2), true);
IMPL_PASSTHROUGH_SAMPLEEVENTINTERFACE(masked_combine, (m_masked_combineToken->HasAmountInput ? 3 : 2), true);
IMPL_PASSTHROUGH_SAMPLESCALARINTERFACE(masked_combine, (m_masked_combineToken->HasAmountInput ? 3 : 2), true);


GSTATE
masked_combine::masked_combine(token_context*               Context,
                               granny_data_type_definition* TokenType,
                               void*                        TokenObject,
                               token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_masked_combineToken(0)
{
    IMPL_INIT_FROM_TOKEN(masked_combine);

    if (EditorCreated())
    {
        m_masked_combineToken->HasAmountInput = true;

        // Add our default input/output
        AddInputImpl(ScalarEdge,"Amount");
        AddInputImpl(PoseEdge,  "From Pose");
        AddInputImpl(PoseEdge,  "To Pose");
        AddInputImpl(MaskEdge,  "Mask");
        AddOutputImpl(PoseEdge, "Pose");
    }
}


GSTATE
masked_combine::~masked_combine()
{
    DTOR_RELEASE_TOKEN(masked_combine);
}

bool GSTATE
masked_combine::FillDefaultToken(granny_data_type_definition* TokenType,
                                 void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares masked_combineImpl*& Slot = // member
    GET_TOKEN_SLOT(masked_combine);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}


void GSTATE
masked_combine::DeleteInputsForOutput(granny_int32x OutputIndex)
{
    DeleteInput(OutputIndex + (m_masked_combineToken->HasAmountInput ? 3 : 2));
}

void GSTATE
masked_combine::SetInputNamesForOutput(granny_int32x OutputIndex, char const* NewEdgeName)
{
    SetInputName(OutputIndex + (m_masked_combineToken->HasAmountInput ? 3 : 2), NewEdgeName);
}

granny_local_pose* GSTATE
masked_combine::SamplePoseOutput(granny_int32x OutputIdx,
                                 granny_real32 AtT,
                                 granny_real32 AllowedError,
                                 granny_pose_cache* PoseCache,
                                 granny_real32* PoseWeight)
{
    GSTATE_AUTO_ZONE_FN_OR_NAME();

    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(PoseCache);

    float Amount = 1.0f;
    node* AmountNode = 0;
    node* FromNode = 0;
    node* ToNode  = 0;
    node* MaskNode    = 0;
    granny_int32x AmountEdge  = -1;
    granny_int32x FromEdge  = -1;
    granny_int32x ToEdge    = -1;
    granny_int32x MaskEdge  = -1;

    if( m_masked_combineToken->HasAmountInput )
    {
        GetInputConnection(0, &AmountNode, &AmountEdge);
        GetInputConnection(1, &FromNode, &FromEdge);
        GetInputConnection(2, &ToNode,   &ToEdge);
        GetInputConnection(3, &MaskNode, &MaskEdge);
    }
    else
    {
        GetInputConnection(0, &FromNode, &FromEdge);
        GetInputConnection(1, &ToNode,   &ToEdge);
        GetInputConnection(2, &MaskNode, &MaskEdge);
    }

    if (!(FromNode && ToNode && MaskNode))
        return 0;

    gstate_character_instance* Instance = GetBoundCharacter();
    granny_model* Model = GetModelForCharacterAutomatic(Instance);

    if( AmountNode )
    {
        granny_real32 MinVal, MaxVal;
        if (AmountNode->GetScalarOutputRange(AmountEdge, &MinVal, &MaxVal) == false)
        {
            MinVal = 0;
            MaxVal = 1;
        }
        GStateAssert(MinVal <= MaxVal);

        Amount = AmountNode->SampleScalarOutput(AmountEdge, AtT, NULL);
    }

    granny_real32 FromPoseWeight = 1.0f;
    granny_local_pose* FromPose = FromNode->SamplePoseOutput(FromEdge, AtT, AllowedError, PoseCache, &FromPoseWeight);

    if( Amount > 0.0f )
    {
        granny_track_mask* Mask = GrannyGetCacheTrackMask(PoseCache, Model->Skeleton->BoneCount);
        granny_real32 ToPoseWeight = 1.0f;
        granny_local_pose* ToPose   = ToNode->SamplePoseOutput(ToEdge, AtT, AllowedError, PoseCache, &ToPoseWeight);
        if (MaskNode->SampleMaskOutput(MaskEdge, AtT, Mask) && FromPose && ToPose)
        {
            granny_real32 At0 = 0;
            granny_real32 At1 = ToPoseWeight * Amount;
            GrannyModulationCompositeLocalPose(FromPose, At0, At1, Mask, ToPose);
        }

        if (ToPose)
            GrannyReleaseCachePose(PoseCache, ToPose);

        GrannyReleaseCacheTrackMask(PoseCache, Mask);
    }
    return FromPose;
}

bool GSTATE
masked_combine::GetRootMotionVectors(granny_int32x OutputIdx,
                                     granny_real32 AtT,
                                     granny_real32 DeltaT,
                                     granny_real32* Translation,
                                     granny_real32* Rotation,
                                     bool Inverse)
{
    GStateAssert(GetBoundCharacter());

    if (DeltaT < 0)
    {
        GS_PreconditionFailed;
        return false;
    }
    if (!Translation || !Rotation)
    {
        GS_PreconditionFailed;
        return false;
    }

    float Amount = 1.0f;
    node* AmountNode = 0;
    node* FromNode = 0;
    node* ToNode  = 0;
    node* MaskNode    = 0;
    granny_int32x AmountEdge  = -1;
    granny_int32x FromEdge  = -1;
    granny_int32x ToEdge    = -1;
    granny_int32x MaskEdge  = -1;

    if( m_masked_combineToken->HasAmountInput )
    {
        GetInputConnection(0, &AmountNode, &AmountEdge);
        GetInputConnection(1, &FromNode, &FromEdge);
        GetInputConnection(2, &ToNode,   &ToEdge);
        GetInputConnection(3, &MaskNode, &MaskEdge);
    }
    else
    {
        GetInputConnection(0, &FromNode, &FromEdge);
        GetInputConnection(1, &ToNode,   &ToEdge);
        GetInputConnection(2, &MaskNode, &MaskEdge);
    }

    if( AmountNode )
    {
        granny_real32 MinVal, MaxVal;
        if (AmountNode->GetScalarOutputRange(AmountEdge, &MinVal, &MaxVal) == false)
        {
            MinVal = 0;
            MaxVal = 1;
        }
        GStateAssert(MinVal <= MaxVal);

        Amount = AmountNode->SampleScalarOutput(AmountEdge, AtT, NULL);
    }

    if (!(FromNode && ToNode && MaskNode))
        return false;

    bool Success = false;
    gstate_character_instance* Instance = GetBoundCharacter();
    granny_model* Model = GetModelForCharacterAutomatic(Instance);
    granny_track_mask* Mask = Amount > 0.0f ? GrannyNewTrackMask(0.0f, Model->Skeleton->BoneCount) : NULL;
    if (Amount > 0.0f && MaskNode->SampleMaskOutput(MaskEdge, AtT, Mask))
    {
        // Might have some work to do here...
        granny_real32 RootWeight = GrannyGetTrackMaskBoneWeight(Mask, 0);
        if (RootWeight == 0)
        {
            Success = FromNode->GetRootMotionVectors(FromEdge, AtT, DeltaT,
                                                     Translation, Rotation,
                                                     Inverse);
        }
        else if (RootWeight == 1)
        {
            Success = ToNode->GetRootMotionVectors(ToEdge, AtT, DeltaT,
                                                   Translation, Rotation,
                                                   Inverse);
        }
        else
        {
            // Gots to blend.
            granny_real32 FT[3] = { 0, 0, 0 };
            granny_real32 FR[3] = { 0, 0, 0 };
            granny_real32 TT[3] = { 0, 0, 0 };
            granny_real32 TR[3] = { 0, 0, 0 };

            Success = (FromNode->GetRootMotionVectors(FromEdge, AtT, DeltaT,
                                                      FT, FR, Inverse) &&
                       ToNode->GetRootMotionVectors(ToEdge, AtT, DeltaT,
                                                    TT, TR, Inverse));
            if (Success)
            {
                {for (int Idx = 0; Idx < 3; ++Idx)
                {
                    Translation[Idx] = FT[Idx] * (1 - RootWeight) + TT[Idx] * RootWeight;
                    Rotation[Idx]    = FR[Idx] * (1 - RootWeight) + TR[Idx] * RootWeight;
                }}
            }
            else
            {
                memcpy(Translation, FT, sizeof(FT));
                memcpy(Rotation,    FR, sizeof(FR));
            }
        }
    }
    else
    {
        Success = FromNode->GetRootMotionVectors(FromEdge, AtT, DeltaT,
                                                 Translation, Rotation,
                                                 Inverse);
    }

    if( Amount > 0.0f )
    {
        GrannyFreeTrackMask(Mask);
    }
    return Success;
}

void GSTATE
masked_combine::GetAutoLinkInfo( granny_int32x InputIdx, granny_bool32& UseAutoLink, granny_int32x& AutoLinkSkipOffset )
{
    //Only auto-link the first pose input. 
    if (m_masked_combineToken->HasAmountInput)
    {
        UseAutoLink = (InputIdx == 1);
        AutoLinkSkipOffset = 4; //There is one scalar input, two pose inputs and a mask input for additive blend. Skip them. 
    }
    else
    {
        UseAutoLink = (InputIdx == 0);
        AutoLinkSkipOffset = 3; //There are two pose inputs and a mask input for additive blend. Skip them. 
    }
}

granny_int32 GSTATE
masked_combine::GetInputIndexFromVisualIndex(granny_int32 Index) const 
{ 
    const int NumStandardInputs = m_masked_combineToken->HasAmountInput ? 4 : 3; //(Scalar), Pose, Pose, Mask
    const int ParamAndFirstPose = m_masked_combineToken->HasAmountInput ? 2 : 1; //(Scalar), Pose

    int NumExtraInputs = GetNumInputs() - NumStandardInputs;

    //Do this so the extra parameters go after the first pose input, rather than at the end of everything. 
    if( Index < ParamAndFirstPose )
    {
        //Param and first pose input
        return Index;
    }
    else if( Index - ParamAndFirstPose < NumExtraInputs)
    {
        //The extra inputs, shifted up to be after first pose
        int ZeroBasedIndex = Index - ParamAndFirstPose;
        return NumStandardInputs + ZeroBasedIndex;
    }
    else
    {
        //The other two pose inputs and the mask input
        int ZeroBasedIndex = Index - (ParamAndFirstPose + NumExtraInputs);
        return ParamAndFirstPose + ZeroBasedIndex;
    }
}
