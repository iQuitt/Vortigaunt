// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_transition_dynamic.cpp $
// $DateTime: 2012/08/28 11:13:50 $
// $Change: 39051 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================
#include "gstate_transition_dynamic.h"
#include "gstate_token_context.h"
#include "gstate_node.h"
#include "gstate_state_machine.h"
#include "gstate_telemetry.h"
#include "gstate_character_instance.h"
#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE tr_dynamic::tr_dynamicImpl
{
    granny_int32 Placeholder;
};

granny_data_type_definition GSTATE
tr_dynamic::tr_dynamicImplType[] =
{
    { GrannyInt32Member, "Placeholder" },
    { GrannyEndMember },
};


// transition is a concrete class, so we must create a slotted container
struct tr_dynamic_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(transition);
    DECL_TOKEN_SLOT(tr_dynamic);
};

granny_data_type_definition tr_dynamic::tr_dynamicTokenType[] =
{
    DECL_UID_MEMBER(tr_dynamic),
    DECL_TOKEN_MEMBER(transition),
    DECL_TOKEN_MEMBER(tr_dynamic),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(tr_dynamic);
IMPL_CREATE_DEFAULT(tr_dynamic);


GSTATE
tr_dynamic::tr_dynamic(token_context*               Context,
                           granny_data_type_definition* TokenType,
                           void*                        TokenObject,
                           token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_tr_dynamicToken(0),
    m_InterruptedTransition(0)
{
    IMPL_INIT_FROM_TOKEN(tr_dynamic);
}


GSTATE
tr_dynamic::~tr_dynamic()
{
    DTOR_RELEASE_TOKEN(tr_dynamic);
}


transition_type GSTATE
tr_dynamic::GetTransitionType() const
{
    return Transition_Dynamic;
}

bool GSTATE
tr_dynamic::FillDefaultToken(granny_data_type_definition* TokenType,
                                   void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares tr_dynamicImpl*& Slot = // member
    GET_TOKEN_SLOT(tr_dynamic);

    // Our slot in this token should be empty.
    // Create a new tr_dynamic token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}


bool GSTATE
tr_dynamic::ShouldActivate(int PassNumber,
                             activate_trigger Trigger,
                             granny_real32 AtT,
                             granny_real32 DeltaFromLastCheck)
{
    //Dynamic transitions are only ever triggered from code, so we should never hit ShouldActivate for them. 
    GStateAssert(false);
    return false;
}

void GSTATE
tr_dynamic::InitializeDynamicTransition(node* Start, node* Dest, bool InterruptedTransition, float Duration, transition * AnyTransitionSource)
{
    Deactivate();

    m_StartNode = Start;
    m_EndNode = Dest;

    m_Conditionals = NULL;
    m_PreferredExit = NULL;

    m_ReferenceNodes = NULL;
    m_SyncedNodes = NULL;

    m_InterruptedTransition = InterruptedTransition;

    RuntimeOverrideTransitionDuration(Duration);
    if( AnyTransitionSource )
    {
        m_PreferredExit = AnyTransitionSource->GetPreferredExit();
        m_DestinationStartNode = AnyTransitionSource->GetDestinationStartNode();
        RuntimeOverrideEventHandlingScheme(AnyTransitionSource->GetEventHandlingScheme());
    }
}

granny_local_pose* GSTATE
tr_dynamic::SamplePose(granny_real32 AtT,
                       granny_real32 AllowedError,
                       granny_pose_cache* PoseCache,
                       granny_real32* PoseWeight)
{
    state_machine* Machine = GSTATE_DYNCAST(m_EndNode->GetParent(), state_machine);

    GSTATE_AUTO_ZONE_FN();
    GStateAssert(IsActive());

    float const Dest = ComputeDestFactor();

    // Have to do the work of blending these together
    granny_local_pose* StartPose = 0;
    {
        GSTATE_AUTO_ZONE(StartNode);
        StartPose = Machine->GetDynamicTransitionSourcePose();
    }

        granny_local_pose* EndPose = 0;
        {
            GSTATE_AUTO_ZONE(EndNode);
            granny_int32x EndExt   = m_EndNode->GetNthExternalOutput(0);
            EndPose = m_EndNode->SamplePoseOutput(EndExt, AtT, AllowedError, PoseCache, PoseWeight);
        }

        granny_model* Model = GetModelForCharacterAutomatic(Machine->GetBoundCharacter());
        if (!Model)
            return 0;

        if (StartPose && EndPose)
        {
            granny_skeleton* Skeleton = Model->Skeleton;
            granny_local_pose* IntoPose = GrannyGetCacheLocalPose(PoseCache, Skeleton->BoneCount);

            GrannyLinearBlend(IntoPose, StartPose, EndPose, Dest);

            // Release endpose before we return
            GrannyReleaseCachePose(PoseCache, EndPose);
            return IntoPose;
        }
        else if (StartPose)
        {
            //Do not hand the dynamic transition source pose back to callers... it could get recycled 
            //internally at any time
            granny_skeleton* Skeleton = Model->Skeleton;
            granny_local_pose* IntoPose = GrannyGetCacheLocalPose(PoseCache, Skeleton->BoneCount);
            GrannyCopyLocalPose(StartPose, IntoPose);

            return IntoPose;
        }
        else if (EndPose)
        {
            return EndPose;
        }
        else
        {
            // Well, shoot. rest pose, I guess
            return 0;
        }
}

bool GSTATE
tr_dynamic::GetRootMotionVectors(granny_real32  AtT,
                                 granny_real32  DeltaT,
                                 granny_real32* ResultTranslation,
                                 granny_real32* ResultRotation,
                                 bool           Inverse)
{
    GStateAssert(m_StartNode);
    GStateAssert(m_EndNode);

    // For now, this is always just a lerp
    float Dest = 1.0f;
    float Duration = GetDuration();
    if (Duration > 0)
    {
        Dest = 1.0f - (m_ActiveTimer / Duration);
        if (Dest < 0)
            Dest = 0;
        else if (Dest > 1)
            Dest = 1;
    }

    granny_int32x EndExt   = m_EndNode->GetNthExternalOutput(0);

    state_machine* Machine = GSTATE_DYNCAST(m_EndNode->GetParent(), state_machine);
    if (Dest == 0)
    {
        ResultTranslation = Machine->GetDynamicTransitionSourceRootTranslation();
        ResultRotation = Machine->GetDynamicTransitionSourceRootRotation();
        return true;
    }
    else if (Dest == 1)
    {
        return m_EndNode->GetRootMotionVectors(EndExt, AtT, DeltaT, ResultTranslation, ResultRotation, Inverse);
    }
    else
    {
        granny_real32* StartRot = Machine->GetDynamicTransitionSourceRootTranslation();
        granny_real32* StartTrans = Machine->GetDynamicTransitionSourceRootRotation();

        granny_real32 EndRot[3];
        granny_real32 EndTrans[3];

        bool StartValid = true;
        bool EndValid   = m_EndNode->GetRootMotionVectors(EndExt, AtT, DeltaT, EndTrans, EndRot, Inverse);

        if (StartValid && EndValid)
        {
            {for (int Idx = 0; Idx < 3; ++Idx)
            {
                ResultTranslation[Idx] = StartTrans[Idx] * (1 - Dest) + EndTrans[Idx] * Dest;
                ResultRotation[Idx]    = StartRot[Idx]   * (1 - Dest) + EndRot[Idx]   * Dest;
            }}
            return true;
        }
        else if (StartValid)
        {
            memcpy(ResultTranslation, StartTrans, sizeof(granny_real32)*3);
            memcpy(ResultRotation,    StartRot,   sizeof(granny_real32)*3);
            return true;
        }
        else if (EndValid)
        {
            memcpy(ResultTranslation, EndTrans, sizeof(granny_real32)*3);
            memcpy(ResultRotation,    EndRot,   sizeof(granny_real32)*3);
            return true;
        }
        else
        {
            // Well, shoot. Nothing, I guess
            return false;
        }
    }
}
