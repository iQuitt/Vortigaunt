// ========================================================================
// $Notice: $
// ========================================================================
#if !defined(GSTATE_NODESTATE_CONDITION_H)
#include "gstate_header_prefix.h"

#include "gstate_conditional.h"

BEGIN_GSTATE_NAMESPACE;

class nodestate_condition : public conditional
{
    typedef conditional parent;

public:
    virtual conditional_type GetType();

    node* GetNode();
    void SetNode(node* Node);

    bool GetCheckTransitioningTo();
    void SetCheckTransitioningTo(bool TransitioningTo);

    bool GetCheckActive();
    void SetCheckActive(bool Active);

    bool GetCheckTransitioningFrom();
    void SetCheckTransitioningFrom(bool TransitioningFrom );

    DECL_CONCRETE_CLASS_TOKEN(nodestate_condition);

    virtual bool CaptureNodeLinks();
    virtual void Note_NodeDelete(node* DeletedNode);

protected:
    virtual bool IsTrueImpl(granny_real32 AtT, granny_real32 DeltaT);
        
private:
    node * m_Node;
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_NODESTATE_CONDITION_H
#endif /* GSTATE_NODESTATE_CONDITION_H */
