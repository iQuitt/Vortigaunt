// Note the lack of header guards here

// Handle this specially, since we sometimes have base-class defaults
#ifdef TAKE_ACTION_NODE
  TAKE_ACTION_NODE(node)
#else
  TAKE_ACTION(node)
#endif

TAKE_ACTION(additive_blend)
TAKE_ACTION(aimat_ik)
TAKE_ACTION(position_ik)
TAKE_ACTION(anim_source)
TAKE_ACTION(blend)
TAKE_ACTION(blend_graph)
TAKE_ACTION(clamp_value)
TAKE_ACTION(constant_velocity)
TAKE_ACTION(event_mixer)
TAKE_ACTION(event_renamer)
TAKE_ACTION(event_source)
TAKE_ACTION(linear_equation)
TAKE_ACTION(linear_remap)
TAKE_ACTION(custom_remap)
TAKE_ACTION(lod_protect)
TAKE_ACTION(mask_invert)
TAKE_ACTION(mask_source)
TAKE_ACTION(mask_union)
TAKE_ACTION(masked_combine)
TAKE_ACTION(mirror)
TAKE_ACTION(nway_blend)
TAKE_ACTION(parameters)
TAKE_ACTION(pose_storage)
TAKE_ACTION(orient_towards)
TAKE_ACTION(orient_match)
TAKE_ACTION(rest_pose)
TAKE_ACTION(selection)
TAKE_ACTION(state_machine)
TAKE_ACTION(timeselect)
TAKE_ACTION(timeshift)
TAKE_ACTION(timeclamp)
TAKE_ACTION(twobone_ik)

#include "client_node_type_list.h"

