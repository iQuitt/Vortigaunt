// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_NWAY_BLEND_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class nway_blend : public node
{
    typedef node parent;

public:
    virtual granny_local_pose* SamplePoseOutput(granny_int32x      OutputIdx,
                                                granny_real32      AtT,
                                                granny_real32      AllowedError,
                                                granny_pose_cache* PoseCache,
                                                granny_real32*     PoseWeight);
    virtual bool GetRootMotionVectors(granny_int32x  OutputIdx,
                                      granny_real32  AtT,
                                      granny_real32  DeltaT,
                                      granny_real32* Translation,
                                      granny_real32* Rotation,
                                      bool Inverse);


    DECL_IO_MANIPULATION_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(nway_blend);
    DECL_SAMPLEMORPH_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();
    DECL_SAMPLEEVENT_INTERFACE();

private:
    bool ComputeBlend(granny_real32  AtT,
                      granny_real32& Factor,
                      granny_int32x& IndexFrom,
                      granny_int32x& IndexTo);
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_NWAY_BLEND_H
#endif /* GSTATE_NWAY_BLEND_H */
