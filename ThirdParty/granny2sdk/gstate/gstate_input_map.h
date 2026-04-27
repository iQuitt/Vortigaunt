#if !defined(GSTATE_INPUT_MAP_H)
#include "gstate_header_prefix.h"
// ========================================================================
// $Notice: $
// ========================================================================

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

void InputMap_CacheNumPoseInputs(node * Node, granny_int32& NumPoseInputs);
void InputMap_TakeTokenOwnership(granny_int32*& InputMap, granny_int32*& OldInputMap, granny_int32& InputMapCount);
void InputMap_ReleaseOwnedToken(granny_int32* InputMap, granny_int32 InputMapCount);
granny_int32 InputMap_GetInputIndex(granny_int32 Index, granny_int32* InputMap, granny_int32 InputMapCount);
granny_int32 InputMap_GetNthInputIndexForOutput(granny_int32 N, node * Node, granny_int32* InputMap, granny_int32 InputMapCount, granny_int32 OutputIndex);
bool InputMap_DeleteInputSet(node * Node, granny_int32 InputIndex, granny_int32*& InputMap, granny_int32& InputMapCount, granny_int32& NumPoseInputs);
void InputMap_AddInputSet(node * Node, granny_int32*& InputMap, granny_int32& InputMapCount, granny_int32& NumPoseInputs);
void InputMap_AddInputChannelToEachInputSet(node * Node, granny_int32 OutputIdx, granny_int32*& InputMap, granny_int32& InputMapCount);
void InputMap_SetNameForEachInputInInputSet(node * Node, granny_int32 OutputIdx, char const* NewEdgeName, granny_int32*& InputMap, granny_int32& InputMapCount);
void InputMap_RemoveInputChannelFromEachInputSet(node * Node, granny_int32 OutputIdx, granny_int32*& InputMap, granny_int32& InputMapCount);


END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_INPUT_MAP_H
#endif /* GSTATE_INPUT_MAP_H */
