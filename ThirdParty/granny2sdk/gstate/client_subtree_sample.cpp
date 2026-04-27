// ========================================================================
// $Notice: $
// ========================================================================
#include "client_subtree_sample.h"

#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_quick_vecs.h"
#include "gstate_telemetry.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE subtree_sample::subtree_sampleImpl
{
    granny_int32x DummyMember;
};

#define IS_BIT_SET(bitarray, idx) (bitarray[idx / 8] & (1 << (idx & 0x7)))

#define SET_BIT(bitarray, idx)     bitarray[idx / 8] |=  (1 << (idx & 0x7))
#define CLEAR_BIT(bitarray, idx)   bitarray[idx / 8] &= ~(1 << (idx & 0x7))

granny_data_type_definition GSTATE
subtree_sample::subtree_sampleImplType[] =
{
    { GrannyInt32Member, "DummyMember" },
    { GrannyEndMember },
};

// subtree_sample is a concrete class, so we must create a slotted container
struct subtree_sample_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(subtree_sample);
};

granny_data_type_definition subtree_sample::subtree_sampleTokenType[] =
{
    DECL_UID_MEMBER(subtree_sample),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(subtree_sample),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(subtree_sample);
IMPL_CREATE_NODE_BOILERPLATE(subtree_sample);


GSTATE
subtree_sample::subtree_sample(token_context*               Context,
                               granny_data_type_definition* TokenType,
                               void*                        TokenObject,
                               token_ownership              TokenIsOwned)
  : parent(Context, TokenType, TokenObject, TokenIsOwned),
    m_subtree_sampleToken(0),
    m_BitfieldCount(0),
    m_Bitfields(0)
{
    IMPL_INIT_FROM_TOKEN(subtree_sample);

    if (EditorCreated())
    {
        // Add our default output (goto parent, impl filters)
        AddOutputImpl(PoseEdge,  "Result");
        AddInputImpl(PoseEdge, "BasePose");
    }
}


GSTATE
subtree_sample::~subtree_sample()
{
    DTOR_RELEASE_TOKEN(subtree_sample);
}


bool GSTATE
subtree_sample::FillDefaultToken(granny_data_type_definition* TokenType,
                                 void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares subtree_sampleImpl*& Slot = // member
    GET_TOKEN_SLOT(subtree_sample);

    // Our slot in this token should be empty.
    // Create a new subtree_sample Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}


granny_int32x GSTATE
subtree_sample::AddInput(node_edge_type EdgeType, char const* EdgeName)
{
    if (EdgeType != PoseEdge)
        return false;

    // Simplest to just recreate the binding vars if necessary
    gstate_character_instance* Instance = GetBoundCharacter();
    if (Instance)
        UnbindFromCharacter();

    granny_int32x RetVal = parent::AddInputImpl(EdgeType, EdgeName);

    if (Instance)
        BindToCharacter(Instance);
    
    return RetVal;
}

bool GSTATE
subtree_sample::DeleteInput(granny_int32x InputIndex)
{
    if (InputIndex == 0)
        return false;

    // We need to remove the
    // Simplest to just recreate the binding vars if necessary
    gstate_character_instance* Instance = GetBoundCharacter();
    if (Instance)
        UnbindFromCharacter();

    bool Success = parent::DeleteInputImpl(InputIndex);

    if (Instance)
        BindToCharacter(Instance);

    return Success;
}

granny_int32x GSTATE
subtree_sample::AddOutput(node_edge_type EdgeType, char const* EdgeName)
{
    // Not allowed
    return -1;
}

bool GSTATE
subtree_sample::DeleteOutput(granny_int32x OutputIndex)
{
    // Also not allowed
    return false;
}


granny_local_pose* GSTATE
subtree_sample::SamplePoseOutput(granny_int32x      OutputIdx,
                                 granny_real32      AtT,
                                 granny_real32      AllowedError,
                                 granny_pose_cache* PoseCache,
                                 granny_real32*     PoseWeight)
{
    GSTATE_AUTO_ZONE_FN();
    GStateAssert(OutputIdx == 0);
    GStateAssert(PoseCache);

    INPUT_CONNECTION(0, Base);
    if (BaseNode == 0)
    {
        // If the base node isn't connected, rest pose.
        return 0;
    }

    gstate_character_instance* Instance = GetBoundCharacter();
    if (!Instance)
        return 0;

    granny_model* Model = GetSourceModelForCharacter(Instance);
    if (!Model)
        return 0;

    granny_local_pose* CompositePose = BaseNode->SamplePoseOutput(BaseEdge, AtT, AllowedError, PoseCache, PoseWeight);
    if (!CompositePose)
    {
        // Build a rest pose here for us to blend into, since 0 return is sematically
        // "rest-pose"
        CompositePose = GrannyGetCacheRestLocalPose(PoseCache, Model);
    }

    // Start compositing in the other poses...
    for (int Idx = 1; Idx < GetNumInputs(); ++Idx)
    {
        INPUT_CONNECTION(Idx, Sample);
        if (SampleNode == 0)
            continue;

        granny_local_pose* MixPose = SampleNode->SamplePoseOutput(SampleEdge, AtT, AllowedError, PoseCache, PoseWeight);

        // Subtlety. If the result here is NULL, treat this as "leave alone" rather than
        // "rest-pose".  For now anyways.
        if (!MixPose)
            continue;

        granny_uint8 const* Bitfield = m_Bitfields[Idx-1];
        for (int BoneIdx = 0; BoneIdx < Model->Skeleton->BoneCount; ++BoneIdx)
        {
            if (IS_BIT_SET(Bitfield, BoneIdx))
            {
                *GrannyGetLocalPoseTransform(CompositePose, BoneIdx) =
                    *GrannyGetLocalPoseTransform(MixPose, BoneIdx);
            }
        }
        
        GrannyReleaseCachePose(PoseCache, MixPose);
    }

    return CompositePose;
}

bool GSTATE
subtree_sample::GetRootMotionVectors(granny_int32x  OutputIdx,
                                     granny_real32  AtT,
                                     granny_real32  DeltaT,
                                     granny_real32* ResultTranslation,
                                     granny_real32* ResultRotation,
                                     bool           Inverse)
{
    memset(ResultTranslation, 0, sizeof(granny_real32)*3);
    memset(ResultRotation,    0, sizeof(granny_real32)*3);

    // Use the 0th input for root motion
    INPUT_CONNECTION(0, Sample);

    // Unconnected
    if (!SampleNode)
        return false;

    return SampleNode->GetRootMotionVectors(SampleEdge, AtT, DeltaT,
                                            ResultTranslation, ResultRotation,
                                            Inverse);
}

granny_real32 GSTATE
subtree_sample::GetDuration(granny_int32x OnOutput,
                            granny_real32 AtT)
{
    GStateAssert(OnOutput == 0);

    // Doesn't support this at the moment (maybe on output 0?)
    return -1;
}


bool GSTATE
subtree_sample::DidLoopOccur(granny_int32x OnOutput,
                             granny_real32 AtT,
                             granny_real32 DeltaT)
{
    GStateAssert(OnOutput == 0);

    // Doesn't support this at the moment (maybe on output 0?)
    return false;
}


bool GSTATE
subtree_sample::SetInputName(granny_int32x InputIdx, char const* NewEdgeName)
{
    // We need to unbind, and the rebind once the spec has been reset so we can grab our
    // animation resources, etc.
    gstate_character_instance* Instance = GetBoundCharacter();
    if (Instance)
        UnbindFromCharacter();

    bool Success = parent::SetInputName(InputIdx, NewEdgeName);

    if (Instance)
        BindToCharacter(Instance);

    return Success;
}

bool GSTATE
subtree_sample::InputNameValid(granny_int32x InputIdx)
{
    GStateCheck(GS_InRange(InputIdx, GetNumInputs()), return false);

    if (InputIdx == 0)
        return true;

    // No model bound, so there is no way to know if this is ok
    if (m_Bitfields == 0)
        return false;

    // slowish, obviously, but this is not called at runtime
    gstate_character_instance* Instance = GetBoundCharacter();
    granny_model*    Model    = GetSourceModelForCharacter(Instance);
    granny_skeleton* Skeleton = Model->Skeleton;
    int NumBytes = (Skeleton->BoneCount + 7) / 8;

    // Are any of the bits set?
    for (int Idx = 0; Idx < NumBytes; ++Idx)
    {
        if (m_Bitfields[InputIdx - 1][Idx] != 0)
            return true;
    }

    return false;
}

bool GSTATE
subtree_sample::BindToCharacter(gstate_character_instance* Instance)
{
    if (!parent::BindToCharacter(Instance))
        return false;

    granny_model*    Model    = GetSourceModelForCharacter(Instance);
    granny_skeleton* Skeleton = Model->Skeleton;
    GStateAssert(m_BitfieldCount == 0);
    GStateAssert(m_Bitfields == 0);

    int NumBytes = (Skeleton->BoneCount + 7) / 8;

    m_BitfieldCount = GetNumInputs() - 1;
    if (m_BitfieldCount != 0)
        m_Bitfields = GStateAllocArray(granny_uint8*, m_BitfieldCount);

    for (int Idx = 0; Idx < m_BitfieldCount; ++Idx)
    {
        GStateAllocZeroedArray(m_Bitfields[Idx], NumBytes);

        // Account for the base input
        granny_int32x InputIdx = Idx + 1;

        granny_int32x BoneIdx  = -1;
        if (GrannyFindBoneByNameLowercase(Skeleton, GetInputName(InputIdx), &BoneIdx))
        {
            // Down the chain we go!
            SET_BIT(m_Bitfields[Idx], BoneIdx);

            for (int Child = BoneIdx + 1; Child < Skeleton->BoneCount; ++Child)
            {
                // Multi root trees are almost always an error, but they are possible.
                if (Skeleton->Bones[Child].ParentIndex != GrannyNoParentBone)
                {
                    if (IS_BIT_SET(m_Bitfields[Idx], Skeleton->Bones[Child].ParentIndex))
                        SET_BIT(m_Bitfields[Idx], Child);
                }
            }
        }
    }

    return true;
}

void GSTATE
subtree_sample::UnbindFromCharacter()
{
    for (int Idx = 0; Idx < m_BitfieldCount; ++Idx)
    {
        GStateDeallocate(m_Bitfields[Idx]);
    }
    GStateDeallocate(m_Bitfields);
    m_BitfieldCount = 0;

    parent::UnbindFromCharacter();
}
