// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_mirror.h"

#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_token_context.h"
#include "gstate_telemetry.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE mirror::mirrorImpl
{
    granny_int32  MirrorAxis;  // must be an enum mirror_axis value

    granny_int32  NameSwapCount;  // must be %2 == 0
    char**        NameSwaps;
};

granny_data_type_definition GSTATE
mirror::mirrorImplType[] =
{
    { GrannyInt32Member, "MirrorAxis" },
    { GrannyReferenceToArrayMember, "NameSwaps", GrannyStringType },
    { GrannyEndMember },
};

// mirror is a concrete class, so we must create a slotted container
struct mirror_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(mirror);
};

granny_data_type_definition mirror::mirrorTokenType[] =
{
    DECL_UID_MEMBER(mirror),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(mirror),

    { GrannyEndMember }
};

static GRANNY_CALLBACK(bool)
    MirrorNodeCallback(char const*   BaseName,
                       char *        MirroredNameBuffer,
                       granny_int32x BufferSize,
                       void *        UserData)
{
    mirror::mirrorImpl* Token = (mirror::mirrorImpl*)UserData;

    // Todo: lotsa strlens here
    {for (int Idx = 0; Idx < Token->NameSwapCount; Idx += 2)
    {
        if (Token->NameSwaps[Idx + 0][0] == 0 ||
            Token->NameSwaps[Idx + 1][0] == 0)
        {
            continue;
        }

        int StartIdx = -1;
        int FinishIdx = -1;

        if (_strnicmp(BaseName, Token->NameSwaps[Idx + 0], strlen(Token->NameSwaps[Idx + 0])) == 0)
        {
            StartIdx  = Idx + 1;
            FinishIdx = Idx + 0;
        }
        else if (_strnicmp(BaseName, Token->NameSwaps[Idx + 1], strlen(Token->NameSwaps[Idx + 1])) == 0)
        {
            StartIdx  = Idx + 0;
            FinishIdx = Idx + 1;
        }

        if (StartIdx != -1)
        {
            // Factored out to make sure I don't forget to modify one of the branches.
            int TotalLen = int(strlen(Token->NameSwaps[StartIdx]) +
                               strlen(BaseName + strlen(Token->NameSwaps[FinishIdx])) + 1);
            if (TotalLen > BufferSize)
            {
                GStateError("construction of mirrored name buffer failed!");
                return false;
            }

            GStateAssert(MirroredNameBuffer);
            GS_StrCpy(MirroredNameBuffer, BufferSize, Token->NameSwaps[StartIdx]);
            GS_StrCat(MirroredNameBuffer, BufferSize, BaseName + strlen(Token->NameSwaps[FinishIdx]));
            return true;
        }
    }}

    if ((int)strlen(BaseName) >= BufferSize)
    {
        GStateError("insufficient buffer size for mirror buffer");
        return false;
    }

    GS_StrCpy(MirroredNameBuffer, BufferSize, BaseName);
    return true;
}

void GSTATE
mirror::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(mirror);
    {
        // granny_unbound_track_mask& Mask    = m_mask_sourceToken->Mask;
        // granny_unbound_track_mask& OldMask = OldToken->Mask;
        GStateCloneArray(m_mirrorToken->NameSwaps,
                         OldToken->NameSwaps,
                         m_mirrorToken->NameSwapCount);
        {for (int Idx = 0; Idx < m_mirrorToken->NameSwapCount; ++Idx)
        {
            GStateCloneString(m_mirrorToken->NameSwaps[Idx], OldToken->NameSwaps[Idx]);
        }}
    }
}

void GSTATE
mirror::ReleaseOwnedToken_mirror()
{
    {for (int Idx = 0; Idx < m_mirrorToken->NameSwapCount; ++Idx)
    {
        GStateDeallocate(m_mirrorToken->NameSwaps[Idx]);
    }}
    GStateDeallocate(m_mirrorToken->NameSwaps);
}

IMPL_CREATE_NODE_BOILERPLATE(mirror);
IMPL_PASSTHROUGH_DIDLOOPOCCUR(mirror, OnOutput == 0);
IMPL_PASSTHROUGH_GETDURATION(mirror, OnOutput == 0);

IMPL_PASSTHROUGH_SAMPLEMORPHINTERFACE(mirror, 0, true);
IMPL_PASSTHROUGH_SAMPLESCALARINTERFACE(mirror, 0, true);
IMPL_PASSTHROUGH_SAMPLEEVENTINTERFACE(mirror, 0, true);


GSTATE
mirror::mirror(token_context*               Context,
               granny_data_type_definition* TokenType,
               void*                        TokenObject,
               token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_mirrorToken(0),
    m_MirrorSpec(0)
{
    IMPL_INIT_FROM_TOKEN(mirror);

    if (EditorCreated())
    {
        // Add our default input/output
        AddInputImpl(PoseEdge,  "Pose");
        AddOutputImpl(PoseEdge, "Pose");
    }
}


GSTATE
mirror::~mirror()
{
    DTOR_RELEASE_TOKEN(mirror);
}

bool GSTATE
mirror::FillDefaultToken(granny_data_type_definition* TokenType,
                         void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares mirrorImpl*& Slot = // member
    GET_TOKEN_SLOT(mirror);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->MirrorAxis    = GrannyMirrorXAxis;
    Slot->NameSwapCount = 0;
    Slot->NameSwaps     = 0;

    return true;
}


bool GSTATE
mirror::CreateMirrorSpec()
{
    granny_model* Model = GetModelForCharacterAutomatic(GetBoundCharacter());
    if (!Model)
        return false;

    GStateAssert(m_MirrorSpec == 0);
    m_MirrorSpec =
        GrannyNewMirrorSpecification(Model->Skeleton->BoneCount,
                                     (granny_mirror_axis)m_mirrorToken->MirrorAxis);

    GStateAssert((m_mirrorToken->NameSwapCount & 1) == 0);
    if (!GrannyBuildMirroringIndices(m_MirrorSpec,
                                     Model->Skeleton,
                                     MirrorNodeCallback, m_mirrorToken))
    {
        GrannyFreeMirrorSpecification(m_MirrorSpec);
        m_MirrorSpec = 0;
        return false;
    }

    return true;
}

bool GSTATE
mirror::BindToCharacter(gstate_character_instance* Instance)
{
    if (!parent::BindToCharacter(Instance))
        return false;

    return CreateMirrorSpec();
}


void GSTATE
mirror::UnbindFromCharacter()
{
    parent::UnbindFromCharacter();

    GrannyFreeMirrorSpecification(m_MirrorSpec);
    m_MirrorSpec = 0;
}


granny_mirror_axis GSTATE
mirror::GetMirrorAxis()
{
    return (granny_mirror_axis)m_mirrorToken->MirrorAxis;
}

void GSTATE
mirror::SetMirrorAxis(granny_mirror_axis Axis)
{
    TakeTokenOwnership();

    gstate_character_instance* Instance = GetBoundCharacter();
    if (Instance)
        UnbindFromCharacter();

    m_mirrorToken->MirrorAxis = Axis;

    if (Instance)
        BindToCharacter(Instance);
}

int GSTATE
mirror::GetNameSwapCount()
{
    return m_mirrorToken->NameSwapCount;
}

bool GSTATE
mirror::SetNameSwaps(int NameSwapCount,
                     char const** NameSwaps)
{
    if (NameSwapCount < 0 || (NameSwapCount & 1) != 0)
    {
        GS_PreconditionFailed;
        return false;
    }
    GStateAssert(NameSwaps || NameSwapCount == 0);

    TakeTokenOwnership();

    granny_int32 NewCount = NameSwapCount;
    char** NewSwaps = 0;
    if (NameSwapCount != 0)
    {
        NewSwaps = GStateAllocArray(char*, NameSwapCount);

        {for (int Idx = 0; Idx < NameSwapCount; ++Idx)
        {
            GStateCloneString(NewSwaps[Idx], NameSwaps[Idx]);
        }}
    }

    // remove the existing array
    {
        {for (int Idx = 0; Idx < m_mirrorToken->NameSwapCount; ++Idx)
        {
            GStateDeallocate(m_mirrorToken->NameSwaps[Idx]);
            m_mirrorToken->NameSwaps[Idx] = 0;
        }}
        GStateDeallocate(m_mirrorToken->NameSwaps);
        m_mirrorToken->NameSwaps = 0;
    }

    m_mirrorToken->NameSwapCount = NewCount;
    m_mirrorToken->NameSwaps = NewSwaps;

    // Rebuild mirror spec if present
    if (m_MirrorSpec)
    {
        GrannyFreeMirrorSpecification(m_MirrorSpec);
        m_MirrorSpec = 0;

        return CreateMirrorSpec();
    }
    else
    {
        // Leave it alone
        return true;
    }
}

char const* GSTATE
mirror::GetNameSwap(int Idx)
{
    GStateAssert(Idx >= 0 && Idx < m_mirrorToken->NameSwapCount);

    return m_mirrorToken->NameSwaps[Idx];
}

bool GSTATE
mirror::SetNameSwap(int Idx, char const* NewStr)
{
    GStateAssert(Idx >= 0 && Idx < m_mirrorToken->NameSwapCount);

    TakeTokenOwnership();

    // Replace the exising string
    GStateReplaceString(m_mirrorToken->NameSwaps[Idx], NewStr);

    // Rebuild mirror spec if present
    if (m_MirrorSpec)
    {
        GrannyFreeMirrorSpecification(m_MirrorSpec);
        m_MirrorSpec = 0;

        return CreateMirrorSpec();
    }
    else
    {
        // Leave it alone
        return true;
    }
}

granny_real32 GSTATE
mirror::GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx)
{
    GStateAssert(OutputIdx == 0);

    node* PoseNode = 0;
    granny_int32x PoseEdge = -1;
    GetInputConnection(0, &PoseNode, &PoseEdge);

    if (!PoseNode)
        return 0;

    return PoseNode->GetLocalTimeOffset(AtT, PoseEdge);
}

granny_local_pose* GSTATE
mirror::SamplePoseOutput(granny_int32x OutputIdx,
                         granny_real32 AtT,
                         granny_real32 AllowedError,
                         granny_pose_cache* PoseCache,
                         granny_real32* PoseWeight)
{
    GSTATE_AUTO_ZONE_FN_OR_NAME();

    GStateAssert(OutputIdx == 0);
    GStateAssert(PoseCache != 0);

    INPUT_CONNECTION(0, Pose);
    if (!PoseNode)
        return 0;

    granny_model* Model = GetModelForCharacterAutomatic(GetBoundCharacter());
    if (!Model)
        return 0;
    granny_skeleton* Skeleton = Model->Skeleton;

    granny_local_pose* DefaultPose = PoseNode->SamplePoseOutput(PoseEdge, AtT, AllowedError, PoseCache, PoseWeight);
    if (DefaultPose)
        GrannyMirrorLocalPose(m_MirrorSpec, Skeleton, DefaultPose);

    return DefaultPose;
}


bool GSTATE
mirror::GetRootMotionVectors(granny_int32x  OutputIdx,
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
    GetInputConnection(0, &PoseNode, &PoseEdge);

    if (!PoseNode)
    {
        memset(ResultTranslation, 0, sizeof(granny_real32)*3);
        memset(ResultRotation,    0, sizeof(granny_real32)*3);
        return false;
    }

    if (!PoseNode->GetRootMotionVectors(PoseEdge, AtT, DeltaT,
                                        ResultTranslation, ResultRotation,
                                        Inverse))
    {
        return false;
    }

    // Mirror the motion.  Axes correspond to x=0, y=1, etc.
    ResultTranslation[m_mirrorToken->MirrorAxis] *= -1;
    {for (int Idx = 0; Idx < 3; ++Idx)
    {
        if (Idx == m_mirrorToken->MirrorAxis)
            continue;

        ResultRotation[Idx] *= -1;
    }}
    
    return true;
}

granny_int32x GSTATE
mirror::GetOutputPassthrough(granny_int32x OutputIdx) const
{
    return OutputIdx;
}

