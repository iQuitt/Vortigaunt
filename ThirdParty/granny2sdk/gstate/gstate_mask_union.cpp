// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_mask_union.h"

#include "gstate_node_visitor.h"
#include "gstate_character_instance.h"
#include "gstate_token_context.h"

#include <string.h>
#include <math.h>

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE mask_union::mask_unionImpl
{
    granny_int32 Dummy;
};

granny_data_type_definition GSTATE
mask_union::mask_unionImplType[] =
{
    { GrannyInt32Member, "Dummy" },
    { GrannyEndMember },
};

// mask_union is a concrete class, so we must create a slotted container
struct mask_union_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(mask_union);
};

granny_data_type_definition mask_union::mask_unionTokenType[] =
{
    DECL_UID_MEMBER(mask_union),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(mask_union),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(mask_union);
IMPL_CREATE_NODE_BOILERPLATE(mask_union);
IMPL_PASSTHROUGH_SAMPLEMORPHINTERFACE(mask_union, 0, true);
IMPL_PASSTHROUGH_SAMPLESCALARINTERFACE(mask_union, 0, true);
IMPL_PASSTHROUGH_SAMPLEEVENTINTERFACE(mask_union, 0, true);


GSTATE
mask_union::mask_union(token_context*               Context,
                       granny_data_type_definition* TokenType,
                       void*                        TokenObject,
                       token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_mask_unionToken(0),
    m_CachedBoneCount(-1),
    m_CachedMask(0)
{
    IMPL_INIT_FROM_TOKEN(mask_union);

    if (EditorCreated())
    {
        // Add our default input/output
        AddInputImpl(MaskEdge, "Mask One");
        AddInputImpl(MaskEdge, "Mask Two");
        AddOutputImpl(MaskEdge, "Union");
    }
}


GSTATE
mask_union::~mask_union()
{
    DTOR_RELEASE_TOKEN(mask_union);
}

bool GSTATE
mask_union::BindToCharacter(gstate_character_instance* Instance)
{
    if (!parent::BindToCharacter(Instance))
        return false;

    granny_model* Model = GetModelForCharacterAutomatic(Instance);
    if (Model == 0)
    {
        parent::UnbindFromCharacter();
        return false;
    }

    m_CachedBoneCount = Model->Skeleton->BoneCount;

    GStateAssert(m_CachedMask == 0);
    m_CachedMask = GrannyNewTrackMask(0.0f, m_CachedBoneCount);
    if (m_CachedMask == 0)
    {
        parent::UnbindFromCharacter();
        return false;
    }

    return true;
}

void GSTATE
mask_union::UnbindFromCharacter()
{
    parent::UnbindFromCharacter();

    GrannyFreeTrackMask(m_CachedMask);
    m_CachedMask = 0;
    m_CachedBoneCount = -1;
}

bool GSTATE
mask_union::FillDefaultToken(granny_data_type_definition* TokenType,
                             void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares mask_unionImpl*& Slot = // member
    GET_TOKEN_SLOT(mask_union);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}


bool GSTATE
mask_union::SampleMaskOutput(granny_int32x      OutputIdx,
                             granny_real32      AtT,
                             granny_track_mask* ModelMask)
{
    GStateAssert(OutputIdx == 0);
    GStateAssert(ModelMask != 0);
    GStateAssert(m_CachedBoneCount != -1);
    GStateAssert(m_CachedMask != 0);

    node* MaskOne = 0;
    node* MaskTwo = 0;
    granny_int32x OneEdge = -1;
    granny_int32x TwoEdge = -1;
    GetInputConnection(0, &MaskOne, &OneEdge);
    GetInputConnection(1, &MaskTwo, &TwoEdge);

    if (MaskOne && MaskTwo)
    {
        GStateAssert(m_CachedMask);

        bool TwoSuccess = MaskTwo->SampleMaskOutput(TwoEdge, AtT, ModelMask);
        bool OneSuccess = MaskOne->SampleMaskOutput(OneEdge, AtT, m_CachedMask);
        if (!(TwoSuccess && OneSuccess))
            return false;

        // Union thresholds to 1 or 0
        {for (int Idx = 0; Idx < m_CachedBoneCount; ++Idx)
        {
            granny_real32 WeightOne = GrannyGetTrackMaskBoneWeight(m_CachedMask, Idx);
            granny_real32 WeightTwo = GrannyGetTrackMaskBoneWeight(ModelMask, Idx);

            GrannySetTrackMaskBoneWeight(ModelMask, Idx,
                                         (WeightOne != 0.0f || WeightTwo != 0.0f) ? 1.0f : 0.0f);
        }}

        return true;
    }
    else if (MaskOne)
    {
        return MaskOne->SampleMaskOutput(OneEdge, AtT, ModelMask);
    }
    else if (MaskTwo)
    {
        return MaskTwo->SampleMaskOutput(TwoEdge, AtT, ModelMask);
    }

    return false;
}
