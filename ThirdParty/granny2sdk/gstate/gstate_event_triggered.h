// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_EVENT_TRIGGERED_H)
#include "gstate_header_prefix.h"

#if !defined(GSTATE_CONDITIONAL_H)
#include "gstate_conditional.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class event_triggered : public conditional
{
    typedef conditional parent;

public:
    virtual conditional_type GetType();

    void SetTriggerEvent(node*  EventNode, granny_int32x  OutputIndex, char const*  EventName);
    void GetTriggerEvent(node** EventNode, granny_int32x* OutputIndex, char const** EventName);

    virtual void Note_OutputEdgeDelete(node*         AffectedNode,
                                       granny_int32x RemovedOutput);
    virtual void Note_NodeDelete(node* AffectedNode);

    DECL_CONCRETE_CLASS_TOKEN(event_triggered);
protected:
    virtual bool IsTrueImpl(granny_real32 AtT, granny_real32 DeltaT);
    virtual bool CaptureNodeLinks();

private:
    node* m_OutputNode;
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_EVENT_TRIGGERED_H
#endif /* GSTATE_EVENT_TRIGGERED_H */
