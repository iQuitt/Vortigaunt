// ========================================================================
// $File: //jeffr/granny_29/gstate/GSTATE_TRANSITION_DYNAMIC.h $
// $DateTime: 2012/05/11 15:03:34 $
// $Change: 37384 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_TRANSITION_DYNAMIC_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_TRANSITION_H
#include "gstate_transition.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class tr_dynamic : public transition
{
    typedef transition parent;

public:
    virtual transition_type GetTransitionType() const;

    bool ShouldActivate(int PassNumber,
                        activate_trigger Trigger,
                        granny_real32 AtT,
                        granny_real32 DeltaT);

    granny_local_pose* SamplePose(granny_real32 AtT,
                                  granny_real32 AllowedError,
                                  granny_pose_cache* PoseCache,
                                  granny_real32* PoseWeight);

    bool GetRootMotionVectors(granny_real32  AtT,
        granny_real32  DeltaT,
        granny_real32* ResultTranslation,
        granny_real32* ResultRotation,
        bool           Inverse);

    void InitializeDynamicTransition(node* Start, node* Dest, bool InterruptedTransition, float Duration, transition * AnyTransitionSource);
    granny_bool32 GetInterruptedTransition() { return m_InterruptedTransition; }

    DECL_CONCRETE_CLASS_TOKEN(tr_dynamic);

    IMPL_CASTABLE_INTERFACE(tr_dynamic);

private:
    granny_bool32 m_InterruptedTransition;
    granny_local_pose * m_SourcePose;
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_TRANSITION_DYNAMIC_H
#endif /* GSTATE_TRANSITION_DYNAMIC_H */
