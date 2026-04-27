// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_CONTAINER_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

struct editing_comment;

BEGIN_GSTATE_NAMESPACE;

class blend_graph;

class container : public node
{
    typedef node parent;

public:
    bool ValidInputOutput() const;  // debug check...

    // The container stores a position to facilitate editing
    bool GetEditPosition(int& x, int& y);
    bool SetEditPosition(int x, int y);

    virtual bool BindToCharacter(gstate_character_instance* Instance);
    virtual void UnbindFromCharacter();

    int   GetNumChildren() const;
    node* GetChild(int ChildIdx) const;
    int   GetChildIdx(node* Child) const;

    int   FindChildIdxByName(char const* Name) const;
    node* FindChildByName(char const* Name) const;

    virtual bool MoveNodeToFront(node* Node);

    // Allows paths of the form
    //  - name|name|name or
    //  - name|*|name
    // * matches any container at ONE level.
    node* FindChildRecursively(char const* Path) const;

    virtual int   AddNewChild(node* Child);
    virtual int   AddCopiedChild(node* Child); // special case. Only different from AddChild in state_machine
    virtual bool  RemoveChildByIdx(int ChildIdx);
    bool          RemoveChild(node* Child);  // forwards to removebyidx, no need to override
    virtual bool  RemoveChildWithoutFixupByIdx(int ChildIdx);
    bool          RemoveChildWithoutFixup(node* Child);  // forwards to RemoveChildWithoutFixupByIdx, no need to override

    virtual void  AutoLinkOutputs(); //Links all possible outputs by name (recursive)

    void TraverseNode(TraverseNodeFunction TraverseFunction, void * TraverseData);
    
    bool DetectLoops() const;

    // Container overrides these to add the internal edges at the same time
    DECL_IO_MANIPULATION_INTERFACE();
    virtual bool SetInputName(granny_int32x InputIdx, char const* NewEdgeName);
    virtual bool SetOutputName(granny_int32x OutputIdx, char const* NewEdgeName);

    // Did a subnode have a loop event?
    virtual bool DidSubLoopOccur(node*         SubNode,
                                 granny_int32  OnOutput,
                                 granny_real32 AtT,
                                 granny_real32 DeltaT) = 0;

    // Debug check
    virtual bool CheckConnections();

    int GetNumComments();
    int AddComment(char const* InitialText,
                   granny_int32x PosX, granny_int32x PosY,
                   granny_int32x SizeX, granny_int32x SizeY);
    void DeleteComment(granny_int32x CommentIdx);
    bool MoveCommentToFront(granny_int32x CommentIdx);

    char const* GetCommentText(granny_int32x CommentIdx);
    bool SetCommentText(granny_int32x CommentIdx, char const* NewText);

    bool GetCommentPosition(granny_int32x CommentIdx,
                            granny_int32x* PosX,
                            granny_int32x* PosY,
                            granny_int32x* SizeX,
                            granny_int32x* SizeY);
    bool SetCommentPosition(granny_int32x CommentIdx,
                            granny_int32x PosX,
                            granny_int32x PosY,
                            granny_int32x SizeX,
                            granny_int32x SizeY);

    bool IsNodeActive(node* Node);

public:
    enum EConstants
    {
        eInvalidChild = -1
    };

private:
    // Cached version of the child token array
    node** m_ChildNodes;

    DECL_CLASS_TOKEN(container);
    IMPL_CASTABLE_INTERFACE(container);

    struct containerImpl
    {
        granny_int32    ChildTokenCount;
        granny_variant* ChildTokens;
        
        granny_int32x   EditingPosX;
        granny_int32x   EditingPosY;

        granny_int32x    CommentCount;
        editing_comment* Comments;
    };

    // let the blend graph peek at the child infrastructure
    friend class blend_graph;

public:
    bool ConnectedInput(granny_int32x ForOutput, node** Node, granny_int32x* EdgeIdx);

    // Only happens in the editor
    virtual void NoteAnimationSlotDeleted(animation_slot* Slot);
    virtual void RemapAnimationSlots(animation_slot** NewSlots, int NewSlotCount);

    virtual void NoteContainerDeleted();
    virtual void Note_ConditionalDelete(state_machine* Owner, int ConditionalIndex);

    virtual void LinkContainerChildren();
    
    // Only for the use of internal node functions, if you please
protected:
    friend class node;

    virtual void NoteOutputRemoval_Pre(node* AffectedNode, granny_int32x ToBeRemoved);
    virtual void NoteOutputRemoval_Post(node* AffectedNode, bool WasExternal);

    virtual void NoteOutputAddition(node* AffectedNode, granny_int32x InsertionIndex);

};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_CONTAINER_H
#endif /* GSTATE_CONTAINER_H */
