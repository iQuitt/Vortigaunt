// ========================================================================
// $Notice: $
// ========================================================================
#if !defined(GSTATE_TWO_PARAM_BLEND_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class two_param_blend : public node
{
    typedef node parent;
    DISALLOW_IO_MANIPULATION_INTERFACE();
    DECL_SAMPLEPOSE_INTERFACE();

public:
    granny_int32x GetNumSources();
    bool          AddSource();
    bool          DeleteSource(granny_int32x SourceIdx);

    void GetSourceCoords(granny_int32x SourceIdx,
                         granny_real32* OutX,
                         granny_real32* OutY);
    void SetSourceCoords(granny_int32x SourceIdx,
                         granny_real32 InX,
                         granny_real32 InY);

    granny_int32x GetNumTriIndices();
    bool          GetTriangulation(granny_int16* Indices, granny_int32 NumIndices);

    animation_slot* GetAnimationSlot(granny_int32x SourceIdx);
    void            SetAnimationSlot(granny_int32x SourceIdx, animation_slot* Slot);

    virtual bool BindToCharacter(gstate_character_instance* Instance);
    virtual void UnbindFromCharacter();

    virtual bool GetPhaseBoundaries(granny_int32x  OnOutput,
                                    granny_real32  AtT,
                                    granny_real32* PhaseStart,
                                    granny_real32* PhaseEnd);
    
    bool GetSampleWeights(granny_real32 AtT,
                          granny_int32  Indices[3],
                          granny_real32 Weights[3],
                          bool          ClipToNearest);
public:
    virtual void          Activate(granny_real32 AtT);
    virtual granny_real32 GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx);
    // virtual void          Synchronize(granny_int32x OnOutput,
    //                                   granny_real32 AtT,
    //                                   granny_real32 ReferenceStart,
    //                                   granny_real32 ReferenceEnd,
    //                                   granny_real32 LocalStart,
    //                                   granny_real32 LocalEnd);

protected:
    virtual void NoteAnimationSlotDeleted(animation_slot* Slot);
    virtual void RemapAnimationSlots(animation_slot** NewSlots);

private:
    granny_controlled_animation** m_Animations;

    granny_real32                 m_LocalOffset;

    granny_real32 m_LastTime;          // m_LastTime = cNotSetup -> Not setup
    granny_real32 m_ComputedDuration;  // 
    granny_int32  m_Indices[3];        // m_Indices[0] == -1 -> No valid sample
    granny_real32 m_Weights[3];

    
    void RecomputeTriangulation();

    granny_real32 ComputeLocalT(granny_real32 AtT);
    void UpdateSampling(granny_real32 AtT);
    
    // Todo: profile to see if the typical case is to leave the parameters alone...
    // // So we can resample
    // granny_real32 m_LastObservedX;
    // granny_real32 m_LastObservedY;
    // granny_int16  m_LastIndices[3];
    // granny_real32 m_LastWeights[3];

    
public:
    DECL_SNAPPABLE();
    DECL_CONCRETE_NODE_TOKEN(two_param_blend);
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_TWO_PARAM_BLEND_H
#endif /* GSTATE_TWO_PARAM_BLEND_H */
