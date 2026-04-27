// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_SELECTION_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

struct animation_spec;

class selection : public node
{
    typedef node parent;

public:
    DECL_IO_MANIPULATION_INTERFACE();

    DECL_SAMPLEPOSE_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(selection);

    DECL_SAMPLEMORPH_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();
    DECL_SAMPLEEVENT_INTERFACE();

    virtual bool AllowSyncInputsWithParentOutputs() const { return true; }
    virtual bool AllowSyncOutputsWithParentOutputs() const { return true; }

public:

    granny_real32 GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx);

    bool ObtainSampleIndex(granny_real32 AtT, granny_int32 OutputIdx, int* Index);
    virtual granny_int32 GetInputIndexFromVisualIndex(granny_int32 Index) const;
    void AddInputSet();
    virtual void AddInputsForOutput(granny_int32x OutputIdx);
    virtual void DeleteInputsForOutput(granny_int32x OutputIdx);
    virtual void SetInputNamesForOutput(granny_int32x OutputIdx, char const* NewEdgeName);
    void AddInputsForParentOutputs();
    bool GetChooseOnActivate() const;
    void SetChooseOnActivate(bool ChooseOnActivate);
    virtual void Activate(granny_real32 AtT);

private:

    void CacheNumPoseInputs();
    granny_real32 m_ActivationParam;

};


END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_SELECTION_H
#endif /* GSTATE_SELECTION_H */
