// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_MASK_SOURCE_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

GSTATE_TRANSPARENT_GRANNY_STRUCT(unbound_track_mask);
GSTATE_OPAQUE_GRANNY_STRUCT(track_mask);


BEGIN_GSTATE_NAMESPACE;

struct animation_spec;

class mask_source : public node
{
    typedef node parent;

public:
    virtual bool BindToCharacter(gstate_character_instance* Instance);
    virtual void UnbindFromCharacter();

    void SetMask(granny_unbound_track_mask* UnboundMask);
    granny_unbound_track_mask const* GetMask();

    virtual bool SampleMaskOutput(granny_int32x OutputIdx,
                                  granny_real32 AtT,
                                  granny_track_mask*);

    DISALLOW_IO_MANIPULATION_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(mask_source);

private:
    granny_track_mask* m_TrackMask;
};


END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_MASK_SOURCE_H
#endif /* GSTATE_MASK_SOURCE_H */
