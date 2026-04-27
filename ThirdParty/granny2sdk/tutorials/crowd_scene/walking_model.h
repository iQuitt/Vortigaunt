#pragma once
// =====================================================================
//  walking_model.h: data structures/types for supporting our HowToWalk
//                   style examples.
// =====================================================================
#include <d3dx9.h>
#include "granny.h"

class Scene;
class AnimatedModel;
class Animation;

enum EControlAnimType
{
    ControlAnim_WalkSlow = 0,
    ControlAnim_WalkMed,
    ControlAnim_WalkFast,

    ControlAnim_RunSlow,
    ControlAnim_RunMed,
    ControlAnim_RunFast,

    // Constants, not actually used for control selection
    ControlAnim_Count,
    ControlAnim_ReferenceSpeed = ControlAnim_WalkMed
};


// Encapsulates the source data for a walking character.  This
//  is then used to generate a WalkingInstance.  Note that this
//  structure doesn't own any of these pointers, it's used to
//  aggregate the pointers, basically.
struct WalkingModel
{
    AnimatedModel* SourceModel;
    Animation*     SourceAnimations[ControlAnim_Count];

    WalkingModel();
    bool IsValidWalkingModel() const;
};


class WalkingInstance
{
public:
    WalkingInstance(WalkingModel* TheWalkingModel, float ArenaRadius);
    ~WalkingInstance();

    void Render ( LPDIRECT3DDEVICE9 d3ddev );
    void RenderSkeleton ( LPDIRECT3DDEVICE9 d3ddev );

    void AdvanceClock(float Elapsed);
    void DoThreadWork(float Elapsed);

    void SetLODState(bool Skeletal, bool Animation, float Error, float AnimLODBias);
    void GetCurrentPosition(granny_triple) const;

    int TotalBones() const;
    int SavedBonesLastFrame() const;

    // Positions the model in a random location with a random walk
    // direction...
    void ArenaInit(float ArenaRadius);

private:
    void UpdateWalk(float Elapsed); // updates our walk parameters...
    bool OutOfArena(const float x, const float z, const float slop = 0.0f);

    WalkingModel* Model;

    // Instance data, walking state
    float      CurrentAngle;
    float      TargetAngle;

    float      Size;

    D3DXMATRIX Orientation;
    D3DXMATRIX OffsetOrientation;

    // Granny's information
    granny_model_instance* GrannyModelInstance;

    // Cached world position, clock, and playground parameters
    float              InternalClock;
    float              ArenaRadius;
    granny_local_pose* GrannyLocalPose;
    granny_world_pose* GrannyWorldPose;

    // Cached lod parameters.
    bool  SkeletalLOD;
    bool  AnimationLOD;
    float AllowedError;
    float AnimLODBias;

    // Information for doing simple walk tracking...
};

