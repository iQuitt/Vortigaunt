// ========================================================================
// $Notice: $
// ========================================================================
#if !defined(GSTATE_EVENT_RENAMER_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class event_renamer : public node
{
    typedef node parent;

public:
    granny_int32x NumRenames();
    void          AddRename(char const* NameInput, char const* NameOutput);
    void          DeleteRename(granny_int32x Index);

    char const*   GetRenameInput(granny_int32x Index);
    char const*   GetRenameOutput(granny_int32x Index);

    void SetRename(granny_int32x Index, char const* NameInput, char const* NameOutput);

    DECL_SAMPLEEVENT_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(event_renamer);
    DISALLOW_IO_MANIPULATION_INTERFACE();

public:
    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_EVENT_RENAMER_H
#endif /* GSTATE_EVENT_RENAMER_H */

