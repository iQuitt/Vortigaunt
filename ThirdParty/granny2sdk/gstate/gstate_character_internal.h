#if !GSTATE_INTERNAL_HEADER
#error "Should not be included from user code"
#endif

// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_CHARACTER_INTERNAL_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_BASE_H
#include "gstate_base.h"
#endif

struct gstate_character_instance;

BEGIN_GSTATE_NAMESPACE;

class token_context;

struct source_file_ref
{
    char*             SourceFilename;
    granny_int32      ExpectedAnimCount;
    granny_uint32     AnimCRC;

    granny_file_info* SourceInfo;
};

struct animation_spec
{
    source_file_ref* SourceReference;
    granny_int32     AnimationIndex;
    char*            ExpectedName;
};

struct animation_slot
{
    char*        AnimSlotName;
    granny_int32 AnimSlotIndex;  // This is redundant, but we need to refer to animation_slots by pointer...
};

struct animation_set
{
    char* Name;

    granny_int32      SourceFileReferenceCount;
    source_file_ref** SourceFileReferences;

    // AnimationSpecCount == AnimationSlotCount in the containing info...
    granny_int32    AnimationSpecCount;
    animation_spec* AnimationSpecs;
};


extern granny_data_type_definition AnimationSetType[];
extern granny_data_type_definition AnimationSlotType[];

// Ok, why am I hiding this here?  Because it's not for you.  So there.  These are for the
// editor.  Seriously.  Don't call these in your game.  :)
class state_machine;
gstate_character_instance* InstantiateEditCharacter(gstate_character_info* Info,
                                                    granny_int32x AnimationSet,
                                                    granny_model* RetargetSourceModel,
                                                    granny_model* Model,
                                                    granny_mesh** Meshes,
                                                    granny_int32  MeshCount);
void UnbindEditCharacter(gstate_character_instance*);
bool ReplaceStateMachine(gstate_character_instance*, state_machine* NewMachine);
void SetCurrentAnimationSet(gstate_character_instance*, granny_int32x Set);

granny_uint32 ComputeAnimCRC(granny_file_info*);

bool ValidateReference(source_file_ref*  Ref, granny_file_info* NewInfo);

END_GSTATE_NAMESPACE;

struct gstate_character_info
{
    // May be zero, which indicates that we can't properly presize the token contexts for
    // instances
    granny_int32 NumUniqueTokenized;

    // The root is always a state_machine, but it does go through the token system
    granny_variant StateMachine;

    granny_int32            AnimationSlotCount;
    GSTATE animation_slot** AnimationSlots;

    granny_int32           AnimationSetCount;
    GSTATE animation_set** AnimationSets;

    // Used by the editor to auto-load the last used model if possible.
    char*        ModelNameHint;
    granny_int32 ModelIndexHint;

    char*        RetargetSourceModelNameHint;
    granny_int32 RetargetSourceModelIndexHint;

    // Extra data for the editor's use (we store controller schemes, etc. here)
    granny_variant EditorData;

    // User extensible fields
    granny_variant ExtendedData;
};
GStateCompileAssert(sizeof(gstate_character_info) == (sizeof(granny_int32) +
                                                      sizeof(granny_variant) +
                                                      sizeof(granny_int32) + sizeof(GSTATE animation_slot**) +
                                                      sizeof(granny_int32) + sizeof(GSTATE animation_set**) +
                                                      sizeof(char*) +
                                                      sizeof(granny_int32) +
                                                      sizeof(char*) +
                                                      sizeof(granny_int32) +
                                                      sizeof(granny_variant) +
                                                      sizeof(granny_variant)));

struct gstate_named_mat4x4
{
    const char * Name;
    granny_real32 Mat4x4[16];
};

struct gstate_character_instance
{
    granny_model* TargetModel;
    granny_model* SourceModel;
    granny_retargeter* Retargeter;

    GSTATE token_context*  TokenContext;
    gstate_character_info* CharacterInfo;

    // 0 by default...
    granny_int32 AnimationSet;

    // Reversed from the normal order, we don't persist this and it keeps it aligned in
    // 64-bit mode
    granny_int32  CharacterMeshCount;
    granny_mesh** CharacterMeshes;

    GSTATE state_machine* StateMachine;

    gstate_named_mat4x4* Mat4x4Cache;
    granny_int32  Mat4x4CacheSize;
    granny_int32  Mat4x4CacheCapacity;

    granny_real32 CurrentTime; // Starts at 0 (todo: random?)
    granny_real32 DeltaT;      // The parameter from the last AdvanceT call

    // Pose cache for persistent poses used by the StateMachine
    granny_pose_cache * PoseCache;
};
GStateCompileAssert(sizeof(gstate_character_instance) == (sizeof(granny_model*) +
                                                          sizeof(granny_model*) +
                                                          sizeof(granny_retargeter*) +
                                                          sizeof(GSTATE token_context*) +
                                                          sizeof(gstate_character_info*) +
                                                          sizeof(granny_int32) +
                                                          sizeof(granny_int32) +
                                                          sizeof(granny_mesh**) +
                                                          sizeof(GSTATE state_machine*) +
                                                          sizeof(gstate_named_mat4x4*) + 
                                                          sizeof(granny_int32) +
                                                          sizeof(granny_int32) +
                                                          sizeof(granny_real32) +
                                                          sizeof(granny_real32) +
                                                          sizeof(granny_pose_cache*)));

#include "gstate_header_postfix.h"
#define GSTATE_CHARACTER_INTERNAL_H
#endif /* GSTATE_CHARACTER_INTERNAL_H */

