// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_character_instance.h"
#include "gstate_character_info.h"
#include "gstate_state_machine.h"
#include "gstate_telemetry.h"
#include "gstate_token_context.h"
#include <string.h>

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;
using namespace std;



gstate_character_instance*
GStateInstantiateCharacterDetailed(gstate_character_info* Info,
                                   granny_real32          CurrentTime,
                                   granny_int32x          AnimationSet,
                                   granny_model*          SourceModel,
                                   granny_model*          TargetModel,
                                   granny_mesh**          CharacterMeshes,
                                   granny_int32x          CharacterMeshCount)
{
    GSTATE_AUTO_ZONE_FN();

    if (!Info || Info->StateMachine.Object == 0)
    {
        GStateError("Invalid arguments to InstantiateCharacter");
        return 0;
    }
    if (CharacterMeshCount < 0 || (CharacterMeshCount != 0 && CharacterMeshes == 0))
    {
        GStateError("Invalid arguments (CharacterMeshes) to InstantiateCharacter");
        return 0;
    }

    GStateCheckIndex(AnimationSet, Info->AnimationSetCount, return 0);

    // todo: check that Info's file references have been successfully loaded.

    gstate_character_instance* Result;
    GStateAllocZeroedStruct(Result);
    if (Result)
    {
        // TODO: error handling
        Result->TargetModel  = TargetModel;
        Result->SourceModel  = SourceModel;
        Result->AnimationSet = AnimationSet;
        Result->CurrentTime  = CurrentTime;
        Result->DeltaT       = -1;

        if (CharacterMeshCount != 0)
        {
            // Take our own copy of this array, just in case.  Not much we can do about
            // the mesh data, obv.
            Result->CharacterMeshes = GStateAllocArray(granny_mesh*, CharacterMeshCount);
            Result->CharacterMeshCount = CharacterMeshCount;
            memcpy(Result->CharacterMeshes, CharacterMeshes, sizeof(granny_mesh*) * CharacterMeshCount);
        }
        else
        {
            GStateAssert(Result->CharacterMeshes == 0 && Result->CharacterMeshCount == 0);
        }

        if (TargetModel != SourceModel)
        {
            Result->Retargeter = GrannyAcquireRetargeter(SourceModel, TargetModel);
            if (Result->Retargeter == 0)
            {
                GStateError("Unable to create retargeter for Source(%s) -> Target(%s)",
                            SourceModel->Name, TargetModel->Name);
                GStateDeallocate(Result);
                return 0;
            }
        }
        else
        {
            Result->Retargeter = 0;
        }

        Result->TokenContext  = GStateNew<token_context>(Info->NumUniqueTokenized);
        Result->CharacterInfo = Info;
        {
            tokenized* Product =
                Result->TokenContext->CreateFromToken(Info->StateMachine.Type,
                                                      Info->StateMachine.Object);
            Result->StateMachine = GSTATE_DYNCAST(Product, state_machine);
            if (Result->StateMachine == 0)
            {
                GS_PreconditionFailed;
                GStateDelete<tokenized>(Product);
            }
            else
            {
                Result->StateMachine->CaptureSiblingData();
                Result->StateMachine->BindToCharacter(Result);
                Result->StateMachine->Activate(Result->CurrentTime);
            }
        }

        Result->PoseCache = GrannyNewPoseCache();
    }

    return Result;
}

inline void GStateMatrixEquals4x4(granny_real32 *Dest, granny_real32 const *Source)
{
    Dest[0] = Source[0];
    Dest[1] = Source[1];
    Dest[2] = Source[2];
    Dest[3] = Source[3];
    Dest[4] = Source[4];
    Dest[5] = Source[5];
    Dest[6] = Source[6];
    Dest[7] = Source[7];
    Dest[8] = Source[8];
    Dest[9] = Source[9];
    Dest[10] = Source[10];
    Dest[11] = Source[11];
    Dest[12] = Source[12];
    Dest[13] = Source[13];
    Dest[14] = Source[14];
    Dest[15] = Source[15];
}

void GStateAllocMat4x4Cache(EXPIN gstate_character_instance* Instance, int MaxCacheEntries)
{
    gstate_named_mat4x4 * NewCache = GStateAllocArray(gstate_named_mat4x4, MaxCacheEntries);
    for( int NewCacheIdx=0; NewCacheIdx<MaxCacheEntries; NewCacheIdx++ )
    {
        //Copy info if it's a joint cache resize. 
        if( NewCacheIdx < Instance->Mat4x4CacheCapacity )
        {
            NewCache[NewCacheIdx].Name = Instance->Mat4x4Cache[NewCacheIdx].Name;
            GStateMatrixEquals4x4(NewCache[NewCacheIdx].Mat4x4, Instance->Mat4x4Cache[NewCacheIdx].Mat4x4);
        }
    }

    granny_int32 OldCount = Instance->Mat4x4CacheSize;
    if( Instance->Mat4x4CacheCapacity )
    {
        GStateFreeMat4x4Cache(Instance);
    }

    Instance->Mat4x4CacheCapacity = MaxCacheEntries;
    Instance->Mat4x4Cache = NewCache;
    if( OldCount <= MaxCacheEntries )
    {
        Instance->Mat4x4CacheSize = OldCount;
    }
    else
    {
        Instance->Mat4x4CacheSize = MaxCacheEntries;
    }
}

void GStateFreeMat4x4Cache(EXPIN gstate_character_instance* Instance)
{
    GStateDeallocate(Instance->Mat4x4Cache);
    Instance->Mat4x4Cache = NULL;
    Instance->Mat4x4CacheCapacity = 0;
    Instance->Mat4x4CacheSize = 0;
}

bool GStateCacheMat4x4(EXPIN gstate_character_instance* Instance, const char * Name, granny_real32 const * Pose4x4)
{
    if( Instance->Mat4x4CacheSize >= Instance->Mat4x4CacheCapacity )
    {
        return false;
    }

    gstate_named_mat4x4 * CachedJoint = &Instance->Mat4x4Cache[Instance->Mat4x4CacheSize];
    CachedJoint->Name = Name;

    GStateMatrixEquals4x4(CachedJoint->Mat4x4,Pose4x4);
    Instance->Mat4x4CacheSize++;

    return true;
}

bool GStateGetCachedMat4x4(gstate_character_instance* Instance, const char * Name, granny_real32 * Pose4x4)
{
    for(int CacheIdx=0; CacheIdx<Instance->Mat4x4CacheSize; CacheIdx++)
    {
        const gstate_named_mat4x4* CachedJoint = &Instance->Mat4x4Cache[CacheIdx];
        if( _stricmp(CachedJoint->Name,Name) == 0 )
        {
            GStateMatrixEquals4x4(Pose4x4, CachedJoint->Mat4x4);
            return true;
        }
    }

    return false;
}

bool GStateGetCachedPosition(gstate_character_instance* Instance, const char * Name, granny_real32 * Position3)
{
    for(int CacheIdx=0; CacheIdx<Instance->Mat4x4CacheSize; CacheIdx++)
    {
        const gstate_named_mat4x4* CachedJoint = &Instance->Mat4x4Cache[CacheIdx];
        if( _stricmp(CachedJoint->Name,Name) == 0 )
        {
            Position3[0] = CachedJoint->Mat4x4[12];
            Position3[1] = CachedJoint->Mat4x4[13];
            Position3[2] = CachedJoint->Mat4x4[14];
            return true;
        }
    }

    return false;
}

void GStateClearMat4x4Cache(EXPIN gstate_character_instance* Instance)
{
    Instance->Mat4x4CacheSize = 0;
}

gstate_character_instance*
GStateInstantiateRetargetedCharacter(gstate_character_info* Info,
                                     granny_real32          CurrentTime,
                                     granny_int32x          AnimationSet,
                                     granny_model*          SourceModel,
                                     granny_model*          TargetModel)
{
    return GStateInstantiateCharacterDetailed(Info, CurrentTime, AnimationSet,
                                              SourceModel, TargetModel,
                                              0, 0);
}

gstate_character_instance*
GStateInstantiateCharacter(gstate_character_info* Info,
                           granny_real32          CurrentTime,
                           granny_int32x          AnimationSet,
                           granny_model*          Model)
{
    return GStateInstantiateCharacterDetailed(Info, CurrentTime, AnimationSet, Model, Model, 0, 0);
}

gstate_character_instance* GSTATE
InstantiateEditCharacter(gstate_character_info* Info,
                         granny_int32x AnimationSet,
                         granny_model* RetargetSourceModel,
                         granny_model* Model,
                         granny_mesh** Meshes,
                         granny_int32  MeshCount)
{
    if (!Info)
    {
        GStateError("Invalid arguments to InstantiateEditCharacter");
        return 0;
    }

    // todo: check that Info's file references have been successfully loaded.

    gstate_character_instance* Result;
    GStateAllocZeroedStruct(Result);
    if (Result)
    {
        // TODO: error handling
        Result->TargetModel = Model;
        Result->SourceModel = Model;
        Result->Retargeter  = 0;

        if( RetargetSourceModel != NULL && Model != NULL && RetargetSourceModel != Model )
        {
            Result->SourceModel = RetargetSourceModel;
            Result->Retargeter = GrannyAcquireRetargeter(RetargetSourceModel, Model);
            if (Result->Retargeter == 0)
            {
                GStateError("Unable to create retargeter for Source(%s) -> Target(%s)",
                    RetargetSourceModel->Name, Model->Name);
            }
        }

        Result->TokenContext  = GStateNew<token_context>();
        Result->CharacterInfo = Info;
        Result->StateMachine = 0;
        Result->AnimationSet = AnimationSet;
        Result->CurrentTime  = 0;
        Result->DeltaT       = -1;

        if (MeshCount != 0)
        {
            // Take our own copy of this array, just in case.  Not much we can do about
            // the mesh data, obv.
            Result->CharacterMeshes = GStateAllocArray(granny_mesh*, MeshCount);
            Result->CharacterMeshCount = MeshCount;
            memcpy(Result->CharacterMeshes, Meshes, sizeof(granny_mesh*) * MeshCount);
        }
        else
        {
            GStateAssert(Result->CharacterMeshes == 0 && Result->CharacterMeshCount == 0);
        }

        Result->PoseCache = GrannyNewPoseCache();
    }

    return Result;
}

void GSTATE
UnbindEditCharacter(gstate_character_instance* Instance)
{
    GStateAssert(Instance);
    GStateAssert(Instance->CharacterInfo);

    if (Instance->StateMachine)
    {
        Instance->StateMachine->UnbindFromCharacter();
        Instance->StateMachine = 0;
    }
}

bool GSTATE
ReplaceStateMachine(gstate_character_instance* Instance,
                    state_machine*             NewMachine)
{
    GStateAssert(Instance);
    GStateAssert(Instance->CharacterInfo);

    UnbindEditCharacter(Instance);

    if( Instance->PoseCache )
    {
        GrannyClearPoseCache(Instance->PoseCache);
    }

    GStateFreeMat4x4Cache(Instance);

    Instance->StateMachine = NewMachine;

    bool Success = true;
    if (Instance->StateMachine)
    {
        GStateAssert(Instance->AnimationSet >= 0 &&
                     Instance->AnimationSet < Instance->CharacterInfo->AnimationSetCount);

        Success = Instance->StateMachine->BindToCharacter(Instance);

        granny_model* Model = GetModelForCharacterAutomatic(Instance);
        if (Model)
        {
            granny_skeleton* Skeleton = Model->Skeleton;
            if( Skeleton )
            {
                GStateAllocMat4x4Cache(Instance,Skeleton->BoneCount);
            }
        }

        Instance->StateMachine->Activate(Instance->CurrentTime);
    }

    return Success;
}

void GSTATE
SetCurrentAnimationSet(gstate_character_instance* Instance, granny_int32x Set)
{
    GStateAssert(Instance);
    GStateAssert(Instance->CharacterInfo);
    GStateAssert(Set >= 0 && Set < Instance->CharacterInfo->AnimationSetCount);

    Instance->AnimationSet = Set;
}

void
GStateFreeCharacterInstance(gstate_character_instance* Instance)
{
    if (!Instance)
        return;

    // Lose this if it's around...
    GStateDeallocate(Instance->CharacterMeshes);
    Instance->CharacterMeshCount = 0;

    GrannyReleaseRetargeter(Instance->Retargeter);
    Instance->Retargeter = 0;

    // If we are an edit character, and are not supposed to delete this, it will already
    // be gone...
    if (Instance->StateMachine)
    {
        Instance->StateMachine->UnbindFromCharacter();
        GStateDelete<state_machine>(Instance->StateMachine);
        Instance->StateMachine = 0;
    }

    if( Instance->PoseCache )
    {
        GrannyFreePoseCache(Instance->PoseCache);
        Instance->PoseCache = 0;
    }

    GStateDelete<token_context>(Instance->TokenContext);
    GStateDeallocate(Instance);
}

state_machine*
GStateGetStateMachine(gstate_character_instance* Instance)
{
    if (!Instance)
        return 0;

    return Instance->StateMachine;
}


gstate_character_info*
GStateGetInfoForCharacter(gstate_character_instance* Instance)
{
    if (!Instance)
        return 0;

    return Instance->CharacterInfo;
}

granny_model* GSTATE
GetTargetModelForCharacter(gstate_character_instance* Instance)
{
    if (!Instance)
        return 0;

    return Instance->TargetModel;
}

granny_model* GSTATE
GetSourceModelForCharacter(gstate_character_instance* Instance)
{
    if (!Instance)
        return 0;

    return Instance->SourceModel;
}

granny_model* GSTATE
GetModelForCharacterAutomatic(gstate_character_instance* Instance)
{
    if( !Instance )
        return 0;

    if(Instance->StateMachine && Instance->StateMachine->GetRetargetAnimationSources())
    {
        return Instance->TargetModel;
    }
    else
    {
        return Instance->SourceModel;
    }

}


granny_retargeter* GSTATE
GetCharacterRetargeter(gstate_character_instance* Instance)
{
    if (!Instance)
    {
        GStateError("Invalid Argument to GetCharacterRetargeter");
        return 0;
    }

    return Instance->Retargeter;
}

bool GSTATE
GrannyGetRetargetAnimationSources(gstate_character_instance* Instance)
{
    if (!Instance)
    {
        return false;
    }

    if( !Instance->StateMachine )
        return false;

    return Instance->StateMachine->GetRetargetAnimationSources();
}

bool  GrannyGetRetargetAnimationSources(gstate_character_instance*);


bool
GStateUpdateModelMatrix(gstate_character_instance*  Instance,
                        granny_real32 const*        ModelMatrix4x4,
                        granny_real32*              DestMatrix4x4)
{
    GSTATE_AUTO_ZONE_FN();
    GStateCheckPtrNotNULL(Instance, return false);
    GStateCheckPtrNotNULL(ModelMatrix4x4, return false);
    GStateCheckPtrNotNULL(DestMatrix4x4, return false);
    GStateCheckPtrNotNULL(Instance->StateMachine, return false);

    GStateCheck(Instance->DeltaT >= 0.0f && "Must call GStateAdvanceTime", return false);

    granny_triple ResultTranslation = { 0, 0, 0 };
    granny_triple ResultRotation = { 0, 0, 0 };

    if (!Instance->StateMachine->GetRootMotionVectors(0,
                                                      Instance->CurrentTime,
                                                      Instance->DeltaT,
                                                      ResultTranslation,
                                                      ResultRotation,
                                                      false))
    {
        return false;
    }

    GrannyApplyRootMotionVectorsToMatrix(ModelMatrix4x4,
                                         ResultTranslation, ResultRotation,
                                         DestMatrix4x4);

    return true;
}

void
GStateAdvanceTime(gstate_character_instance* Instance,
                  granny_real32              DeltaT)
{
    GSTATE_AUTO_ZONE_FN();
    GStateCheckPtrNotNULL(Instance, return);
    GStateCheckPtrNotNULL(Instance->StateMachine, return);
    GStateCheck(DeltaT >= 0, return);

    Instance->CurrentTime += DeltaT;
    Instance->DeltaT       = DeltaT;

    Instance->StateMachine->AdvanceT(Instance->CurrentTime, Instance->DeltaT);
}


granny_real32
GStateInstanceTime(EXPIN gstate_character_instance* Instance)
{
    GStateCheckPtrNotNULL(Instance, return 0);

    return Instance->CurrentTime;
}


granny_real32
GStateInstanceLastDeltaT(EXPIN gstate_character_instance* Instance)
{
    GStateCheckPtrNotNULL(Instance, return 0);

    return Instance->DeltaT;
}


granny_local_pose*
GStateSampleAnimationLOD(gstate_character_instance* Instance,
                         granny_real32              AllowedError,
                         granny_pose_cache*         PoseCache)
{
    GSTATE_AUTO_ZONE_FN();
    GStateCheckPtrNotNULL(Instance, return 0);
    GStateCheckPtrNotNULL(PoseCache, return 0);
    GStateCheckPtrNotNULL(Instance->StateMachine, return 0);
    GStateCheck(Instance->DeltaT >= 0 && "Must call GStateAdvanceTime", return 0);

    granny_real32 PoseWeight = 1.0f;
    granny_local_pose* Pose =
        Instance->StateMachine->SamplePoseOutput(0, Instance->CurrentTime, AllowedError, PoseCache, &PoseWeight);
    if (Pose && Instance->Retargeter)
    {
        granny_int32 BoneCount = Instance->TargetModel->Skeleton->BoneCount;
        granny_local_pose* TargPose =
            GrannyGetCacheLocalPose(PoseCache, BoneCount);

        if (GrannyRetargetPose(Pose, TargPose, 0, BoneCount, Instance->Retargeter))
        {
            // Success!
            GrannyReleaseCachePose(PoseCache, Pose);
            Pose = TargPose;
        }
        else
        {
            // Well, that's bad. um
            GrannyReleaseCachePose(PoseCache, TargPose);
            GrannyReleaseCachePose(PoseCache, Pose);
            return 0;
        }
    }

    return Pose;
}

granny_local_pose*
GStateSampleAnimation(gstate_character_instance* Instance,
                      granny_pose_cache*         PoseCache)
{
    return GStateSampleAnimationLOD(Instance, 0, PoseCache);
}

animation_spec* GSTATE
GetSpecForCharacterSlot(gstate_character_instance* Instance, animation_slot* AnimSlot)
{
    GStateCheckPtrNotNULL(Instance, return 0);
    GStateCheckPtrNotNULL(AnimSlot, return 0);

    gstate_character_info* Info = Instance->CharacterInfo;
    GStateCheckIndex(AnimSlot->AnimSlotIndex, Instance->CharacterInfo->AnimationSlotCount, return 0);
    GStateCheckIndex(Instance->AnimationSet, Instance->CharacterInfo->AnimationSetCount, return 0);

    return &Info->AnimationSets[Instance->AnimationSet]->AnimationSpecs[AnimSlot->AnimSlotIndex];
}
