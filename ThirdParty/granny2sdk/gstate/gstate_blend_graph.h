// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_BLEND_GRAPH_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_CONTAINER_H
#include "gstate_container.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class blend_graph : public container
{
    typedef container parent;

public:
    DECL_IO_MANIPULATION_INTERFACE();

    virtual bool   SetOutputName(granny_int32x OutputIdx, char const* NewEdgeName);

    virtual int AddNewChild(node* Child);
    virtual int AddCopiedChild(node* Child);

    virtual granny_real32      SampleScalarOutput(granny_int32x OutputIdx,
                                                  granny_real32 AtT,
                                                  gstate_scalar_track_entry * TrackEntry);

    virtual bool               GetScalarOutputRange(granny_int32x OutputIdx,
                                                    granny_real32* OutMin, granny_real32* OutMax);

    virtual bool GetAllowAnyTransition() const;
    void SetAllowAnyTransition(bool Allow);

    virtual granny_real32 GetAnyTransitionDuration() const;
    void SetAnyTransitionDuration(granny_real32 Duration);

    virtual bool GetAllowTransitionExit() const;
    void SetAllowTransitionExit(bool Allow);

    virtual void AdvanceT(granny_real32 CurrentTime, granny_real32 DeltaT);

    
    DECL_SAMPLEPOSE_INTERFACE();

    virtual bool          GetMorphChannelBindings(granny_int32x OutputIdx,
                                                  char const**  MeshNames,
                                                  granny_int32x MeshNameCount);
    virtual granny_int32x GetNumMorphChannels(granny_int32x OutputIdx);
    virtual bool SampleMorphOutput(granny_int32x  OutputIdx,
                                   granny_real32  AtT,
                                   granny_real32* MorphWeights,
                                   granny_int32x NumMorphWeights);

    virtual bool SampleMaskOutput(granny_int32x OutputIdx,
                                  granny_real32 AtT,
                                  granny_track_mask* Mask);
    virtual bool SampleEventOutput(granny_int32x            OutputIdx,
                                   granny_real32            AtT,
                                   granny_real32            DeltaT,
                                   gstate_text_track_entry* EventBuffer,
                                   granny_int32x const      EventBufferSize,
                                   granny_int32x*           NumEvents);
    virtual bool GetAllEvents(granny_int32x            OutputIdx,
                              gstate_text_track_entry* EventBuffer,
                              granny_int32x const      EventBufferSize,
                              granny_int32x*           NumEvents);
    virtual bool GetCloseEventTimes(granny_int32x  OutputIdx,
                                    granny_real32  AtT,
                                    char const*    TextToFind,
                                    granny_real32* PreviousTime,
                                    granny_real32* NextTime);


public:
    virtual void Activate(granny_real32 AtT);
    virtual granny_real32 GetLocalTimeOffset(granny_real32 AtT, granny_int32x OnOutput);

    virtual bool DidSubLoopOccur(node*         SubNode,
                                 granny_int32  OnOutput,
                                 granny_real32 AtT,
                                 granny_real32 DeltaT);

private:
    virtual void AutoLinkOutputs();

protected:
    DECL_CONCRETE_NODE_TOKEN(blend_graph);
    IMPL_CASTABLE_INTERFACE(blend_graph);
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_BLEND_GRAPH_H
#endif /* GSTATE_BLEND_GRAPH_H */
