// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_MIRROR_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

struct granny_mirror_specifcation;

BEGIN_GSTATE_NAMESPACE;

class mirror : public node
{
    typedef node parent;

public:
    virtual bool BindToCharacter(gstate_character_instance* Instance);
    virtual void UnbindFromCharacter();

    granny_mirror_axis GetMirrorAxis();
    void SetMirrorAxis(granny_mirror_axis);

    int  GetNameSwapCount();
    bool SetNameSwaps(int NamePairCount,
                      char const** NamePairs);

    char const* GetNameSwap(int Idx);
    bool SetNameSwap(int Idx, char const* NewStr);

    DECL_SAMPLEPOSE_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(mirror);
    DECL_SAMPLEMORPH_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();
    DECL_SAMPLEEVENT_INTERFACE();

    granny_real32 GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx);

    virtual bool AllowSyncInputsWithParentOutputs() const { return true; }
    virtual bool AllowSyncOutputsWithParentOutputs() const { return true; }

public:
    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;

private:
    granny_mirror_specification* m_MirrorSpec;
    bool CreateMirrorSpec();
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_MIRROR_H
#endif /* GSTATE_MIRROR_H */
