// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_rest_pose.h $
// $DateTime: 2012/03/16 15:41:10 $
// $Change: 36794 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_REST_POSE_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

struct granny_rest_pose_specifcation;

BEGIN_GSTATE_NAMESPACE;

class rest_pose : public node
{
    typedef node parent;

public:
    virtual granny_local_pose* SamplePoseOutput(granny_int32x  OutputIdx,
                                                granny_real32 AtT,
                                                granny_real32 AllowedError,
                                                granny_pose_cache* PoseCache,
                                                granny_real32* PoseWeight);

    virtual bool GetRootMotionVectors(granny_int32x  OutputIdx,
                                      granny_real32  AtT,
                                      granny_real32  DeltaT,
                                      granny_real32* ResultTranslation,
                                      granny_real32* ResultRotation,
                                      bool           Inverse);

    DISALLOW_IO_MANIPULATION_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(rest_pose);

    DECL_SAMPLEEVENT_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();
    DECL_SAMPLEMORPH_INTERFACE();
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_REST_POSE_H
#endif /* GSTATE_REST_POSE_H */
