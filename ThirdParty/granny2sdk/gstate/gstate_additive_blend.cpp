// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_additive_blend.h"

#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_telemetry.h"
#include "gstate_token_context.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

enum
{
    eParamEdge = 0,
    eIntoEdge  = 1,
    eBaseEdge  = 2,
    eDeltaEdge = 3,
    eMaskEdge  = 4
};

struct GSTATE additive_blend::additive_blendImpl
{
    granny_int32 DummyMember;
};

granny_data_type_definition GSTATE
additive_blend::additive_blendImplType[] =
{
    { GrannyInt32Member, "DummyMember" },
    { GrannyEndMember },
};

// additive_blend is a concrete class, so we must create a slotted container
struct additive_blend_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(additive_blend);
};

granny_data_type_definition additive_blend::additive_blendTokenType[] =
{
    DECL_UID_MEMBER(additive_blend),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(additive_blend),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(additive_blend);
IMPL_CREATE_NODE_BOILERPLATE(additive_blend);

//Additive Blend inputs look like: PARAM, POSE, POSE, POSE, MASK, MORPH, MORPH, MORPH, MORPH, MORPH, ...
//Additive Blend outputs look like: POSE, MORPH, MORPH, MORPH, MORPH, MORPH, ...
//So, we use an offset of 4 below to get to the morph channels (it looks like it should be 5, but 
//actually it's 4 because the offsets start 1 later already because the first MORPH is after POSE)
IMPL_PASSTHROUGH_SAMPLEMORPHINTERFACE(additive_blend, 4, true);
IMPL_PASSTHROUGH_SAMPLEEVENTINTERFACE(additive_blend, 4, true);
IMPL_PASSTHROUGH_SAMPLESCALARINTERFACE(additive_blend, 4, true);

GSTATE
additive_blend::additive_blend(token_context*               Context,
                               granny_data_type_definition* TokenType,
                               void*                        TokenObject,
                               token_ownership              TokenIsOwned)
  : parent(Context, TokenType, TokenObject, TokenIsOwned),
    m_additive_blendToken(0)
{
    IMPL_INIT_FROM_TOKEN(additive_blend);

    // Editing context?
    if (EditorCreated())
    {
        // Default is one pose output and the inputs you would expect for additive blending
        AddOutputImpl(PoseEdge,  "Pose");

        granny_int32x ParamInput = AddInputImpl(ScalarEdge, "Amount");
        granny_int32x IntoInput  = AddInputImpl(PoseEdge,   "Into");
        granny_int32x BaseInput  = AddInputImpl(PoseEdge,   "Base");
        granny_int32x DeltaInput = AddInputImpl(PoseEdge,   "Delta");
        granny_int32x MaskInput  = AddInputImpl(MaskEdge,   "Mask");
        GStateAssert(ParamInput == eParamEdge &&
                     IntoInput  == eIntoEdge &&
                     BaseInput  == eBaseEdge &&
                     DeltaInput == eDeltaEdge &&
                     MaskInput  == eMaskEdge);
        GStateUnused(ParamInput);
        GStateUnused(IntoInput);
        GStateUnused(BaseInput);
        GStateUnused(DeltaInput);
        GStateUnused(MaskInput);
    }
}

GSTATE
additive_blend::~additive_blend()
{
    DTOR_RELEASE_TOKEN(additive_blend);
}

bool GSTATE
additive_blend::FillDefaultToken(granny_data_type_definition* TokenType,
                                 void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares additive_blendImpl*& Slot = // member
    GET_TOKEN_SLOT(additive_blend);

    // Our slot in this token should be empty.
    // Create a new additive_blend Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}

// TODO: finish this up...
granny_local_pose* GSTATE
additive_blend::SamplePoseOutput(granny_int32x      OutputIdx,
                                 granny_real32      AtT,
                                 granny_real32      AllowedError,
                                 granny_pose_cache* PoseCache,
                                 granny_real32*     PoseWeight)
{
    GSTATE_AUTO_ZONE_FN_OR_NAME();

    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(PoseCache);

    // Can't do anything if input is missing
    INPUT_CONNECTION(eIntoEdge, Into);
    if (!IntoNode)
        return 0;

    // We will always use this
    granny_local_pose* IntoPose = IntoNode->SamplePoseOutput(IntoEdge, AtT, AllowedError, PoseCache, PoseWeight);

    // With no param, just grab the base bit...
    INPUT_CONNECTION(eParamEdge, Param);
    if (!ParamNode)
        return IntoPose;

    INPUT_CONNECTION(eBaseEdge,  Base);
    INPUT_CONNECTION(eDeltaEdge, Delta);
    if (!BaseNode || !DeltaNode)
        return IntoPose;

    // Handle the mask node specially, since this may be an old node that is missing that edge
    node* MaskNode = 0;
    granny_int32x MaskEdge = -1;
    if (GetNumInputs() > eMaskEdge)
    {
        GetInputConnection(eMaskEdge, &MaskNode, &MaskEdge);
    }

    // NOW sample the parameter, since we're probably going to use it...
    granny_real32 Amount = ParamNode->SampleScalarOutput(ParamEdge, AtT, NULL);

    // If there is 0 blended in, don't bother to sample the pose...
    if (Amount == 0.0f)
    {
        return IntoPose;
    }

    granny_real32 BasePoseWeight = 1.0f; //Ignored, but need to pass something here. 
    granny_local_pose* BasePose = BaseNode->SamplePoseOutput(BaseEdge, AtT, AllowedError, PoseCache, &BasePoseWeight);
    if (!BasePose)
        return IntoPose;

    granny_real32 AdditivePoseWeight = 1.0f; //Additive blend contribution will get scaled out with the mask weight
    granny_local_pose* DeltaPose = DeltaNode->SamplePoseOutput(DeltaEdge, AtT, AllowedError, PoseCache, &AdditivePoseWeight);
    if (!DeltaPose)
    {
        GrannyReleaseCachePose(PoseCache, BasePose);
        return IntoPose;
    }

    granny_model* Model = GetModelForCharacterAutomatic(GetBoundCharacter());
    GStateAssert(Model);
    granny_skeleton* Skeleton = Model->Skeleton;
    GStateAssert(Skeleton);

    bool MaskBlended = false;
    if (MaskNode != 0)
    {
        granny_track_mask* Mask = GrannyGetCacheTrackMask(PoseCache, Skeleton->BoneCount);
        if (Mask)
        {
            if (MaskNode->SampleMaskOutput(MaskEdge, AtT, Mask))
            {
                GrannyMaskedAdditiveBlend(IntoPose, DeltaPose, BasePose, 0, Skeleton->BoneCount, Mask, Amount * AdditivePoseWeight);
                MaskBlended = true;
            }
            
            GrannyReleaseCacheTrackMask(PoseCache, Mask);
        }
    }

    // Fall through for ease of error handling above.  Additive Blend if we didn't above.
    if (!MaskBlended)
    {
        GrannyAdditiveBlend(IntoPose, DeltaPose, BasePose, 0, Skeleton->BoneCount, Amount * AdditivePoseWeight);
    }

    GrannyReleaseCachePose(PoseCache, BasePose);
    GrannyReleaseCachePose(PoseCache, DeltaPose);

    return IntoPose;
}

bool GSTATE
additive_blend::GetRootMotionVectors(granny_int32x  OutputIdx,
                                     granny_real32  AtT,
                                     granny_real32  DeltaT,
                                     granny_real32* ResultTranslation,
                                     granny_real32* ResultRotation,
                                     bool           Inverse)
{
    GStateAssert(GetBoundCharacter());

    if (DeltaT < 0)
    {
        GS_PreconditionFailed;
        return false;
    }
    if (!ResultTranslation || !ResultRotation)
    {
        GS_PreconditionFailed;
        return false;
    }

    node* PoseNode = 0;
    granny_int32x PoseEdge = -1;
    GetInputConnection(eIntoEdge, &PoseNode, &PoseEdge);

    // The assumption is that only the "Into" Edge matters here...
    if (PoseNode)
    {
        return PoseNode->GetRootMotionVectors(PoseEdge, AtT, DeltaT,
                                              ResultTranslation, ResultRotation,
                                              Inverse);
    }
    else
    {
        memset(ResultTranslation, 0, sizeof(granny_real32)*3);
        memset(ResultRotation,    0, sizeof(granny_real32)*3);
        return false;
    }
}

void GSTATE
additive_blend::GetAutoLinkInfo( granny_int32x InputIdx, granny_bool32& UseAutoLink, granny_int32x& AutoLinkSkipOffset )
{
    //Only auto-link the first pose input. 
    GStateAssert( GetInputType(1) == PoseEdge );
    UseAutoLink = InputIdx == 1;

    AutoLinkSkipOffset = 3; //There are three pose inputs for additive blend. Skip them. 
}

granny_int32 GSTATE
additive_blend::GetInputIndexFromVisualIndex(granny_int32 Index) const 
{ 
    const int NumStandardInputs = 5; //Param, Pose, Pose, Pose, Mask
    const int ParamAndFirstPose = 2; //Param, Pose

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
