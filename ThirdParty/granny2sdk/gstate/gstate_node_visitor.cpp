// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_node_visitor.h"
#include "gstate_node_header_list.h"
#include "gstate_cpp_settings.h"

GSTATE node_visitor::~node_visitor()
{
}

void GSTATE
node_visitor::VisitNode(node*)
{
    // Nada
}

// Ignore the node, defined above.
#define TAKE_ACTION_NODE(type)
#define TAKE_ACTION(type)                       \
    void GSTATE                                 \
    node_visitor::VisitNode(type* Node)         \
    {                                           \
        VisitNode((type::parent*)Node);         \
    }

#include "gstate_node_type_list.h"

#undef TAKE_ACTION_NODE
#undef TAKE_ACTION

