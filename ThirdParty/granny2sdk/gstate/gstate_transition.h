// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_TRANSITION_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_TOKENIZED_H
#include "gstate_tokenized.h"
#endif

BEGIN_GSTATE_NAMESPACE;
class node;
class transition;

enum transition_type
{
    Transition_OnRequest     = 0,
    Transition_OnLoop        = 1,
    Transition_OnSubLoop     = 2,
    Transition_LastResort    = 3,
    Transition_OnConditional = 4,
    Transition_Dynamic       = 5,

    NumTransitionTypes
};

enum transition_blend_type
{
    Transition_BlendLinear  = 0,
    Transition_BlendSmooth  = 1,
    NumTransitionBlendTypes
};

enum transition_event_handling_scheme
{
    Event_SwitchAtMidpoint = 0,
    Event_SampleBoth       = 1,
    Event_StartOnly        = 2,
    Event_EndOnly          = 3,
    Event_Neither          = 4,

    NumEventHandlingSchemes,
};

enum activate_trigger
{
    Trigger_Requested = 0,
    Trigger_Automatic = 1,

    NumTriggerTypes
};


char const* NameForTransitionType(transition_type);
char const* NameForEventHandlingScheme(transition_event_handling_scheme);
char const* NameForBlendType(transition_blend_type);

bool        TransitionTypeValidForNodes(transition_type, node* StartNode, node* EndNode);
transition* TransferTransitionToType(transition_type Type, transition* OldTransition);

struct synchronize_spec
{
    granny_variant ReferenceNode;
    granny_variant SyncedNode;

    // Am I going to regret optimizing for 1 dword?  Probably.
    granny_int16 ReferenceEventEdge;
    granny_int16 SyncedEventEdge;

    // If non-null, we're an event sync'er
    char* EventName;
};

class transition : public tokenized
{
    typedef tokenized parent;

public:
    virtual transition_type GetTransitionType() const = 0;

    void SetNodes(node* StartNode, granny_int32x StartX, granny_int32x StartY,
                  node* EndNode,   granny_int32x EndX,   granny_int32x EndY,
                  node* DestinationStartNode);
    void SetStartNode(node* StartNode);
    void SetEndNode(node* EndNode, node* DestinationStartNode);

    void SetAnyTransitionStartNode(node* StartNode);

    bool        SetPreferredExit(transition* PreferredExit);
    transition* GetPreferredExit();

    transition_event_handling_scheme GetEventHandlingScheme() const;
    void                             RuntimeOverrideEventHandlingScheme(transition_event_handling_scheme NewScheme); //For runtime override
    void                             SetEventHandlingScheme(transition_event_handling_scheme NewScheme);
    transition_blend_type GetBlendType() const;
    void SetBlendType(transition_blend_type NewBlendType);

    granny_real32 GetDuration();
    void          SetDuration(granny_real32);

    node* GetDestinationStartNode();
    void          SetDestinationStartNode(node * DestStartNode);

    void GetDrawingHints(granny_int32x& StartX, granny_int32x& StartY,
        granny_int32x& EndX, granny_int32x& EndY) const;

    void SetDrawingHints(granny_int32x StartX, granny_int32x StartY,
        granny_int32x EndX, granny_int32x EndY);

    virtual char const* GetName() const;
    virtual bool        SetName(char const* NewName);

    bool          GetIsAnyTransition();
    void          SetIsAnyTransition(bool Any);

    // User meta data interface.  Should be of the form:
    //
    //  "whatever=foo;this=bar;"
    //
    // I.e, terminate in a semi colon, '=' denotes the boundary between key and value.  This will
    // allow the studio to loft interfaces around the string safely.
    void        SetKeyValueString(char const* KeyValues);
    char const* GetKeyValueString();

    node* GetStartNode();
    node* GetEndNode();

    // ========== Condition interface
    //  Conditionals control when a transition is "takeable."  You may specify more than
    //  one condition, and and/or them together to create complex conditions.  The
    //  condition object itself is owned by the state_machine parent of the start/end
    //  nodes.
    granny_int32x GetNumConditionals();
    granny_int32x AddConditional();
    void          DeleteConditional(granny_int32x Index);
    conditional*  GetConditional(granny_int32x Index);
    void          SetConditional(granny_int32x Index, conditional*);
    bool          GetAndConditions();
    void          SetAndConditions(bool And);

    void SetInvertConditional(granny_int32x Index, bool Inverted);
    bool GetInvertConditional(granny_int32x Index);

    
    bool CheckConditionals(granny_real32 AtT);

    bool GetAllowTransitionExit() const;
    void SetAllowTransitionExit(bool Allow);

    // =====
    
    // ========== Sync interface
    //  Briefly, a sync is a list of nodes that are to be aligned for the transition.  For
    //  instance, when you transition from a walk to a run, you want to line up the gait
    //  such that the blend is correct.  It is possible that the referred-to nodes are
    //  internal to a blend-graph or state-machine.  Only the first level of nodes may be
    //  aligned in this manner for now.
    //
    //  A sync can either be phase-based, i.e, the position of the animation is aligned
    //  within a single iteration of the animation, or event based.
    granny_int32x GetNumSyncs();
    granny_int32x AddSync();
    void          DeleteSync(granny_int32x Index);

    void GetSyncParameters(granny_int32x Index,
                           synchronize_spec* OutParam,
                           node**            OutReference,
                           node**            OutSynced);
    void SetSyncParameters(granny_int32x Index, synchronize_spec const& NewParam);

    // Note that to minimize blending artifacts, the point that we sync the animations to
    // is the *midpoint* of the transition, where the influence from both animations is
    // strongest.

    void ExecuteSyncPlan(granny_real32 SyncT);
    // =====
    

    // Pass 0: probabilistic transitions
    // Pass 1: last resort, etc.
    virtual bool ShouldActivate(int PassNumber,
                                activate_trigger Trigger,
                                granny_real32 AtT,
                                granny_real32 DeltaT) = 0;


    virtual granny_local_pose* SamplePose(granny_real32 AtT,
                                  granny_real32 AllowedError,
                                  granny_pose_cache* PoseCache,
                                  granny_real32* PoseWeight);
    virtual bool GetRootMotionVectors(granny_real32  AtT,
                              granny_real32  DeltaT,
                              granny_real32* ResultTranslation,
                              granny_real32* ResultRotation,
                              bool           Inverse);

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
    virtual bool GetCloseEventTimes(granny_int32x  OutputIdx,
                                    granny_real32  AtT,
                                    char const*    TextToFind,
                                    granny_real32* PreviousTime,
                                    granny_real32* NextTime);
    
    virtual void Activate(granny_real32 AtT);
    virtual void Deactivate();
    void AdvanceT(granny_real32 AtT, granny_real32 DeltaT);
    bool IsActive() const;

    virtual bool CreateSnapshot(granny_file_writer* Writer);
    virtual bool ResetFromSnapshot(granny_file_reader* Reader,
                                   granny_int32        StartPosition);

    DECL_CLASS_TOKEN(transition);
    IMPL_CASTABLE_INTERFACE(transition);
public:
    // Creation interface for the node that contains this transition
    virtual bool CaptureNodeLinks();

    virtual void Note_ConditionalDelete(conditional* Conditional);
    virtual void Note_NodeDelete(node* DeletedNode);
    virtual void Note_OutputEdgeDelete(node* DeletedNode, granny_int32x EdgeIndex);

protected:
    void RuntimeOverrideTransitionDuration(float Duration);

protected:
    node* m_StartNode;
    node* m_EndNode;
    node* m_DestinationStartNode;

    conditional** m_Conditionals;

    transition* m_PreferredExit;

    // Cached for the sync interface
    node** m_ReferenceNodes;
    node** m_SyncedNodes;

    granny_bool32 m_Active;
    granny_real32 m_ActiveTimer;

public:
    granny_real32 ComputeDestFactor();
    granny_real32 GetTimeRunning();
};

END_GSTATE_NAMESPACE;


#include "gstate_header_postfix.h"
#define GSTATE_TRANSITION_H
#endif /* GSTATE_TRANSITION_H */
