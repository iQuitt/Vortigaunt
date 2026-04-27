// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_timeselect.h $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_TIMESELECT_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class timeselect : public node
{
    typedef node parent;

public:
    DECL_SAMPLEMORPH_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();
    DECL_SAMPLEEVENT_INTERFACE();
    DECL_IO_MANIPULATION_INTERFACE();

    granny_real32 GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx);
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

    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;

    bool GetTimeIsRelative() const;
    void SetTimeIsRelative(bool IsRelative);

    DECL_CONCRETE_NODE_TOKEN(timeselect);

    virtual void AddInputsForOutput(granny_int32x OutputIndex);
    virtual void DeleteInputsForOutput(granny_int32x OutputIndex);
    virtual void SetInputNamesForOutput(granny_int32x OutputIndex, char const* NewEdgeName);

private:

    granny_real32 GetPosition(granny_real32 AtT);
    granny_real32 ComputeSampleT(granny_real32 AtT, granny_real32 Position, node* Node, granny_int32x Edge);
};


END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_TIMESELECT_H
#endif /* GSTATE_TIMESELECT_H */
