// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_ADDITIVE_BLEND_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class additive_blend : public node
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
                                      granny_real32* ResultTranslation,
                                      granny_real32* ResultRotation,
                                      bool           Inverse);

    // Todo: duration?
    // todo: DidLoopOccur only for eIntoEdge?
    
protected:
    DECL_CONCRETE_NODE_TOKEN(additive_blend);

    DECL_SAMPLEMORPH_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();
    DECL_SAMPLEEVENT_INTERFACE();

    virtual bool AllowSyncInputsWithParentOutputs() const { return true; }
    virtual bool AllowSyncOutputsWithParentOutputs() const { return true; }

    virtual void GetAutoLinkInfo( granny_int32x InputIdx, granny_bool32& UseAutoLink, granny_int32x& AutoLinkSkipOffset );

    virtual granny_int32 GetInputIndexFromVisualIndex(granny_int32 Index) const;

};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_ADDITIVE_BLEND_H
#endif /* GSTATE_ADDITIVE_BLEND_H */
