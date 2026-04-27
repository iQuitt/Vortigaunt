// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_MASK_UNION_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class mask_union : public node
{
    typedef node parent;

public:
    virtual bool BindToCharacter(gstate_character_instance* Instance);
    virtual void UnbindFromCharacter();

    virtual bool SampleMaskOutput(granny_int32x OutputIdx,
                                  granny_real32 AtT,
                                  granny_track_mask*);

    DISALLOW_IO_MANIPULATION_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(mask_union);
    DECL_SAMPLEMORPH_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();
    DECL_SAMPLEEVENT_INTERFACE();

private:
    // Set with BindToCharacter/Unbind
    granny_int32x      m_CachedBoneCount;
    granny_track_mask* m_CachedMask;
};


END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_MASK_UNION_H
#endif /* GSTATE_MASK_UNION_H */
