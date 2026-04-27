// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_CHARACTER_INSTANCE_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_BASE_H
#include "gstate_base.h"
#endif

struct gstate_character_info;

EXPGROUP(CharacterInstanceGroup)

EXPTYPE struct gstate_character_instance;
/*
  An unique instance of a character type.

  This is the structure that holds instance-specific data.  (Current state, parameter
  values, etc.)  Immutable data for a character is contained in the
  $gstate_character_info.  Each $gstate_character_instance represents the binding of a
  specific granny_model to a specific $gstate_character_info, as well as the changes that
  have occurred since the instantiation.
*/

BEGIN_GSTATE_NAMESPACE;

EXPTYPE class state_machine;
/*
  Todo: Document me.
*/

struct animation_spec;
struct animation_slot;

granny_model* GetTargetModelForCharacter(gstate_character_instance* Instance);
granny_model* GetSourceModelForCharacter(gstate_character_instance*);
granny_model* GetModelForCharacterAutomatic(gstate_character_instance*);
granny_retargeter* GetCharacterRetargeter(gstate_character_instance*);
bool  GrannyGetRetargetAnimationSources(gstate_character_instance*);

animation_spec* GetSpecForCharacterSlot(gstate_character_instance*, animation_slot* AnimSlot);

END_GSTATE_NAMESPACE;

EXPAPI gstate_character_info* GStateGetInfoForCharacter(EXPIN gstate_character_instance* Instance);
/*
  Returns the $gstate_character_info used to create this instance

  See $GStateInstantiateCharacter.
*/


EXPAPI gstate_character_instance* GStateInstantiateCharacter(EXPIN gstate_character_info* Info,
                                                             EXPIN granny_real32 CurrentTime,
                                                             EXPIN granny_int32x AnimationSet,
                                                             EXPIN granny_model* Model);
/*
  Creates a unique instance of the specified character

  $:Info The state machine specification for the character
  $:CurrentTime The base clock for the character.
  $:AnimationSet The variant to intialize.  Must be [ 0, $GStateGetNumAnimationSets(Info) )
  $:Model The granny_model to bind to the animation set
  $:return A valid $gstate_character_instance, or NULL on failure (check the logs for details)

  Note that if you need to retarget the $gstate_character_info specification to a
  granny_model other than the one it was authored for, you should use
  $GStateInstantiateRetargetedCharacter.

  CurrentTime may be whatever you like, but it is recommended that you use the global
  clock for your application so that values for $GStateInstanceTime will line up between
  instances.

  Note that this version of the function does not bind morph targets, you must use
  $GStateInstantiateCharacterDetailed for that purpose.  Morph edges will *not* sample
  correctly unless bound at character startup.
*/

EXPAPI gstate_character_instance* GStateInstantiateRetargetedCharacter(EXPIN gstate_character_info* Info,
                                                                       EXPIN granny_real32 CurrentTime,
                                                                       EXPIN granny_int32x AnimationSet,
                                                                       EXPIN granny_model* SourceModel,
                                                                       EXPIN granny_model* TargetModel);
/*
  Creates an instance of the specified character, retargeted to a different granny_model

  $:Info The state machine specification for the character
  $:CurrentTime The base clock for the character.  It is recommended that this be the global clock for your application.
  $:AnimationSet The variant to intialize.  Must be [ 0, $GStateGetNumAnimationSets(Info) )
  $:SourceModel The granny_model to bind to the animation set
  $:TargetModel The granny_model to bind to the animation set
  $:return A valid $gstate_character_instance, or NULL on failure (check the logs for details)

  If the model you would like to render is different than the model that the animation
  graph was created for, you must use this function to output valid pose information for
  the character.  Any poses returned from $GStateSampleAnimation or
  $GStateSampleAnimationLOD will be in the "space" of the target model, i.e, it will be
  sampled in the bone ordering and layout of the SourceModel, but return
  granny_local_poses that match TargetModel's layout.

  Note that this version of the function does not bind morph targets, you must use
  $GStateInstantiateCharacterDetailed for that purpose.  Morph edges will *not* sample
  correctly unless bound at character startup.
*/

EXPAPI gstate_character_instance* GStateInstantiateCharacterDetailed(EXPIN gstate_character_info* Info,
                                                                     EXPIN granny_real32 CurrentTime,
                                                                     EXPIN granny_int32x AnimationSet,
                                                                     EXPIN granny_model* SourceModel,
                                                                     EXPIN granny_model* TargetModel,
                                                                     EXPIN granny_mesh** CharacterMeshes,
                                                                     EXPIN granny_int32x CharacterMeshCount);

EXPAPI void GStateAllocMat4x4Cache(EXPIN gstate_character_instance* Instance, int MaxCacheEntries);
/*
  Allocates space for a cache of matrices, indexed by name.

  $:Instance The state machine specification for the character
  $:MaxCacheEntries How many matrix entries to reserve space for in the cache

*/

EXPAPI void GStateFreeMat4x4Cache(EXPIN gstate_character_instance* Instance);
/*
  Deallocates the cache

  $:Instance The state machine specification for the character

*/

EXPAPI bool GStateCacheMat4x4(EXPIN gstate_character_instance* Instance, EXPIN const char * Name, granny_real32 const * Pose4x4);
/*
  Caches a Mat4x4 by name. 

  $:Instance The state machine specification for the character
  $:Name The name
  $:Mat4x4 The pose

*/

EXPAPI bool GStateGetCachedMat4x4(EXPIN gstate_character_instance* Instance, EXPIN const char * Name, granny_real32 * Pose4x4);
/*
  Gets the cached Mat4x4 for Name. Returns false if it was not found.

  $:Instance The state machine specification for the character
  $:Name Name of the Mat4x4 to look up
  $:Mat4x4 Space to put the pose in, if found. 

*/

EXPAPI bool GStateGetCachedPosition(gstate_character_instance* Instance, const char * Name, granny_real32 * Position3);
/*
  Gets the cached local space position of a joint (returns false if it was not found)

  $:Instance The state machine specification for the character
  $:Name Name of the joint to look up
  $:Position3 Space to put the position in, if found. 

*/


EXPAPI void GStateClearMat4x4Cache(EXPIN gstate_character_instance* Instance);
/*
  Clears the joint cache for the next frame

  $:Instance The state machine specification for the character

*/


EXPAPI void GStateFreeCharacterInstance(EXPFREE gstate_character_instance* Instance);
/*
  Frees the memory for a character

  $:Instance The character instance to free.  This pointer is invalid after calling this function.
*/

using GSTATE state_machine;
EXPAPI state_machine* GStateGetStateMachine(EXPIN gstate_character_instance* Instance);
/*
  Returns the root level $state_machine for Instance

  $:Instance The character to query
  $:return NULL on failure, or invalid index (see below).
*/

EXPAPI void GStateAdvanceTime(EXPIN gstate_character_instance* Instance,
                              EXPIN granny_real32              DeltaT);
/*
  Advances time by DeltaT seconds

  $:Instance The character to tick
  $:DeltaT The time increment.

  Note that it is an error to pass a DeltaT parameter that is less than 0.  Time must
  always move forward.  (DeltaT == 0.0 is perfectly OK, however.)
*/

EXPAPI granny_real32 GStateInstanceTime(EXPIN gstate_character_instance* Instance);
/*
  Returns the time for the $gstate_character_instance.

  $:Instance The character to query
  $:return Current time

  To facilitate queries against the scalar and event tracks in the root state_machine,
  returns the current time at the Instance level.  Provided as a convenience so the global
  time + Instance offset doesn't have to be tracked in addition to the DeltaT pumps coming
  from $GStateAdvanceTime.
*/

EXPAPI granny_real32 GStateInstanceLastDeltaT(EXPIN gstate_character_instance* Instance);
/*
  Returns the last value passed to $GStateAdvanceTime

  $:Instance The character to query
  $:return Last DeltaT value

  To facilitate queries against the scalar and event tracks in the root state_machine,
  returns the last value passed to $GStateAdvanceTime at the Instance level.  Provided as
  a convenience so the DeltaT doesn't have to be tracked across the call graph.
*/


EXPAPI granny_local_pose* GStateSampleAnimation(EXPIN gstate_character_instance* Instance,
                                                EXPIN granny_pose_cache* PoseCache);
/*
  Returns the current animated pose for a character.

  $:Instance The character graph to sample
  $:PoseCache A granny_pose_cache for temporary results
  $:return The current pose.  Note that you must free this with GrannyReleaseCachePose when you are done with the result.

  In $state_machine terms, this samples the 0th external output of the $state_machine at
  the top-level.  Sampling cascades to the current state or transition, and out from
  there.  Note that in the present version of the library, results are not cached, so
  every time you call this, a new granny_local_pose will be computed.  Hang onto the
  result if you need to use it multiple times.
*/  

EXPAPI granny_local_pose* GStateSampleAnimationLOD(EXPIN gstate_character_instance* Instance,
                                                   EXPIN granny_real32              AllowedError,
                                                   EXPIN granny_pose_cache*         PoseCache);
/*
  Returns the current animated pose for a character, with Level of Detail sampling

  $:Instance The character graph to sample
  $:AllowedError The acceptable screen-space error.
  $:PoseCache A granny_pose_cache for temporary results
  $:return The current pose.  Note that you must free this with GrannyReleaseCachePose when you are done with the result.

  Mostly the same as $GStateSampleAnimation, but uses Granny's built-in animation LOD to
  speed up the sampling process.  For more details on animation LOD, please see the
  "Animation Level Of Detail" section in the Granny documentation.
*/  

EXPAPI bool GStateUpdateModelMatrix(EXPIN  gstate_character_instance*  Instance,
                                    EXPIN  granny_real32 const* ModelMatrix4x4,
                                    EXPOUT granny_real32*       DestMatrix4x4);
/*
  Handles motion extracted character movement

  $:Instance The character graph to sample
  $:ModelMatrix4x4 The starting root offset matrix
  $:DestMatrix4x4 The resulting matrix after root motion was applied.  (This may be the same as ModelMatrix4x4)
  $:return true for successful motion extraction, false if the function failed for some reason.

  Please see the "Understanding Model World-space Movement" in the Granny documentation
  for more information on motion extraction.  This function is the GState equivalent of
  GrannyUpdateModelMatrix.
*/  

#include "gstate_header_postfix.h"
#define GSTATE_CHARACTER_INSTANCE_H
#endif /* GSTATE_CHARACTER_INSTANCE_H */
