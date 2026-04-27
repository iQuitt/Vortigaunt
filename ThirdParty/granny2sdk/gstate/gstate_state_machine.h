// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_STATE_MACHINE_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_CONTAINER_H
#include "gstate_container.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class parameters;
class conditional;
class tr_dynamic;

class state_machine : public container
{
    typedef container parent;

public:
    virtual int  AddNewChild(node* Child);
    virtual int  AddCopiedChild(node* Child);
    virtual bool RemoveChildByIdx(int ChildIdx);

    int  GetStartStateIdx() const;
    void SetStartStateIdx(int StartState);
    void SetStartState(node* State);
    void SetStartStateOverride(node* State);
    node* GetStartStateOverride();

    bool GetUseLastActive() const;
    void SetUseLastActive(bool UseLast);

    bool GetRandomStartState() const;
    void SetRandomStartState(bool RandomStartState);

    bool GetRetargetAnimationSources() const;
    void SetRetargetAnimationSources(bool UseLast);

    virtual bool GetAllowAnyTransition() const;
    void SetAllowAnyTransition(bool Allow);

    virtual granny_real32 GetAnyTransitionDuration() const;
    void SetAnyTransitionDuration(granny_real32 Duration);

    virtual bool GetAllowTransitionExit() const;
    void SetAllowTransitionExit(bool Allow);

    virtual bool GetAlwaysUseAnimEvents() const;
    void SetAlwaysUseAnimEvents(bool Use);

    virtual bool GetAutoForwardTransitionRequests() const;
    void SetAutoForwardTransitionRequests(bool Use);

    void CreateDynamicTransition();

    const char * GetMorphMeshName(int MorphOutputIdx);
    void SetMorphMeshName(int MorphOutputIdx, const char * MeshName);
    const char * GetMorphMeshNameForOutputName(const char * OutputName);

    tokenized* GetActiveElement();

    bool MoveNodeToFront(node* Node);
    
    node* GetStateByName(const char* StateName);

    // By name API
    bool RequestChangeToState(granny_real32 AtT, granny_real32 DeltaT, node * State);
    bool RequestChangeToState(granny_real32 AtT, granny_real32 DeltaT, char const* StateName);
    bool ForceChangeToState(granny_real32 AtT, char const* StateName);
    bool ForceChangeToState(granny_real32 AtT, node * State);

    // Normally, you should use these
    bool StartTransitionByName(granny_real32 AtT, char const* TransitionName);
    bool StartTransition(granny_real32 AtT, transition* Transition);
    bool ForceStartTransition(granny_real32 AtT, transition* Transition);
    bool StartDynamicTransition(granny_real32 AtT, granny_real32 DeltaT, node * State, float EarlyExitTransitionDuration, transition * AnyTransitionSource);
    bool StartDynamicTransition(granny_real32 AtT, granny_real32 DeltaT, const char * StateName, float EarlyExitTransitionDuration, transition * AnyTransitionSource);
    void StartAnyTransition(transition * AnyTransition, granny_real32 AtT, granny_real32 DeltaT, node * State, float EarlyExitTransitionDuration);
    void StartAnyTransition(transition * AnyTransition, granny_real32 AtT, granny_real32 DeltaT, const char * StateName, float EarlyExitTransitionDuration);

    // But if necessary: hit with hammer!
    bool ForceState(granny_real32 AtT, node* State);
    bool ForceTransition(granny_real32 AtT, transition* Transition);

    // Nudge the exit of the node in the correct direction
    void        SetPreferredExit(transition* PreferredExit);
    transition* GetPreferredExit();

    // Pose
    virtual granny_local_pose* SamplePoseOutput(granny_int32       OutputIdx,
                                                granny_real32      AtT,
                                                granny_real32      AllowedError,
                                                granny_pose_cache* PoseCache,
                                                granny_real32*     PoseWeight);
    virtual bool GetRootMotionVectors(granny_int32x OutputIdx,
                                      granny_real32 AtT,
                                      granny_real32 DeltaT,
                                      granny_real32* Translation,
                                      granny_real32* Rotation,
                                      bool Inverse);

    // Event
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
    // todo: next event?
    virtual bool GetCloseEventTimes(granny_int32x  OutputIdx,
                                    granny_real32  AtT,
                                    char const*    TextToFind,
                                    granny_real32* PreviousTime,
                                    granny_real32* NextTime);

    // Morph/scalar/mask
    granny_real32 SampleScalarOutput(granny_int32x OutputIdx,
                                     granny_real32 AtT); 

    //Note: zero-initialize TrackEntry if you pass it into SampleScalarOutput
    virtual granny_real32 SampleScalarOutput(granny_int32x OutputIdx,
                                     granny_real32 AtT,
                                     gstate_scalar_track_entry * TrackEntry);

    virtual bool GetScalarOutputRange(granny_int32x  OutputIdx,
                                     granny_real32* OutMin,
                                     granny_real32* OutMax);

    DECL_SAMPLEMORPH_INTERFACE();
    DECL_SAMPLEMASK_INTERFACE();

    virtual void AdvanceT(granny_real32 CurrentTime, granny_real32 DeltaT);

    DECL_CONCRETE_NODE_TOKEN(state_machine);
    DECL_SNAPPABLE();
    IMPL_CASTABLE_INTERFACE(state_machine);

    // Published parameters (corresponds to edges other than 0)
public:
    DECL_IO_MANIPULATION_INTERFACE();

    virtual bool SetOutputName(granny_int32x OutputIdx, char const* NewEdgeName);

    // Conditionals for transitions, etc.
public:
    granny_int32x GetNumConditionals();
    conditional*  GetConditional(granny_int32x Index);
    // bool          IsConditionalTrue(granny_int32x Index, granny_real32 AtT);

    granny_int32x AddConditional(conditional* NewCondition);
    bool ReplaceConditional(granny_int32x ConditionalIndex, conditional* NewCondition);
    void DeleteConditional(granny_int32x ConditionalIndex);

    static bool IsStateNode(node*);

public:
    virtual void Activate(granny_real32 AtT);
    virtual granny_real32 GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx);
    virtual bool BindToCharacter(gstate_character_instance* Instance);

    virtual bool DidSubLoopOccur(node*         SubNode,
                                 granny_int32  OnOutput,
                                 granny_real32 AtT,
                                 granny_real32 DeltaT);

    virtual bool DidLoopOccur(granny_int32x OnOutput,
        granny_real32 AtT,
        granny_real32 DeltaT);

    // For keeping the edge names consistent.  Not really for client use
    void NoteParameterNameChange(node* ParamOrEvent, int OutputIdx);
    virtual void NoteOutputRemoval_Pre(node* AffectedNode, granny_int32x ToBeRemoved);
    virtual void NoteOutputRemoval_Post(node* AffectedNode, bool WasExternal);
    virtual void NoteOutputAddition(node* AffectedNode, granny_int32x InsertionIndex);

    void NoteDeleteTransition(transition* Transition);

    virtual void NoteContainerDeleted();
    virtual void Note_ConditionalDelete(state_machine* Owner, int ConditionalIndex);
    
    virtual bool CheckConnections();

    bool IsDynamicTransitionActive();
    tr_dynamic* GetDynamicTransition() { return m_DynamicTransition; }
    granny_local_pose* GetDynamicTransitionSourcePose() { return m_DTSourcePose; }
    granny_real32* GetDynamicTransitionSourceRootTranslation() { return &m_DTSourceRootTranslation[0]; }
    granny_real32* GetDynamicTransitionSourceRootRotation() { return &m_DTSourceRootRotation[0]; }

    void AdjustChildInputs();

    static void SetNodeCopyContext( granny_uint32 *CopiedItems, granny_int32 NumCopiedItems, granny_uint32* MapKeys, granny_uint32* MapValues, granny_int32 NumMapItems );

private:
    static granny_uint32 *CopyContext_CopiedItems;
    static granny_int32 CopyContext_NumCopiedItems;
    static granny_uint32* CopyContext_MapKeys;
    static granny_uint32* CopyContext_MapValues;
    static granny_int32 CopyContext_NumMapItems;

    tokenized*    m_Active;
    conditional** m_Conditionals;

    transition* m_PreferredExit;

    tr_dynamic* m_DynamicTransition;

    node* m_StartStateOverride;

    //Stuff cached as the source of a dynamic transition. 
    //todo: Morph Weights
    granny_local_pose* m_DTSourcePose;
    granny_real32 m_DTSourceRootTranslation[3];
    granny_real32 m_DTSourceRootRotation[3];

    granny_int32x LastRandomStartStatesIndex;
    granny_int32x LastRandomStartStates[3];

private:
    void AddInputForNodeChild(node* Node, int OutputIdx, node * Child);
    void AddInputsForNode(node* Node);
    void ActivateConditionals(granny_real32 AtT);
    void SetActive( tokenized * Active );
    
    // bool AddParameterToChildren(node* Param, int OutputIdx);
    // bool RemoveParameterFromChildren(node* Param, int OutputIdx);

    bool AddOutputToChildren(int NewOutputIdx);
    bool RemoveOutputFromChildren(int DelOutputIdx);
    void RemoveOutputFromConditionals(node* AffectedNode, int DelOutputIdx);
    void RefreshChildOutputName(granny_int32x OutputIdx);

    void DefaultStartState();
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_STATE_MACHINE_H
#endif /* GSTATE_STATE_MACHINE_H */
