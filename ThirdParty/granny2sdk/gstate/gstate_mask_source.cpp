// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_mask_source.h"

#include "gstate_node_visitor.h"
#include "gstate_character_instance.h"
#include "gstate_token_context.h"

#include <string.h>
#include <math.h>

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE mask_source::mask_sourceImpl
{
    granny_unbound_track_mask Mask;
};

granny_data_type_definition GSTATE
mask_source::mask_sourceImplType[] =
{
    { GrannyInlineMember, "Mask", GrannyUnboundTrackMaskType },
    { GrannyEndMember },
};

// mask_source is a concrete class, so we must create a slotted container
struct mask_source_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(mask_source);
};

granny_data_type_definition mask_source::mask_sourceTokenType[] =
{
    DECL_UID_MEMBER(mask_source),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(mask_source),

    { GrannyEndMember }
};


static void
FreeUnboundMask(granny_unbound_track_mask& Mask)
{
	for (int Idx = 0; Idx < Mask.WeightCount; ++Idx)
    {
        GStateDeallocate(Mask.Weights[Idx].Name);
        Mask.Weights[Idx].Name = 0;
	}
    GStateDeallocate(Mask.Weights);
    Mask.Weights = 0;
}


void GSTATE
mask_source::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(mask_source);
    {
        granny_unbound_track_mask& Mask    = m_mask_sourceToken->Mask;
        granny_unbound_track_mask& OldMask = OldToken->Mask;
        GStateCloneArray(Mask.Weights,
                         OldMask.Weights,
                         Mask.WeightCount);
        {for (int Idx = 0; Idx < Mask.WeightCount; ++Idx)
        {
            GStateCloneString(Mask.Weights[Idx].Name, OldMask.Weights[Idx].Name);
        }}
    }
}


void GSTATE
mask_source::ReleaseOwnedToken_mask_source()
{
    FreeUnboundMask(m_mask_sourceToken->Mask);
}

IMPL_CREATE_NODE_BOILERPLATE(mask_source);

GSTATE
mask_source::mask_source(token_context*               Context,
                         granny_data_type_definition* TokenType,
                         void*                        TokenObject,
                         token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_mask_sourceToken(0),
    m_TrackMask(0)
{
    IMPL_INIT_FROM_TOKEN(mask_source);

    if (EditorCreated())
    {
        // Add our default output
        AddOutputImpl(MaskEdge, "Mask");
    }
}

GSTATE
mask_source::~mask_source()
{
    DTOR_RELEASE_TOKEN(mask_source);
}

bool GSTATE
mask_source::FillDefaultToken(granny_data_type_definition* TokenType,
                              void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares mask_sourceImpl*& Slot = // member
    GET_TOKEN_SLOT(mask_source);

    // Our slot in this token should be empty.
    // Create a new mask source Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}


bool GSTATE
mask_source::BindToCharacter(gstate_character_instance* Instance)
{
    GStateAssert(GetNumOutputs() == 1);
    GStateAssert(m_TrackMask == 0);

    if (!parent::BindToCharacter(Instance))
        return false;

    granny_model* Model = GetModelForCharacterAutomatic(Instance);
    if (!Model || !Model->Skeleton)
        return false;

    m_TrackMask = GrannyNewTrackMask(m_mask_sourceToken->Mask.DefaultWeight, Model->Skeleton->BoneCount);

    // Note that we only affect the unbound mask on the SetMask call...
    GrannyBindTrackmaskToModel(&m_mask_sourceToken->Mask, Model, m_TrackMask);

    return true;
}


void GSTATE
mask_source::UnbindFromCharacter()
{
    GStateAssert(GetNumOutputs() == 1);
    parent::UnbindFromCharacter();

    GrannyFreeTrackMask(m_TrackMask);
    m_TrackMask = 0;

    // Do *not* delete the unbound track mask in the token
}

void GSTATE
mask_source::SetMask(granny_unbound_track_mask* UnboundMask)
{
    GStateAssert(UnboundMask);

    TakeTokenOwnership();
	granny_unbound_track_mask Store = m_mask_sourceToken->Mask;

    // Copy in...
    m_mask_sourceToken->Mask.DefaultWeight = UnboundMask->DefaultWeight;
    m_mask_sourceToken->Mask.WeightCount   = UnboundMask->WeightCount;

    GStateCloneArray(m_mask_sourceToken->Mask.Weights,
                     UnboundMask->Weights,
                     m_mask_sourceToken->Mask.WeightCount);
    for (int Idx = 0; Idx < m_mask_sourceToken->Mask.WeightCount; ++Idx)
    {
        GStateCloneString(m_mask_sourceToken->Mask.Weights[Idx].Name,
                          UnboundMask->Weights[Idx].Name);
    }

    FreeUnboundMask(Store);

    gstate_character_instance* Instance = GetBoundCharacter();
    if (m_TrackMask && Instance)
    {
        granny_model* Model = GetModelForCharacterAutomatic(Instance);
        if (Model)
        {
            // Note that we only affect the unbound mask on the SetMask call...
            GrannyBindTrackmaskToModel(&m_mask_sourceToken->Mask, Model, m_TrackMask);
        }
    }
}

granny_unbound_track_mask const* GSTATE
mask_source::GetMask()
{
    return &m_mask_sourceToken->Mask;
}

bool GSTATE
mask_source::SampleMaskOutput(granny_int32x      OutputIdx,
                              granny_real32      AtT,
                              granny_track_mask* ModelMask)
{
    if (!ModelMask)
        return false;

    return GrannyCopyTrackMask(ModelMask, m_TrackMask);
}
