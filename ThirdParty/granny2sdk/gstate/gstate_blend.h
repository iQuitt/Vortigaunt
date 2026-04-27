// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_BLEND_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class blend : public node
{
    typedef node parent;

public:
    DECL_SAMPLEPOSE_INTERFACE();

    DECL_SAMPLEMORPH_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();
    DECL_SAMPLEEVENT_INTERFACE();

    virtual void Activate(granny_real32 AtT);

    void CacheNumPoseInputs();

    bool CanPhaseLock(bool RefreshCache);
    bool GetPhaseLocked() const;
    void SetPhaseLocked(bool Lock);

    bool GetNeighborhooded() const;
    void SetNeighborhooded(bool Lock);

    bool GetChooseOnActivate() const;
    void SetChooseOnActivate(bool ChooseOnActivate);

    bool GetCircularBlend() const;
    void SetCircularBlend(bool CircularBlend);

    virtual granny_int32 GetInputIndexFromVisualIndex(granny_int32 Index) const;

    granny_int32 ObtainSampleIndex(granny_int32 N, granny_int32 OutputIndex);

    void AddInputSet();                                                     //Adds a new set of inputs matching the outputs
    virtual void AddInputsForOutput(granny_int32x OutputIndex);
    virtual void DeleteInputsForOutput(granny_int32x OutputIndex);
    virtual void SetInputNamesForOutput(granny_int32x OutputIdx, char const* NewEdgeName);

    virtual bool AllowSyncInputsWithParentOutputs() const { return true; } //Actually we will sync the outputs too, we'll do it in AddInputsForParentOutputs(). 
    virtual bool AllowSyncOutputsWithParentOutputs() const { return true; } //Output syncing happens when we sync the inputs

    virtual void AddInputsForParentOutputs();

protected:
    virtual void NoteInputChange(granny_int32x InputIndex);
    virtual void CaptureSiblingData();

    granny_real32* m_Durations;  // there are GetNumOutputs()-1 of these. This is pretty wasteful since we can have non-pose inputs as well. 

    granny_real32 m_LocalOffset;
    granny_real32 m_LastObservedParam;
    granny_real32 m_LastDurationFrom;
    granny_real32 m_LastDurationTo;
    granny_real32 m_ActivationParam;

    DECL_CONCRETE_NODE_TOKEN(blend);
    DECL_IO_MANIPULATION_INTERFACE();
    DECL_SNAPPABLE();

private:
    bool ComputeBlend(granny_real32  AtT,
                      granny_int32x  OutputIndex,
                      granny_real32& Factor,
                      granny_int32x& IndexFrom,
                      granny_int32x& IndexTo);

};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_BLEND_H
#endif /* GSTATE_BLEND_H */
