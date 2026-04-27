// ========================================================================
// $Notice: $
// ========================================================================
#if !defined(GSTATE_EVENT_MIXER_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class event_mixer : public node
{
    typedef node parent;

    DECL_SAMPLEEVENT_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(event_mixer);
    DECL_IO_MANIPULATION_INTERFACE();
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_EVENT_MIXER_H
#endif /* GSTATE_EVENT_MIXER_H */
