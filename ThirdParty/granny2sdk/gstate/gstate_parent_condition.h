// ========================================================================
// $Notice: $
// ========================================================================
#if !defined(GSTATE_PARENT_CONDITION_H)
#include "gstate_header_prefix.h"

#include "gstate_conditional.h"

BEGIN_GSTATE_NAMESPACE;

class parent_condition : public conditional
{
    typedef conditional parent;

public:
    virtual conditional_type GetType();

    // Set to NULL, -1 to clear.
    void SetExternalCondition(state_machine* Machine, int ConditionIndex);
    void GetExternalCondition(state_machine** Machine, int* ConditionIndex);

    DECL_CONCRETE_CLASS_TOKEN(parent_condition);

    virtual bool CaptureNodeLinks();

protected:
    virtual bool IsTrueImpl(granny_real32 AtT, granny_real32 DeltaT);

private:
    state_machine * m_ExternalStateMachine;
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_PARENT_CONDITION_H
#endif /* GSTATE_parent_condition_H */
