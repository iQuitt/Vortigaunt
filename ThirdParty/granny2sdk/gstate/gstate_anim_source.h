// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_ANIM_SOURCE_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

struct animation_slot;
struct anim_source_token;

class anim_source : public node
{
    typedef node parent;

public:
    virtual bool BindToCharacter(gstate_character_instance* Instance);
    virtual void UnbindFromCharacter();

    DECL_SAMPLEPOSE_INTERFACE();
    DECL_IO_MANIPULATION_INTERFACE();

    virtual bool GetPhaseBoundaries(granny_int32x  OnOutput,
                                    granny_real32  AtT,
                                    granny_real32* PhaseStart,
                                    granny_real32* PhaseEnd);

    animation_slot* GetAnimationSlot();
    bool            GetOutputLoopClamping() const;
    granny_real32   GetOutputSpeed() const;

    void SetAnimationSlot(animation_slot* Slot);
    void SetOutputLoopClamping(bool Clamped);
    void SetOutputSpeed(granny_real32 Speed);
    void SetAllowAnyTransition(bool Allow);
    granny_real32 GetAnyTransitionDuration() const;
    void SetAnyTransitionDuration(granny_real32 Duration);
    void SetAllowTransitionExit(bool Allow);

    bool        GetEventUseAnimEvents(granny_int32x ForOutput);
    char const* GetEventTrackName(granny_int32x ForOutput);
    bool        SetEventTrack(granny_int32x ForOutput, bool UseAnimEvents, char const* TrackName);
    virtual bool GetAllowAnyTransition() const;
    virtual bool GetAllowTransitionExit() const;

    static char const* GetAnimEventTrackName();

    
    bool GetScalarTrack(granny_int32x  ForOutput,
                        char const*&   Name,
                        granny_uint32& TrackKey);
    bool SetScalarTrack(granny_int32x       ForOutput,
                        char const*         TrackName,
                        granny_uint32 const TrackKey);

    char const* GetMorphTrackName(granny_int32x  ForOutput);
    bool SetMorphTrackName(granny_int32x       ForOutput,
                           char const*         TrackName);

    virtual bool          GetMorphChannelBindings(granny_int32x OutputIdx,
                                                  char const**  MeshNames,
                                                  granny_int32x MeshNameCount);
    virtual granny_int32x GetNumMorphChannels(granny_int32x OutputIdx);
    virtual bool SampleMorphOutput(granny_int32x  OutputIdx,
                                   granny_real32  AtT,
                                   granny_real32* MorphWeights,
                                   granny_int32x NumMorphWeights);

    virtual granny_real32 SampleScalarOutput(granny_int32x OutputIdx,
                                             granny_real32 AtT,
                                             gstate_scalar_track_entry * TrackEntry);

    virtual bool SampleEventOutput(granny_int32x            OutputIdx,
                                   granny_real32            AtT,
                                   granny_real32            DeltaT,
                                   gstate_text_track_entry* EventBuffer,
                                   granny_int32x const      EventBufferSize,
                                   granny_int32x*           NumEvents);

    bool GetAllEvents(granny_int32x            OutputIdx,
                      gstate_text_track_entry* EventBuffer,
                      granny_int32x const      EventBufferSize,
                      granny_int32x*           NumEvents);

    bool GetCloseEventTimes(granny_int32x  OutputIdx,
                            granny_real32  AtT,
                            char const*    TextToFind,
                            granny_real32* PrevTime,
                            granny_real32* NextTime);
    

    DECL_CONCRETE_NODE_TOKEN(anim_source);
    DECL_SNAPPABLE();
    IMPL_CASTABLE_INTERFACE(anim_source);

public:
    virtual void          Activate(granny_real32 AtT);
    virtual granny_real32 GetLocalTimeOffset(granny_real32 AtT, granny_int32x OnOutput);
    virtual void          Synchronize(granny_int32x OnOutput,
                                      granny_real32 AtT,
                                      granny_real32 ReferenceStart,
                                      granny_real32 ReferenceEnd,
                                      granny_real32 LocalStart,
                                      granny_real32 LocalEnd);

    // For updating tokens on load.  Fixes morph edge outputs from older formats.  Note
    // that this will leak memory, so it's not a good idea to allow this to run in the
    // game.
    static void FixTokenEdges(anim_source_token* Token);

    // For batch resetting all event edges.
    static void ResetEventEdges(anim_source_token* Token);

    virtual bool AllowSyncInputsWithParentOutputs() const { return false; }
    virtual bool AllowSyncOutputsWithParentOutputs() const { return true; }

protected:
    virtual void NoteAnimationSlotDeleted(animation_slot* Slot);
    virtual void RemapAnimationSlots(animation_slot** NewSlots, int NewSlotCount);

private:
    granny_controlled_animation* m_Animation;
    granny_text_track**          m_EventTracks;   // sparse?

    granny_curve2**              m_ScalarTracks;  // sparse?
    granny_real32*               m_ScalarDefaults;
    granny_int32*                m_ScalarBoneIndex; //Which bone index this scalar track is associated with. 

    granny_track_group**         m_MorphTracks;   // sparse?

    // Entry here is MeshChannelIdx -> VectorTrackIdx (NULL array indicates no binding, -1 indicates no mapping)
    granny_int32**               m_MorphBindings; // nb. entries may be null
    granny_int32*                m_MorphBindingCounts; // number of bound names...
    
    granny_real32                m_LocalOffset;

    granny_bool32                m_ActivationEdge; // true for just the frame the animation is activated on
};


END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_ANIM_SOURCE_H
#endif /* GSTATE_ANIM_SOURCE_H */
