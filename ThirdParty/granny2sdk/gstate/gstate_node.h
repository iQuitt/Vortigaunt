// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_NODE_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_BASE_H
#include "gstate_base.h"
#endif

#ifndef GSTATE_TOKENIZED_H
#include "gstate_tokenized.h"
#endif


BEGIN_GSTATE_NAMESPACE;

class container;
class transition;
class node_visitor;
struct animation_slot;

enum node_edge_type
{
    // These may NOT change.  Ever.
    PoseEdge   = 0,
    ScalarEdge = 1,
    MaskEdge   = 2,
    EventEdge  = 3,
    MorphEdge  = 4,

    // explicit because the above are explicit
    EdgeTypeCount = 5
};

class node;
typedef void (*TraverseNodeFunction)(node *Node, void *Data);

class node : public tokenized
{
    typedef tokenized parent;

public:
    // Nodes are the first place we need to get character specific information...
    virtual bool BindToCharacter(gstate_character_instance* Instance);
    virtual void UnbindFromCharacter();
    gstate_character_instance* GetBoundCharacter() const;

    virtual void AcceptNodeVisitor(node_visitor*);
    virtual void TraverseNode(TraverseNodeFunction TraverseFunction, void * TraverseData);

    virtual char const* GetName() const;
    virtual bool        SetName(char const* NewName);

    bool IsNodeAnOnLoopTransitionEndNode(node * EndNode) const;

    //The temporary name of the node while typing in its name
    char const * GetEditName();
    bool SetEditName(char const* NewName);

    virtual void        MarkNameUserSpecified();
    virtual bool        IsNameUserSpecified();

    container* GetParent();
    virtual void SetParent(container*);

    // Nodes store a position to facilitate editing
    void GetPosition(granny_int32x& x, granny_int32x& y);
    void SetPosition(granny_int32x x, granny_int32x y);

    // If this returns < 0, the node does not support duration querying.
    virtual granny_real32 GetDuration(granny_int32x OnOutput, granny_real32 AtT);

    virtual granny_real32 GetAnyTransitionDuration() const;

    transition * GetAnyTransition();

    virtual bool GetPhaseBoundaries(granny_int32x OnOutput,
                                    granny_real32 AtT,
                                    granny_real32* PhaseStart,
                                    granny_real32* PhaseEnd);

    // =========================================================
    // Input/Output API
    // =========================================================
    granny_int32x GetNumInputs() const;
    granny_int32x GetNumOutputs() const;

    granny_int32x GetNthExternalInput(granny_int32x n) const;
    granny_int32x GetNthExternalOutput(granny_int32x n) const;
    granny_int32x GetNthInternalInput(granny_int32x n) const;
    granny_int32x GetNthInternalOutput(granny_int32x n) const;

    granny_int32x WhichExternalInput(granny_int32x RawIndex) const;
    granny_int32x WhichExternalOutput(granny_int32x RawIndex) const;

    // DECL_IO_MANIPULATION_INTERFACE
    virtual granny_int32x AddInput(node_edge_type EdgeType, char const* EdgeName);
    virtual bool          DeleteInput(granny_int32x InputIndex);
    virtual granny_int32x AddOutput(node_edge_type EdgeType, char const* EdgeName);
    virtual bool          DeleteOutput(granny_int32x OutputIndex);

    virtual void AddInputsForOutput(granny_int32x OutputIdx);
    virtual void DeleteInputsForOutput(granny_int32x OutputIdx);
    virtual void SetInputNamesForOutput(granny_int32x OutputIdx, char const* NewEdgeName);

    void           GetInputConnection(granny_int32x   InputIdx,
                                      node**          OtherNode,
                                      granny_int32x*  NodeOutputIdx) const;
    char const*    GetInputName(granny_int32x InputIdx) const;
    node_edge_type GetInputType(granny_int32x InputIdx) const;
    bool           IsInputExternal(granny_int32x InputIdx) const;
    bool           IsInputInternal(granny_int32x InputIdx) const { return !IsInputExternal(InputIdx); }
    bool           SetInputConnection(granny_int32x InputIdx,
                                      node*         OtherNode,
                                      granny_int32x OtherOutputIdx);
    virtual bool   SetInputName(granny_int32x InputIdx, char const* NewEdgeName);

    node_edge_type GetOutputType(granny_int32x OutputIdx) const;
    char const*    GetOutputName(granny_int32x OutputIdx) const;
    bool           IsOutputExternal(granny_int32x OutputIdx) const;
    bool           IsOutputInternal(granny_int32x OutputIdx) const { return !IsOutputExternal(OutputIdx); }
    virtual bool   SetOutputName(granny_int32x OutputIdx, char const* NewEdgeName);

    virtual bool AllowSyncInputsWithParentOutputs() const { return false; }
    virtual bool AllowSyncOutputsWithParentOutputs() const { return true; }
    virtual void AddInputsForParentOutputs();
    virtual void AddOutputsForParentOutputs();

    virtual void GetAutoLinkInfo( granny_int32x InputIdx, granny_bool32& UseAutoLink, granny_int32x& AutoLinkSkipOffset );

    //The actual inputs of a node are in the order they got added, but they get displayed sorted in the order that they match up to 
    //outputs. This function provides that mapping. 
    virtual granny_int32 GetInputIndexFromVisualIndex(granny_int32 Index) const;

    // =========================================================
    // Sampling API
    // =========================================================

    virtual granny_real32 SampleScalarOutput(granny_int32x OutputIdx,
                                             granny_real32 AtT,
                                             gstate_scalar_track_entry * TrackEntry);

    virtual bool GetScalarOutputRange(granny_int32x  OutputIdx,
                                      granny_real32* OutMin,
                                      granny_real32* OutMax);  // returns false if range query unsupported


    // -- Animation pose API
    granny_local_pose* SamplePoseOutput(granny_int32x OutputIdx,
                                        granny_real32 AtT,
                                        granny_real32 AllowedError,
                                        granny_pose_cache* PoseCache);

    virtual granny_local_pose* SamplePoseOutput(granny_int32x OutputIdx,
        granny_real32 AtT,
        granny_real32 AllowedError,
        granny_pose_cache* PoseCache,
        granny_real32* PoseWeight);

    virtual bool GetRootMotionVectors(granny_int32x  OutputIdx,
                                      granny_real32  AtT,
                                      granny_real32  DeltaT,
                                      granny_real32* Translation,
                                      granny_real32* Rotation,
                                      bool           Inverse);

    // todo: may have to adjust this for state_machine
    virtual bool          GetMorphChannelBindings(granny_int32x OutputIdx,
                                                  char const**  MeshNames,
                                                  granny_int32x MeshNameCount);
    virtual granny_int32x GetNumMorphChannels(granny_int32x OutputIdx);
    virtual bool          SampleMorphOutput(granny_int32x  OutputIdx,
                                            granny_real32  AtT,
                                            granny_real32* MorphWeights,
                                            granny_int32x NumMorphWeights);
    
    virtual bool SampleMaskOutput(granny_int32x OutputIdx,
                                  granny_real32 AtT,
                                  granny_track_mask*);

    // -- Event api
    virtual bool SampleEventOutput(granny_int32x            OutputIdx,
                                   granny_real32            AtT,
                                   granny_real32            DeltaT,
                                   gstate_text_track_entry* EventBuffer,
                                   granny_int32x const      EventBufferSize,
                                   granny_int32x*           NumEvents);
    virtual bool GetNextEvent(granny_int32x OutputIdx,
                              granny_real32 AtT,
                              gstate_text_track_entry* Event);
    virtual bool GetAllEvents(granny_int32x            OutputIdx,
                              gstate_text_track_entry* EventBuffer,
                              granny_int32x const      EventBufferSize,
                              granny_int32x*           NumEvents); // Returns all possible events on this edge
    virtual bool GetCloseEventTimes(granny_int32x  OutputIdx,
                                    granny_real32  AtT,
                                    char const*    TextToFind,
                                    granny_real32* PreviousTime,
                                    granny_real32* NextTime);


    virtual void AdvanceT(granny_real32 CurrentTime, granny_real32 DeltaT) { }

    // =========================================================
    // Transition API
    // =========================================================
    granny_int32x GetNumTransitions() const;
    transition*   GetTransition(granny_int32x TransitionIdx);

    granny_int32x AddTransition(transition* Transition);
    bool          DeleteTransitionByIdx(granny_int32x TransitionIdx);
    bool          DeleteTransition(transition* Transition);

    //Used by copy-paste system to transfer a transition originating in this node to another node
    bool TransferTransitionsTo(transition * Transition, node * OtherNode);

    // Used to validate edits in the tool.  Asserts if connections are found to be
    // incorrect after changes are baked.
    virtual bool CheckConnections();

    virtual bool GetAllowAnyTransition() const { return false; }
    virtual bool GetAllowTransitionExit() const { return false; }

    virtual void EnsureAnyTransition();

    // User meta data interface.  Should be of the form:
    //
    //  "whatever=foo;this=bar;"
    //
    // I.e, terminate in a semi colon, '=' denotes the boundary between key and value.  This will
    // allow the studio to loft interfaces around the string safely.
    void        SetKeyValueString(char const* KeyValues);
    char const* GetKeyValueString();

protected:
    granny_int32x AddInputImpl(node_edge_type EdgeType, char const* EdgeName);
    bool          DeleteInputImpl(granny_int32x InputIndex);

    granny_int32x AddOutputImpl(node_edge_type EdgeType, char const* EdgeName);
    bool          DeleteOutputImpl(granny_int32x OutputIndex);
    
    // todo: need to use a vistor for this to seal it off?
public:
    virtual bool DidLoopOccur(granny_int32x OnOutput,
                              granny_real32 AtT,
                              granny_real32 DeltaT);
public:
    DECL_CLASS_TOKEN(node);
    IMPL_CASTABLE_INTERFACE(node);

public:
    // todo: need to loft this out into a "state_element" interface.
    //   tokenized->state_element->(node | transition)
    virtual void          Activate(granny_real32 AtT);
    virtual granny_real32 GetLocalTimeOffset(granny_real32 AtT, granny_int32x OnOutput);

    virtual void Synchronize(granny_int32x OnOutput,
                             granny_real32 AtT,
                             granny_real32 ReferenceStart,
                             granny_real32 ReferenceEnd,
                             granny_real32 LocalStart,
                             granny_real32 LocalEnd);

public:
    // Should only be called by container at creation time.
    virtual void CaptureSiblingLinks();
    virtual void CaptureSiblingData();

    // For advanced input/output walking
public:
    virtual bool GetOutputForward(granny_int32x OutputIdx, granny_int32x& ResultIdx) const;
    virtual bool GetInputForward(granny_int32x InputIdx, granny_int32x& ResultIdx) const;

    // Only happens in the editor
    virtual void NoteAnimationSlotDeleted(animation_slot* Slot);

    static void AutoLinkBlendNode(node * FromNode, granny_int32x FromEdge, node * ToNode, granny_int32x ToEdge, bool Disconnect, granny_int32x OutputFilter);

// === For the editor only === 
// === For the editor only === 
// === For the editor only === 
public:
    void TransferTransitionsTo(node* OtherNode);  // transfers all transitions owned by this node
    void RetargetTransitionsTo(node* OtherNode);  // changes the *target* of the transitions owned by this node

    // ONLY FOR THE STUDIO
    bool CaptureTransitions(transition** CaptureArray, int CaptureArraySize);
    bool RemoveTransitionByIdx(granny_int32x TransitionIdx);

    // Only for editor display
    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;

    // Only for copy/paste functionality
    virtual void RemapAnimationSlots(animation_slot** NewSlots, int NewSlotCount);

    static void SetAutoLinkBlendNodeOutputs(bool val);
    static bool GetAutoLinkBlendNodeOutputs();

    static void SetAutoAddBlendGraphChildOutputs(bool val);
    static bool GetAutoAddBlendGraphChildOutputs();

    static void SetAutoAddBlendGraphChildInputsForOutputs(bool val);
    static bool GetAutoAddBlendGraphChildInputsForOutputs();

// === For the editor only === 
// === For the editor only === 
// === For the editor only === 
    
protected:
    // Allows manipulation of internal inputs/outputs
    void SetInputInternal(granny_int32x InputIdx, granny_int32x ExternalOutput);
    void SetOutputInternal(granny_int32x OutputIdx, granny_int32x ExternalInput);

    // Notification of input changes, happens *after* the input has been switched
    virtual void NoteInputChange(granny_int32x InputIndex);

private:
    node**       m_Inputs;
    transition** m_Transitions;
    transition*  m_AnyTransition;

    gstate_character_instance* m_BoundCharacter;
    container*   m_Parent;
    char*        m_EditName;

    static bool m_AutoLinkBlendNodeOutputs;
    static bool m_AutoAddBlendGraphChildOutputs;
    static bool m_AutoAddBlendGraphChildInputsForOutputs;
};


END_GSTATE_NAMESPACE;

// Keep these elsewhere...
#include "gstate_node_macros.h"


#include "gstate_header_postfix.h"
#define GSTATE_NODE_H
#endif /* GSTATE_NODE_H */
