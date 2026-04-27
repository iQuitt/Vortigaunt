// =====================================================================
//  walking_model.h: data structures/types for supporting our HowToWalk
//                   style examples.
// =====================================================================
#define _USE_MATH_DEFINES
#include "walking_model.h"
#include "crowd_scene.h"
#include "scene.h"
#include <string.h>
#include <stdlib.h>

#include <math.h>
#define DEG_TO_RAD(x) float((x) * M_PI / 180.0f)

// duping some internal functionality here...
namespace
{
    EControlAnimType GetRandomWalkingAnim()
    {
        // Used to mask off anim types not supported by this demo, no
        // longer really necessary
        static const EControlAnimType AcceptableTypes[] = {
            ControlAnim_WalkSlow,
            ControlAnim_WalkMed,
            ControlAnim_WalkFast,
            ControlAnim_RunSlow,
            ControlAnim_RunMed,
            ControlAnim_RunFast
        };
        static const int NumAcceptable = sizeof(AcceptableTypes) / sizeof(AcceptableTypes[0]);

        // Using a mod rand() is technically a pretty bad thing to do, but what
        //  the hey, we're not making a slot machine here...
        return AcceptableTypes[rand() % NumAcceptable];
    }

    void GetSceneLodUtilityVariables(granny_real32  CurrentSceneLod,
                                     granny_real32* AllowedErrorEnd,
                                     granny_real32* AllowedErrorScaler )
    {
        *AllowedErrorEnd    = CurrentSceneLod;
        *AllowedErrorScaler = 0.f;

        granny_real32 factor = GrannyGetGlobalLODFadingFactor();

        if ( CurrentSceneLod > 0.0f && factor < 1.0f)
        {
            *AllowedErrorEnd    = CurrentSceneLod * factor;
            *AllowedErrorScaler = 1.0f / ( CurrentSceneLod - *AllowedErrorEnd );
        }
    }

    void VectorCrossProduct3(granny_real32 *Dest, granny_real32 const *Operand1,
                             granny_real32 const *Operand2)
    {
        granny_real32 const X = Operand1[1]*Operand2[2] - Operand1[2]*Operand2[1];
        granny_real32 const Y = Operand1[2]*Operand2[0] - Operand1[0]*Operand2[2];
        granny_real32 const Z = Operand1[0]*Operand2[1] - Operand1[1]*Operand2[0];

        Dest[0] = X;
        Dest[1] = Y;
        Dest[2] = Z;
    }

    granny_real32 VectorLength3(granny_real32* Vec)
    {
        return sqrtf((Vec[0] * Vec[0]) +
                     (Vec[1] * Vec[1]) +
                     (Vec[2] * Vec[2]));
    }

    inline granny_real32 NormalizeOrZero3(granny_real32 *Dest)
    {
        granny_real32 Length = VectorLength3(Dest);
        if(Length > 0.00001f)
        {
            for (int i = 0; i < 3; i++) Dest[i] *= 1.0f / Length;
        }
        else
        {
            for (int i = 0; i < 3; i++) Dest[i] = 0.0f;
        }

        return(Length);
    }

    // Find the first active animation that has any effect.
    granny_animation_binding *
    GetFirstActiveAnimationBinding ( granny_model_instance *ModelInstance )
    {
        granny_animation_binding *FirstActiveAnimBinding = NULL;
        if ( ModelInstance )
        {
            {for(granny_model_control_binding *Binding = GrannyModelControlsBegin(ModelInstance);
                 Binding != GrannyModelControlsEnd(ModelInstance);
                 Binding = GrannyModelControlsNext(Binding))
            {
                granny_control *Control = GrannyGetControlFromBinding(Binding);
                if ( GrannyGetControlEffectiveWeight ( Control ) > 0.001f )
                {
                    FirstActiveAnimBinding = GrannyGetAnimationBindingFromControlBinding ( Binding );
                    // NOTE! FirstActiveAnimBinding can still be NULL here!
                    break;
                }
            }}
        }
        return FirstActiveAnimBinding;
    }

} // namespace {}



// =====================================================================
WalkingModel::WalkingModel()
{
    // Null everything out, we depend on clients to init our pointers...
    SourceModel = NULL;
    memset(SourceAnimations, 0, sizeof(SourceAnimations));
}

bool WalkingModel::IsValidWalkingModel() const
{
    if (SourceModel == NULL)
        return false;

    for (int i = 0; i < ControlAnim_Count; i++)
    {
        if (SourceAnimations[i] == NULL)
            return false;

        // TODO: extra checks...
    }

    return true;
}


// =====================================================================
WalkingInstance::WalkingInstance(WalkingModel*      TheWalkingModel,
                                 float              ArenaRadius)
  : Model(TheWalkingModel),
    GrannyModelInstance(NULL),
    InternalClock(0.0f),
    ArenaRadius(ArenaRadius),
    SkeletalLOD(false),
    AnimationLOD(false),
    AllowedError(0.0f),
    AnimLODBias(1.0f)
{
    Size = 1.0f + ((rand() / float(RAND_MAX-1)) * 0.3f - 0.15f);

	ASSERT(TheWalkingModel);
    ASSERT(TheWalkingModel->IsValidWalkingModel());

    granny_model* TheGrannyModel = TheWalkingModel->SourceModel->GrannyModel;

    // Create the granny_model_instance
    GrannyModelInstance = GrannyInstantiateModel(TheGrannyModel);

    // Play an animation on the node.
    granny_control *Control = GrannyPlayControlledAnimation(0.0f,
                                                            Model->SourceAnimations[GetRandomWalkingAnim()]->GetGrannyAnimation(),
                                                            GrannyModelInstance );
    GrannySetControlLoopCount(Control, 0);
    GrannyFreeControlOnceUnused ( Control );

    // Set the default position from the exporting initial placement
    {
        GrannyBuildCompositeTransform4x4 ( &TheGrannyModel->InitialPlacement, (float*)&OffsetOrientation );
    }

    // Set up the initial orientation
    ArenaInit(ArenaRadius);

    // Create a cached world pose for this instance.  Note that we would normally use the
    //  world pose with the model to save space, since only one render can be in flight
    //  at any given time, but this allows us to completely separate animation from
    //  rendering for timing purposes...
    GrannyLocalPose = GrannyNewLocalPose(TheGrannyModel->Skeleton->BoneCount);
    GrannyWorldPose = GrannyNewWorldPose(TheGrannyModel->Skeleton->BoneCount);
}

WalkingInstance::~WalkingInstance()
{
    GrannyFreeLocalPose(GrannyLocalPose);
    GrannyFreeWorldPose(GrannyWorldPose);
    GrannyFreeModelInstance(GrannyModelInstance);

    GrannyLocalPose = NULL;
    GrannyWorldPose = NULL;
    GrannyModelInstance = NULL;
    Model = NULL;
}


void WalkingInstance::Render( LPDIRECT3DDEVICE9 d3ddev )
{
    // And render the model with this instance data.
    Model->SourceModel->Render(d3ddev, GrannyWorldPose);
}


struct LineVertex
{
    granny_triple xyz;
    DWORD         diffuse;
};
#define LINE_FVF (D3DFVF_DIFFUSE | D3DFVF_XYZ)

void WalkingInstance::RenderSkeleton( LPDIRECT3DDEVICE9 d3ddev )
{
    d3ddev->SetFVF(LINE_FVF);

    granny_real32 CurrentSceneLod = AllowedError;
    granny_real32 LodAllowedErrorEnd = 0.0f;
    granny_real32 LodAllowedErrorScaler = 0.0f;
    GetSceneLodUtilityVariables(CurrentSceneLod, &LodAllowedErrorEnd, &LodAllowedErrorScaler);

    granny_skeleton *Skeleton = Model->SourceModel->GrannyModel->Skeleton;
    {for(int BoneIndex = 0;
         BoneIndex < Skeleton->BoneCount;
         ++BoneIndex)
    {
        granny_triple BoneColor;
        {
            granny_real32 BlendFactor;
            if (Skeleton->LODType == GrannyNoSkeletonLOD || !SkeletalLOD)
            {
                BlendFactor = 1.0f;
            }
            else
            {
                granny_real32 BoneError = Skeleton->Bones[BoneIndex].LODError;
                if (BoneError >= CurrentSceneLod)
                    BlendFactor = 1.0f;
                else if (BoneError > LodAllowedErrorEnd)
                    BlendFactor = (Skeleton->Bones[BoneIndex].LODError - CurrentSceneLod) * LodAllowedErrorScaler;
                else
                    BlendFactor = 0.0f;
            }

            float AnimBlendFactor = 1.0f;
            if (AnimationLOD)
            {
                // Find the first active animation.
                granny_animation_binding *FirstActiveAnimBinding = GetFirstActiveAnimationBinding ( GrannyModelInstance );

                if (FirstActiveAnimBinding)
                {
                    if ( BoneIndex < FirstActiveAnimBinding->TrackBindingCount )
                    {
                        granny_real32 TrackError = FirstActiveAnimBinding->TrackBindings[BoneIndex].LODError;

                        if (TrackError < AllowedError * AnimLODBias)
                            AnimBlendFactor = 0.0f;
                    }
                }
            }



            // Blend from white to red
            BoneColor[0] = BlendFactor;
            BoneColor[1] = 1.0f;
            BoneColor[2] = AnimBlendFactor;
        }


        LineVertex lv[2];
        lv[0].diffuse = D3DXCOLOR(BoneColor[0], BoneColor[1], BoneColor[2], 1.0f);
        lv[1].diffuse = lv[0].diffuse;

        granny_real32 *World = GrannyGetWorldPose4x4(GrannyWorldPose, BoneIndex);
        memcpy(lv[0].xyz, &World[12], sizeof(granny_triple));

        int ParentIndex = Skeleton->Bones[BoneIndex].ParentIndex;
        if(ParentIndex != GrannyNoParentBone)
        {
            granny_real32 *ParentWorld = GrannyGetWorldPose4x4(GrannyWorldPose, ParentIndex);

            memcpy(lv[1].xyz, &ParentWorld[12], sizeof(granny_triple));
            d3ddev->DrawPrimitiveUP(D3DPT_LINELIST, 1, lv, sizeof(lv[0]));
        }
    }}
}

void WalkingInstance::AdvanceClock(float Elapsed)
{
    if (Elapsed <= 0.0f)
        return;
    InternalClock += Elapsed;

    // Sample and blend the poses.
    GrannySetModelClock(GrannyModelInstance, InternalClock);

    // Good a time as any to free up any animation controls that have finished their existence.
    GrannyFreeCompletedModelControls(GrannyModelInstance);
}

void WalkingInstance::DoThreadWork(float Elapsed)
{
    granny_model* GrannyModel = Model->SourceModel->GrannyModel;
    const int NumBones        = GrannyModel->Skeleton->BoneCount;

    UpdateWalk(Elapsed);

    // If lod is enabled, compute those parameters now...
    int SampledBones = NumBones;
    if (SkeletalLOD)
        SampledBones = GrannyGetBoneCountForLOD(GrannyModel->Skeleton, AllowedError);

    if (AnimationLOD)
    {
        GrannySampleModelAnimationsLOD(GrannyModelInstance, 0, SampledBones, GrannyLocalPose, AllowedError * AnimLODBias);
    }
    else
    {
        GrannySampleModelAnimations(GrannyModelInstance, 0, SampledBones, GrannyLocalPose);
    }

    // Compute the final world position...
    D3DXMATRIX ResultOrn;
    D3DXMatrixMultiply ( &ResultOrn, &OffsetOrientation, &Orientation );
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            ResultOrn(x, y) *= Size;

    // Now move the bones into world space (so they are no longer hierarchical).
    if (SkeletalLOD)
    {
        GrannyBuildWorldPoseLOD ( GrannyModel->Skeleton,
                                  0, NumBones,
                                  0, SampledBones,
                                  GrannyLocalPose,
                                  (float*)&ResultOrn,
                                  GrannyWorldPose );
    }
    else
    {
        GrannyBuildWorldPose ( GrannyModel->Skeleton,
                               0, NumBones,
                               GrannyLocalPose,
                               (float*)&ResultOrn,
                               GrannyWorldPose );
    }
}

void WalkingInstance::ArenaInit(float TheArenaRadius)
{
    // Store the size of the playground we can walk in...
    ArenaRadius = TheArenaRadius;

    // Stupid circle distribution
    float x, z;
    do {
        x = ((float(rand()) / RAND_MAX) * ArenaRadius * 2) - ArenaRadius;
        z = ((float(rand()) / RAND_MAX) * ArenaRadius * 2) - ArenaRadius;
    } while ((x * x) + (z * z) > ArenaRadius * ArenaRadius);

    // Set up the initial orientation
    {
        granny_transform xform;
        GrannyMakeIdentity(&xform);
        xform.Flags |= GrannyHasPosition;
        xform.Position[0] = x;
        xform.Position[1] = 0;
        xform.Position[2] = z;

        CurrentAngle = (float(rand()) / RAND_MAX) * float(M_PI * 2);
        TargetAngle = CurrentAngle;

        // Need to add a GrannyQuaternionEqualsAxisAngle an exported
        // function.  This just makes the rotation random around the
        // y-axis, which is up in our space.
        xform.Flags |= GrannyHasOrientation;
        xform.Orientation[0] = 0;
        xform.Orientation[1] = sin(CurrentAngle / 2.0f);
        xform.Orientation[2] = 0;
        xform.Orientation[3] = cos(CurrentAngle / 2.0f);

        GrannyBuildCompositeTransform4x4 ( &xform, (float*)&Orientation );
    }
}

bool WalkingInstance::OutOfArena(const float x, const float z,
                                 const float slop /* = 0.0f */)
{
    float RadiusWithSlop = ArenaRadius + slop;

    return (x*x + z*z >= RadiusWithSlop*RadiusWithSlop);
}


// TODO: Unlame-ify
void WalkingInstance::UpdateWalk(float Elapsed)
{
    // xpos = Orientation._41,
    // zpos = Orientation._43,

    const bool OutBefore = OutOfArena(Orientation._41, Orientation._43);
    GrannyUpdateModelMatrix(GrannyModelInstance, Elapsed, (float*)&Orientation, (float*)&Orientation, false );
    const bool OutAfter  = OutOfArena(Orientation._41, Orientation._43);

    if (OutAfter)
    {
        if (!OutBefore)
        {
            // Turn around...
            float randOffset = ((rand() / float(RAND_MAX)) * 10.0f) - 5.0f;
            TargetAngle = CurrentAngle - DEG_TO_RAD(180 + randOffset);
            if (TargetAngle < 0.0f)
            {
                TargetAngle  += float(M_PI * 2.0);
                CurrentAngle += float(M_PI * 2.0);
            }
        }
        else
        {
            // See if we're /really/ outside the arena, and turn towards the center.
            //  Note that we might set the target multiple times before we make it back in, but
            //  who cares?
            const float really_out = 1.0f;
            if (OutOfArena(Orientation._41, Orientation._43, really_out))
                TargetAngle = atan2(-Orientation._41, -Orientation._43);
        }
    }

    if (TargetAngle != CurrentAngle)
    {
        const float MaxAnglePerSec = float(M_PI*2);
        float MaxThisUpdate = Elapsed * MaxAnglePerSec;
        float Diff = TargetAngle - CurrentAngle;
        if (fabsf(Diff) > MaxThisUpdate)
        {
            if (Diff < 0.0f) CurrentAngle -= MaxThisUpdate;
            else             CurrentAngle += MaxThisUpdate;
        }
        else
        {
            CurrentAngle = TargetAngle;
        }

        granny_transform xform;
        GrannyMakeIdentity(&xform);
        xform.Flags |= GrannyHasPosition | GrannyHasOrientation;
        xform.Position[0] = Orientation._41;
        xform.Position[1] = 0;
        xform.Position[2] = Orientation._43;
        xform.Orientation[0] = 0;
        xform.Orientation[1] = sin(CurrentAngle / 2.0f);
        xform.Orientation[2] = 0;
        xform.Orientation[3] = cos(CurrentAngle / 2.0f);
        GrannyBuildCompositeTransform4x4 ( &xform, (float*)&Orientation );
    }
}

void WalkingInstance::GetCurrentPosition(granny_triple pOutput) const
{
    pOutput[0] = Orientation._41;
    pOutput[1] = 0.0f;
    pOutput[2] = Orientation._43;
}

void WalkingInstance::SetLODState(bool Skeleton, bool Animation, float Error, float Bias)
{
    SkeletalLOD  = Skeleton;
    AnimationLOD = Animation;
    AllowedError = Error;
    AnimLODBias = Bias;
}

int WalkingInstance::TotalBones() const
{
    return Model->SourceModel->GrannyModel->Skeleton->BoneCount;
}

int WalkingInstance::SavedBonesLastFrame() const
{
    if (!SkeletalLOD && !AnimationLOD)
        return 0;

    int SampledBones = Model->SourceModel->GrannyModel->Skeleton->BoneCount;
    if (SkeletalLOD)
    {
        SampledBones = GrannyGetBoneCountForLOD(Model->SourceModel->GrannyModel->Skeleton, AllowedError);
    }
    int SavedFromSkeletal = Model->SourceModel->GrannyModel->Skeleton->BoneCount - SampledBones;

    int SavedFromAnimLOD = 0;
    if (AnimationLOD)
    {
        // Find the first active animation.
        granny_animation_binding *FirstActiveAnimBinding = GetFirstActiveAnimationBinding ( GrannyModelInstance );

        if (FirstActiveAnimBinding)
        {
            for (int i = 0; i < SampledBones; i++)
            {
                if ( i < FirstActiveAnimBinding->TrackBindingCount )
                {
                    granny_real32 TrackError = FirstActiveAnimBinding->TrackBindings[i].LODError;

                    if (TrackError < AllowedError * AnimLODBias)
                        SavedFromAnimLOD++;
                }
            }
        }
    }

    return SavedFromSkeletal + SavedFromAnimLOD;
}
