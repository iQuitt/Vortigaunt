// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_container.h"

#include "gstate_quick_vecs.h"
#include "gstate_token_context.h"
#include "gstate_conditional.h"
#include "gstate_transition.h"
#include "gstate_transition_onsubloop.h"
#include "gstate_state_machine.h"
#include "gstate_blend_graph.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;
using namespace std;

struct editing_comment
{
    char*        Text;

    granny_int32 PosX;
    granny_int32 PosY;

    granny_int32 SizeX;
    granny_int32 SizeY;
};

static granny_data_type_definition EditingCommentType[] =
{
    { GrannyStringMember, "Text" },
    { GrannyInt32Member,  "PosX" },
    { GrannyInt32Member,  "PosY" },
    { GrannyInt32Member,  "SizeX" },
    { GrannyInt32Member,  "SizeY" },
    { GrannyEndMember },
};

granny_data_type_definition container::containerImplType[] =
{
    { GrannyReferenceToArrayMember, "ChildTokens", GrannyVariantType },
    { GrannyInt32Member,            "EditingPosX" },
    { GrannyInt32Member,            "EditingPosY" },
    { GrannyReferenceToArrayMember, "Comments", EditingCommentType },
    { GrannyEndMember },
};

// container is a pure virtual class, so there is no need to create a slotted token container

void GSTATE
container::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(container);

    // Clone the comments and their strings...
    GStateCloneArray(m_containerToken->Comments,
                     OldToken->Comments,
                     m_containerToken->CommentCount);
    for (int Idx = 0; Idx < m_containerToken->CommentCount; ++Idx)
    {
        GStateCloneString(m_containerToken->Comments[Idx].Text,
                          OldToken->Comments[Idx].Text);
    }
    
    // and the child tokens...
    GStateCloneArray(m_containerToken->ChildTokens,
                     OldToken->ChildTokens,
                     m_containerToken->ChildTokenCount);
}

void GSTATE
container::ReleaseOwnedToken_container()
{
    // Release the comments and their strings...
    for (int Idx = 0; Idx < m_containerToken->CommentCount; ++Idx)
    {
        GStateDeallocate(m_containerToken->Comments[Idx].Text);
    }
    GStateDeallocate(m_containerToken->Comments);
    
    // And the child tokens
    GStateDeallocate(m_containerToken->ChildTokens);
}


GSTATE
container::container(token_context*               Context,
                     granny_data_type_definition* TokenType,
                     void*                        TokenObject,
                     token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_ChildNodes(0),
    m_containerToken(0)
{
    IMPL_INIT_FROM_TOKEN(container);
    {
        if (m_containerToken->ChildTokenCount)
            m_ChildNodes = GStateAllocArray(node*, m_containerToken->ChildTokenCount);

        // First, create all of the children.  Note that we're handling here a very special
        // case, in which there are certain sub-nodes that are members of deleted node types.
        // That will *not* happen very often, but it does occur.  Make sure that we don't
        // screw it up.  When this happens, we have to adjust ChildTokens, which is
        // *not* accounted for with a TakeTokenOwnership.  Fun!
        {for (int Idx = 0; Idx < m_containerToken->ChildTokenCount; /*++Idx*/)
        {
            tokenized* Product =
                GetTokenContext()->CreateFromToken(m_containerToken->ChildTokens[Idx].Type,
                                                   m_containerToken->ChildTokens[Idx].Object);
            if (Product != 0)
            {
                m_ChildNodes[Idx] = GSTATE_DYNCAST(Product, node);
                GStateAssert(m_ChildNodes[Idx]);
                GStateAssert(m_ChildNodes[Idx]->GetParent() == this);

                // Advance
                ++Idx;
            }
            else
            {
                GStateWarning("container::container: Found an invalid child node, possibly obsolete class?\n");
                if ((Idx + 1) < m_containerToken->ChildTokenCount)
                {
                    // Shift the rest of the array down
                    memmove(m_containerToken->ChildTokens + Idx,
                            m_containerToken->ChildTokens + (Idx+1),
                            (sizeof(*m_containerToken->ChildTokens) *
                             (m_containerToken->ChildTokenCount - (Idx+1))));
                }

                // This can cause ChildTokenCount to go to zero, if that occurs, NULL the
                // array pointer, we have no children.  Do NOT free.  Note that this lack of
                // free() is why QVecRemoveElement is not employed here.
                --m_containerToken->ChildTokenCount;
                if (m_containerToken->ChildTokenCount == 0)
                    m_containerToken->ChildTokens = 0;
            }
        }}

        // Linkup must happen in the concrete container class.
    }
}

GSTATE
container::~container()
{
    // Delete our children...
    {for (int Idx = 0; Idx < m_containerToken->ChildTokenCount; ++ Idx)
    {
        GStateDelete<node>(m_ChildNodes[Idx]);
        m_ChildNodes[Idx] = 0;
    }}
    GStateDeallocate(m_ChildNodes);
    m_ChildNodes = 0;

    DTOR_RELEASE_TOKEN(container);
}


bool GSTATE
container::GetEditPosition(int& x, int& y)
{
    x = m_containerToken->EditingPosX;
    y = m_containerToken->EditingPosY;

    return true;
}

bool GSTATE
container::SetEditPosition(int x, int y)
{
    // You're not supposed to set this if you're in a non-editable context
    GStateAssert(IsEditable());

    TakeTokenOwnership();

    m_containerToken->EditingPosX = x;
    m_containerToken->EditingPosY = y;
    return true;
}


bool GSTATE
container::BindToCharacter(gstate_character_instance* Instance)
{
    if (!parent::BindToCharacter(Instance))
        return false;

    bool AllSucceeded = true;
    {for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        GStateAssert(m_ChildNodes && m_ChildNodes[Idx]);

        AllSucceeded = AllSucceeded &&
            m_ChildNodes[Idx]->BindToCharacter(Instance);
    }}

    return AllSucceeded;
}

void GSTATE
container::UnbindFromCharacter()
{
    {for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        GStateAssert(m_ChildNodes && m_ChildNodes[Idx]);
        m_ChildNodes[Idx]->UnbindFromCharacter();
    }}

    parent::UnbindFromCharacter();
}


bool GSTATE
container::FillDefaultToken(granny_data_type_definition* TokenType,
                            void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares containerImpl*& Slot = // member
    GET_TOKEN_SLOT(container);

    // Our slot in this token should be empty.
    // Create a new NodeToken
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    // Initialize to default (note that by default, we have no states)
    Slot->ChildTokenCount = 0;
    Slot->ChildTokens     = 0;

    Slot->EditingPosX = 0;
    Slot->EditingPosY = 0;

    return true;
}

int GSTATE
container::GetNumChildren() const
{
    return m_containerToken->ChildTokenCount;
}

node* GSTATE
container::GetChild(int ChildIdx) const
{
    if (ChildIdx < 0 || ChildIdx >= m_containerToken->ChildTokenCount)
    {
        GS_PreconditionFailed;
        return 0;
    }

    GStateAssert(m_ChildNodes[ChildIdx] && m_ChildNodes[ChildIdx]->GetParent() == this);
    return m_ChildNodes[ChildIdx];
}

int GSTATE
container::GetChildIdx(node* Child) const
{
    if (Child == 0)
        return eInvalidChild;

    {for (int Idx = 0; Idx < m_containerToken->ChildTokenCount; ++Idx)
    {
        if (m_ChildNodes[Idx] == Child)
        {
            GStateAssert(Child->GetParent() == this);
            return Idx;
        }
    }}

    return eInvalidChild;
}

int GSTATE
container::FindChildIdxByName(char const* Name) const
{
    if (!Name)
        return eInvalidChild;

    {for (int Idx = 0; Idx < m_containerToken->ChildTokenCount; ++Idx)
    {
        GStateAssert(m_ChildNodes[Idx]);

        if (_stricmp(m_ChildNodes[Idx]->GetName(), Name) == 0)
            return Idx;
    }}

    return eInvalidChild;
}

node* GSTATE
container::FindChildByName(char const* Name) const
{
    if (!Name)
        return 0;

    int const Idx = FindChildIdxByName(Name);
    if (Idx == eInvalidChild)
        return 0;

    return GetChild(Idx);
}

bool GSTATE
container::MoveNodeToFront(node* Node)
{
    int ChildIdx = GetChildIdx(Node);
    GStateAssert(ChildIdx != eInvalidChild);
    GStateAssert(Node == m_ChildNodes[ChildIdx]);

    TakeTokenOwnership();

    // Move up the existing nodes above this one...
    granny_variant StorToken = m_containerToken->ChildTokens[ChildIdx];
    for (int Idx = ChildIdx; Idx >= 1; --Idx)
    {
        m_containerToken->ChildTokens[Idx] = m_containerToken->ChildTokens[Idx-1];
        m_ChildNodes[Idx] = m_ChildNodes[Idx-1];
    }

    m_containerToken->ChildTokens[0] = StorToken;
    m_ChildNodes[0] = Node;

    return true;
}


node* GSTATE
container::FindChildRecursively(char const* Path) const
{
    if (!Path || !Path[0])
    {
        GStateError("Invalid path in %s: '%s'\n", __FUNCTION__, Path);
        return 0;
    }

    char const* Sep = strchr(Path, '|');
    if (Sep)
    {
        size_t const Length = Sep - Path;
        if (Length == 0)
        {
            // Something like "||" or "|foo", which is invalid
            GStateError("Invalid path in %s: '%s'\n", __FUNCTION__, Path);
            return 0;
        }

        char const* Star = strchr(Path, '*');
        if (Star)
        {
            // Is the star closer than the path sep?
            size_t const DistToStar = Star - Path;
            GStateAssert(DistToStar != Length);
            if (DistToStar < Length)
            {
                // Invalid paths: "f*|" "*ajks|", etc. Only "*|" is valid right here
                if (DistToStar != 0 && Length != 1)
                {
                    GStateError("Invalid path in %s: '%s'\n", __FUNCTION__, Path);
                    return 0;
                }

                // Looking for any container
                {for (int Idx = 0; Idx < m_containerToken->ChildTokenCount; ++Idx)
                {
                    GStateAssert(m_ChildNodes[Idx]);

                    container* Container = GSTATE_DYNCAST(m_ChildNodes[Idx], container);
                    if (Container)
                    {
                        node* Found = Container->FindChildRecursively(Path + Length + 1);

                        // We return the first found node...
                        if (Found)
                            return Found;
                    }
                }}

                // No node found
                return 0;
            }

            // Star is *after* the path sep, fall through to the normal search.
        }

        // We want to operate the search in place without modifying Path
        {for (int Idx = 0; Idx < m_containerToken->ChildTokenCount; ++Idx)
        {
            GStateAssert(m_ChildNodes[Idx]);

            container* Container = GSTATE_DYNCAST(m_ChildNodes[Idx], container);
            if (Container)
            {
                if (_strnicmp(Container->GetName(), Path, Length) == 0)
                {
                    return Container->FindChildRecursively(Path + Length + 1);
                }
            }
        }}

        return 0;
    }
    else
    {
        // No path elements left
        return FindChildByName(Path);
    }
}

int GSTATE
container::AddCopiedChild(node* Child)
{
    return AddNewChild(Child);
}

int GSTATE
container::AddNewChild(node* Child)
{
    GStateAssert(Child->GetParent() == 0);
    GStateAssert(Child->GetTokenContext() == GetTokenContext());

    if (Child == 0)
    {
        GS_InvalidCodePath("adding null child?");
        return eInvalidChild;
    }

    // if (Child already present)
    //   assert
    //   return index of child
    int ExistingIdx = GetChildIdx(Child);
    if (ExistingIdx != eInvalidChild)
    {
        GS_InvalidCodePath("Re-adding child");
        GStateAssert(Child->GetParent() == this);

        return ExistingIdx;
    }

    TakeTokenOwnership();

    // Do the m_ChildNodes first so we can use the token count, which isn't updated
    node*& NewChildNode      = QVecPushNewElementNoCount(m_containerToken->ChildTokenCount, m_ChildNodes);
    granny_variant& NewToken = QVecPushNewElement(m_containerToken->ChildTokenCount, m_containerToken->ChildTokens);

    // Set the new entries
    if (!Child->GetTypeAndToken(&NewToken))
    {
        GS_InvalidCodePath("oooh, that's bad, couldn't get child token");
        return eInvalidChild;
    }
    NewChildNode = Child;

    // Set the parent field
    Child->SetParent(this);

    return m_containerToken->ChildTokenCount - 1;
}

bool GSTATE
container::RemoveChildWithoutFixup(node* Child)
{
    GStateAssert(Child->GetParent() == this);
    GStateAssert(Child->GetTokenContext() == GetTokenContext());

    if (Child == 0)
    {
        GS_InvalidCodePath("adding null child?");
        return false;
    }

    int ExistingIdx = GetChildIdx(Child);
    GStateAssert(ExistingIdx != eInvalidChild);

    return RemoveChildWithoutFixupByIdx(ExistingIdx);
}

bool GSTATE
container::RemoveChild(node* Child)
{
    GStateAssert(Child->GetParent() == this);
    GStateAssert(Child->GetTokenContext() == GetTokenContext());

    if (Child == 0)
    {
        GS_InvalidCodePath("adding null child?");
        return false;
    }

    int ExistingIdx = GetChildIdx(Child);
    GStateAssert(ExistingIdx != eInvalidChild);

    return RemoveChildByIdx(ExistingIdx);
}

bool GSTATE
container::RemoveChildWithoutFixupByIdx(int ChildIdx)
{
    GStateAssert(ChildIdx >= 0 && ChildIdx < m_containerToken->ChildTokenCount);

    TakeTokenOwnership();

    QVecRemoveElementNoCount(ChildIdx, m_containerToken->ChildTokenCount, m_ChildNodes);
    QVecRemoveElement(ChildIdx, m_containerToken->ChildTokenCount, m_containerToken->ChildTokens);
    return true;
}

bool GSTATE
container::RemoveChildByIdx(int ChildIdx)
{
    GStateAssert(ChildIdx >= 0 && ChildIdx < m_containerToken->ChildTokenCount);

    node* RemChild = m_ChildNodes[ChildIdx];
    GStateAssert(RemChild);

    TakeTokenOwnership();

    QVecRemoveElementNoCount(ChildIdx, m_containerToken->ChildTokenCount, m_ChildNodes);
    QVecRemoveElement(ChildIdx, m_containerToken->ChildTokenCount, m_containerToken->ChildTokens);

    // Input/Output handling
    {
        // We need to look at all internal inputs, to see if we refer to this child.
        {for (int Idx = 0; Idx < GetNumInputs(); ++Idx)
        {
            if (IsInputExternal(Idx))
                continue;

            node* Connected;
            granny_int32x ConnectedIdx;
            GetInputConnection(Idx, &Connected, &ConnectedIdx);
            if (Connected == RemChild)
                SetInputConnection(Idx, 0, -1);
        }}

        // And the external inputs of all children to see if they refer to this node
        {for (int ItChild = 0; ItChild < GetNumChildren(); ++ItChild)
        {
            node* Child = GetChild(ItChild);

            {for (int Idx = 0; Idx < Child->GetNumInputs(); ++Idx)
            {
                if (!Child->IsInputExternal(Idx))
                    continue;

                node* Connected;
                granny_int32x ConnectedIdx;
                Child->GetInputConnection(Idx, &Connected, &ConnectedIdx);
                if (Connected == RemChild)
                    Child->SetInputConnection(Idx, 0, -1);
            }}
        }}
    }

    // Delete all transitions from and to this child
    {
        while (RemChild->GetNumTransitions() != 0)
            RemChild->DeleteTransitionByIdx(0);

        {for (int Idx = 0; Idx < m_containerToken->ChildTokenCount; ++Idx)
        {
            GStateAssert(m_ChildNodes[Idx] && m_ChildNodes[Idx] != RemChild);

            node* OtherChild = m_ChildNodes[Idx];
            {for (int OIdx = OtherChild->GetNumTransitions() - 1; OIdx >= 0; --OIdx)
            {
                transition* Transition = OtherChild->GetTransition(OIdx);
                GStateAssert(Transition->GetStartNode() == OtherChild);

                if (Transition->GetEndNode() == RemChild)
                    OtherChild->DeleteTransitionByIdx(OIdx);
            }}
        }}
    }

    // Aaaand one more thing.  Transitions can peek into our children, so we need to give
    // them an opportunity to adjust themselves accordingly.  Note that sync specs can
    // exist not only on *our* transitions, but transitions *to* us, so we really have to
    // dig in our parent's node list a bit to find them.  Since we'll be going through our
    // own transitions at the same time, we can just wait until we encounter ourselves in
    // the parent's child list.
    container* Parent = GetParent();
    if (Parent)
    {
        {for (int SibIdx = 0; SibIdx < Parent->GetNumChildren(); ++SibIdx)
        {
            node* Sibling = Parent->GetChild(SibIdx);

            {for (int Idx = 0; Idx < Sibling->GetNumTransitions(); ++Idx)
            {
                transition* Transition = Sibling->GetTransition(Idx);
                Transition->Note_NodeDelete(RemChild);
                if( Transition->GetDestinationStartNode() == RemChild )
                {
                    Transition->SetDestinationStartNode(NULL);
                }
            }}
        }}

        //Destination start nodes can actually peek down two levels in the case of 
        //blend graphs (they look inside of state machines that are inside of blend 
        //graphs), so check up two levels for transitions that might refere to a node
        //when deleting!
        container* GrandParent = Parent->GetParent();
        if( GrandParent )
        {
            {for (int SibIdx = 0; SibIdx < GrandParent->GetNumChildren(); ++SibIdx)
            {
                node* Sibling = GrandParent->GetChild(SibIdx);

                {for (int Idx = 0; Idx < Sibling->GetNumTransitions(); ++Idx)
                {
                    transition* Transition = Sibling->GetTransition(Idx);
                    if( Transition->GetDestinationStartNode() == RemChild )
                    {
                        Transition->SetDestinationStartNode(NULL);
                    }
                }}
            }}
        }
    }

    // Note that conditionals are notified in state_machine::RemoveChildByIdx

    
    // Clear the parent field
    RemChild->SetParent(0);

    return true;
}


void GSTATE
container::AutoLinkOutputs()
{
    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        container * ChildAsContainer = GSTATE_DYNCAST(GetChild(Idx), container);
        if( ChildAsContainer )
        {
            ChildAsContainer->AutoLinkOutputs();
        }
    }
}

void GSTATE
container::TraverseNode(TraverseNodeFunction TraverseFunction, void * TraverseData)
{
    parent::TraverseNode(TraverseFunction, TraverseData);
    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        GetChild(Idx)->TraverseNode(TraverseFunction, TraverseData);
    }
}


struct node_chain_entry
{
    node const*       Node;
    node_chain_entry* Next;
};

static bool
WalkConnectionsForLoop(node const* Node, node_chain_entry* Chain)
{
    node_chain_entry* Walk = Chain;
    while (Walk)
    {
        if (Walk->Node == Node)
            return true;
        Walk = Walk->Next;
    }

    node_chain_entry ChainHead = { Node, Chain };
    {for (int Idx = 0; Idx < Node->GetNumInputs(); ++Idx)
    {
        if (Node->IsInputInternal(Idx))
            continue;

        node* OtherNode = 0;
        granny_int32x Edge;
        Node->GetInputConnection(Idx, &OtherNode, &Edge);
        if (OtherNode)
        {
            if (WalkConnectionsForLoop(OtherNode, &ChainHead))
                return true;
        }
    }}

    return false;
}


bool GSTATE
container::DetectLoops() const
{
    if (WalkConnectionsForLoop(this, 0))
        return true;

    {for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        if (WalkConnectionsForLoop(GetChild(Idx), 0))
            return true;
    }}

    {for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        container* ContChild = GSTATE_DYNCAST(GetChild(Idx), container);
        if (ContChild && ContChild->DetectLoops())
            return true;
    }}

    return false;
}


bool GSTATE
container::ConnectedInput(granny_int32x  ForOutput,
                          node**         Node,
                          granny_int32x* EdgeIdx)
{
    GStateAssert(Node && EdgeIdx);

    granny_int32x Forward;
    if (!GetOutputForward(ForOutput, Forward) || Forward < 0)
    {
        *Node = 0;
        *EdgeIdx = 0;
        return false;
    }

    GetInputConnection(Forward, Node, EdgeIdx);
    return (*Node != 0);
}


void GSTATE
container::NoteOutputRemoval_Pre(node* AffectedNode, granny_int32x ToBeRemoved)
{
    // This one is a bit tricky.  We need to adjust the input connections of any node that
    // refers to the affected node with an input index >= ToBeRemoved.  Everything will
    // shift down, of course.  We also need to adjust any internal inputs that refer to
    // this node.
    {for (int ChildIdx = 0; ChildIdx < GetNumChildren(); ++ChildIdx)
    {
        node* Child = GetChild(ChildIdx);
        if (Child == AffectedNode)
            continue;

        {for (int Idx = 0; Idx < Child->GetNumInputs(); ++Idx)
        {
            if (Child->IsInputInternal(Idx))
                continue;

            node* OtherNode = 0;
            granny_int32x EdgeIdx = -1;
            Child->GetInputConnection(Idx, &OtherNode, &EdgeIdx);
            if (OtherNode != AffectedNode)
                continue;

            // Disconnect or shift it down?
            if (EdgeIdx == ToBeRemoved)
            {
                Child->SetInputConnection(Idx, 0, -1);
            }
            else if (EdgeIdx > ToBeRemoved)
            {
                Child->SetInputConnection(Idx, OtherNode, EdgeIdx-1);
            }
        }}
    }}

    // Look at our internal inputs next...
    {for (int Idx = 0; Idx < GetNumInputs(); ++Idx)
    {
        if (IsInputExternal(Idx))
            continue;

        node* OtherNode = 0;
        granny_int32x EdgeIdx = -1;
        GetInputConnection(Idx, &OtherNode, &EdgeIdx);
        if (OtherNode != AffectedNode)
            continue;

        // Disconnect or shift it down?
        if (EdgeIdx == ToBeRemoved)
        {
            SetInputConnection(Idx, 0, -1);
        }
        else if (EdgeIdx > ToBeRemoved)
        {
            SetInputConnection(Idx, OtherNode, EdgeIdx-1);
        }
    }}

    // Aaaand one more thing.  Transitions can peek into our children, so we need to give
    // them an opportunity to adjust themselves accordingly.  See the RemoveChildByIdx for
    // more info.
    container* Parent = GetParent();
    if (Parent)
    {
        {for (int SibIdx = 0; SibIdx < Parent->GetNumChildren(); ++SibIdx)
        {
            node* Sibling = Parent->GetChild(SibIdx);

            {for (int Idx = 0; Idx < Sibling->GetNumTransitions(); ++Idx)
            {
                transition* Transition = Sibling->GetTransition(Idx);
                Transition->Note_OutputEdgeDelete(AffectedNode, ToBeRemoved);
            }}
        }}
    }
}


void GSTATE
container::NoteOutputRemoval_Post(node* AffectedNode, bool WasExternal)
{
    // Nop
}

void GSTATE
container::NoteOutputAddition(node* AffectedNode, granny_int32x InsertionIndex)
{
    // nothing to do for now...
    // todo: uh, I don't think this is true.
    // GStateAssert(!"true?  Iff insertionindex == AffectedNode->GetNumOutputs()?");
}

void GSTATE
container::NoteAnimationSlotDeleted(animation_slot* Slot)
{
    parent::NoteAnimationSlotDeleted(Slot);

    for (int Idx = 0; Idx < m_containerToken->ChildTokenCount; ++Idx)
    {
        GStateAssert(m_ChildNodes[Idx] != 0);
        m_ChildNodes[Idx]->NoteAnimationSlotDeleted(Slot);
    }
}


void GSTATE
container::NoteContainerDeleted()
{
    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        container* AsContainer = GSTATE_DYNCAST(GetChild(Idx), container);
        if (AsContainer)
            AsContainer->NoteContainerDeleted();
    }
}

void GSTATE
container::Note_ConditionalDelete(state_machine* Owner, int ConditionalIndex)
{
    // Remove the conditional from any transitions in our children
    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        container* AsContainer = GSTATE_DYNCAST(GetChild(Idx), container);
		if (AsContainer)
            AsContainer->Note_ConditionalDelete(Owner, ConditionalIndex);
    }
}


void GSTATE
container::LinkContainerChildren()
{
    CaptureSiblingData();
    CaptureSiblingLinks();

    state_machine* AsStateMachine = GSTATE_DYNCAST(this, state_machine);
    if( AsStateMachine )
    {
        //Root container kicks off all the hookup now that everthing below it (and including it) is built. 
        // Now, create all of the conditionals.  Note that like
        // container::container we have to handle deleted node types.  See the
        // comment in that function for more info.
        for (int Idx = 0; Idx < AsStateMachine->GetNumConditionals(); Idx++)
        {
            // All of the nodes referred to should be valid at this point...
            AsStateMachine->GetConditional(Idx)->CaptureNodeLinks();
        }
    }

    for (int Idx = 0; Idx < m_containerToken->ChildTokenCount; ++Idx)
    {
        GStateAssert(m_ChildNodes[Idx] != 0);

        container* AsContainer = GSTATE_DYNCAST(m_ChildNodes[Idx], container);
        if( AsContainer )
        {
            AsContainer->LinkContainerChildren();
        }
        else
        {
            m_ChildNodes[Idx]->CaptureSiblingData();
            m_ChildNodes[Idx]->CaptureSiblingLinks();
        }
    }
}

granny_int32x GSTATE
container::AddInput(node_edge_type EdgeType, char const* EdgeName)
{
    if (!EdgeName || !EdgeName[0])
        return -1;

    granny_int32x ExternalInput  = node::AddInputImpl(EdgeType, EdgeName);
    granny_int32x InternalOutput = node::AddOutputImpl(EdgeType, EdgeName);

    // Now we need to mark that as an internal output, and the input that it refers to
    SetOutputInternal(InternalOutput, ExternalInput);

    return ExternalInput;
}

bool GSTATE
container::DeleteInput(granny_int32x InputIndex)
{
    if (IsInputExternal(InputIndex))
    {
        // Delete the corresponding internal output
        granny_int32x ForwardTo;
        if (!GetInputForward(InputIndex, ForwardTo))
        {
            GS_PreconditionFailed;
            return false;
        }

        DeleteOutput(ForwardTo);
    }

    return parent::DeleteInputImpl(InputIndex);
}

granny_int32x GSTATE
container::AddOutput(node_edge_type EdgeType, char const* EdgeName)
{
    if (!EdgeName || !EdgeName[0])
        return false;

    granny_int32x ExternalOutput = node::AddOutputImpl(EdgeType, EdgeName);
    granny_int32x InternalInput  = node::AddInputImpl(EdgeType, EdgeName);

    // Now we need to mark that as an internal output, and the input that it refers to
    SetInputInternal(InternalInput, ExternalOutput);

    return ExternalOutput;
}

bool GSTATE
container::DeleteOutput(granny_int32x OutputIndex)
{
    if (IsOutputExternal(OutputIndex))
    {
        // Delete the corresponding internal input
        granny_int32x ForwardTo;
        if (!GetOutputForward(OutputIndex, ForwardTo))
        {
            GS_PreconditionFailed;
            return false;
        }

        DeleteInput(ForwardTo);
    }

    if (!parent::DeleteOutputImpl(OutputIndex))
    {
        // this will not end well.
        return false;
    }


    // Disconnect and adjust any children that refer to this output
    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        node* Child = GetChild(Idx);
        for (int InputIdx = 0; InputIdx < Child->GetNumInputs(); ++InputIdx)
        {
            INPUT_CONNECTION_ON(Child, InputIdx, Test);
            if (TestNode == this)
            {
                if (TestEdge == OutputIndex)
                    Child->SetInputConnection(InputIdx, 0, -1);
                else if (TestEdge > OutputIndex)
                    Child->SetInputConnection(InputIdx, this, TestEdge-1);
            }
        }
    }

    // As well as internal inputs
    for (int InputIdx = 0; InputIdx < GetNumInputs(); ++InputIdx)
    {
        if (IsInputExternal(InputIdx))
            continue;

        INPUT_CONNECTION(InputIdx, Test);
        if (TestNode == this)
        {
            if (TestEdge == OutputIndex)
                SetInputConnection(InputIdx, 0, -1);
            else if (TestEdge > OutputIndex)
                SetInputConnection(InputIdx, this, TestEdge-1);
        }
    }

    return true;
}


bool GSTATE
container::SetInputName(granny_int32x InputIdx, char const* NewEdgeName)
{
    if (parent::SetInputName(InputIdx, NewEdgeName) == false)
        return false;

    if (IsInputInternal(InputIdx))
        return true;

    // change the name of the internal correspondent...
    granny_int32x ForwardTo;
    if (!GetInputForward(InputIdx, ForwardTo) || ForwardTo < 0)
        return false;

    return SetOutputName(ForwardTo, NewEdgeName);
}

bool GSTATE
container::SetOutputName(granny_int32x OutputIdx, char const* NewEdgeName)
{
    if (parent::SetOutputName(OutputIdx, NewEdgeName) == false)
        return false;

    if (IsOutputInternal(OutputIdx))
        return true;

    // change the name of the internal correspondent...
    granny_int32x ForwardTo;
    if (!GetOutputForward(OutputIdx, ForwardTo) || ForwardTo < 0)
        return false;

    return SetInputName(ForwardTo, NewEdgeName);
}


bool GSTATE
container::CheckConnections()
{
    if (!parent::CheckConnections())
        return false;

    // All of our internal inputs must reference either a child or our own internal
    // outputs.  Note that the type of the connection is already checked in node::
    for (int Idx = 0; Idx < GetNumInputs(); ++Idx)
    {
        if (IsInputExternal(Idx))
            continue;

        INPUT_CONNECTION(Idx, Test);
        if (TestNode == this)
        {
            if (!IsOutputInternal(TestEdge))
            {
                GS_InvalidCodePath("Connected to self, must be an internal output");
                return false;
            }
        }
        else if (TestNode != 0)
        {
            if (GetChildIdx(TestNode) == eInvalidChild || !TestNode->IsOutputExternal(TestEdge))
            {
                GS_InvalidCodePath("Must be connected to an external output of a child");
                return false;
            }
        }
    }
    
    // All of our children's external inputs must either reference one of our internal
    // outputs, or an external output of a child.  This also passes the CheckConnections
    // call downwards...
    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        node* Child = GetChild(Idx);
        if (!Child->CheckConnections())
            return false;

        for (int ChildInput = 0; ChildInput < Child->GetNumInputs(); ++ChildInput)
        {
            if (Child->IsInputInternal(ChildInput))
                continue;

            INPUT_CONNECTION_ON(Child, ChildInput, Test);
            if (TestNode == this)
            {
                if (!IsOutputInternal(TestEdge))
                {
                    GS_InvalidCodePath("Connected to container, must be an internal output");
                    return false;
                }
            }
            else if (TestNode != 0)
            {
                if (GetChildIdx(TestNode) == eInvalidChild || !TestNode->IsOutputExternal(TestEdge))
                {
                    GS_InvalidCodePath("Must be connected to an external output of a sibling");
                    return false;
                }
            }
        }
    }

    return true;
}


int GSTATE
container::GetNumComments()
{
    GStateAssert(m_containerToken->CommentCount >= 0);

    return m_containerToken->CommentCount;
}

int GSTATE
container::AddComment(char const* InitialText,
                      granny_int32x PosX, granny_int32x PosY,
                      granny_int32x SizeX, granny_int32x SizeY)
{
    GStateCheckPtrNotNULL(InitialText, return -1);

    TakeTokenOwnership();

    editing_comment& NewComment = QVecPushNewElement(m_containerToken->CommentCount,
                                                     m_containerToken->Comments);

    GStateCloneString(NewComment.Text, InitialText);
    NewComment.PosX  = PosX;
    NewComment.PosY  = PosY;
    NewComment.SizeX = SizeX;
    NewComment.SizeY = SizeY;

    return (m_containerToken->CommentCount - 1);
}

void GSTATE
container::DeleteComment(granny_int32x CommentIdx)
{
    GStateCheckIndex(CommentIdx, m_containerToken->CommentCount, return);

    TakeTokenOwnership();

    GStateDeallocate(m_containerToken->Comments[CommentIdx].Text);
    QVecRemoveElement(CommentIdx,
                      m_containerToken->CommentCount,
                      m_containerToken->Comments);
}

bool GSTATE
container::MoveCommentToFront(granny_int32x CommentIdx)
{
    GStateCheckIndex(CommentIdx, m_containerToken->CommentCount, return false);

    TakeTokenOwnership();

    // Move up the existing nodes above this one...
    editing_comment StorComment = m_containerToken->Comments[CommentIdx];
    for (int Idx = CommentIdx; Idx >= 1; --Idx)
        m_containerToken->Comments[Idx] = m_containerToken->Comments[Idx-1];

    m_containerToken->Comments[0] = StorComment;

    return true;
}

char const* GSTATE
container::GetCommentText(granny_int32x CommentIdx)
{
    GStateCheckIndex(CommentIdx, m_containerToken->CommentCount, return 0);

    return m_containerToken->Comments[CommentIdx].Text;
}

bool GSTATE
container::SetCommentText(granny_int32x CommentIdx, char const* NewText)
{
    GStateCheckIndex(CommentIdx, m_containerToken->CommentCount, return false);
    GStateCheckPtrNotNULL(NewText, return false);

    TakeTokenOwnership();

    GStateReplaceString(m_containerToken->Comments[CommentIdx].Text, NewText);
    return true;
}

bool GSTATE
container::GetCommentPosition(granny_int32x CommentIdx,
                              granny_int32x* PosX,
                              granny_int32x* PosY,
                              granny_int32x* SizeX,
                              granny_int32x* SizeY)
{
    GStateCheckIndex(CommentIdx, m_containerToken->CommentCount, return false);
    GStateCheckPtrNotNULL(PosX, return false);
    GStateCheckPtrNotNULL(PosY, return false);
    GStateCheckPtrNotNULL(SizeX, return false);
    GStateCheckPtrNotNULL(SizeY, return false);

    *PosX  = m_containerToken->Comments[CommentIdx].PosX;
    *PosY  = m_containerToken->Comments[CommentIdx].PosY;
    *SizeX = m_containerToken->Comments[CommentIdx].SizeX;
    *SizeY = m_containerToken->Comments[CommentIdx].SizeY;
    
    return true;
}

bool GSTATE
container::SetCommentPosition(granny_int32x CommentIdx,
                              granny_int32x PosX,
                              granny_int32x PosY,
                              granny_int32x SizeX,
                              granny_int32x SizeY)
{
    GStateCheckIndex(CommentIdx, m_containerToken->CommentCount, return false);

    m_containerToken->Comments[CommentIdx].PosX  = PosX;
    m_containerToken->Comments[CommentIdx].PosY  = PosY;
    m_containerToken->Comments[CommentIdx].SizeX = SizeX;
    m_containerToken->Comments[CommentIdx].SizeY = SizeY;
    
    return true;
}

void GSTATE
container::RemapAnimationSlots(animation_slot** NewSlots, int NewSlotCount)
{
    for (int Idx = 0; Idx < GetNumChildren(); ++Idx)
    {
        m_ChildNodes[Idx]->RemapAnimationSlots(NewSlots, NewSlotCount);
    }
}

//IsNodeActive determines if the Node is active within it's parent container, and also every ancestor of Node up 
//until the caller (this) is also active.
//Notes: 
//* Potential enhancement: Every node in a blend_graph is considered 'active' even though in reality we may not 
//  want to consider nodes with zero weight, or inactive in other data-driven ways, as active. 
//* If the current active item is a transition, this will succeed if Node is a start or end node in that transition
//
bool GSTATE
container::IsNodeActive(node* Node)
{
    container * Parent = Node->GetParent();
    if( Parent == NULL )
    {
        //Root container is always active. 
        return true;
    }

    state_machine* ParentStateMachine = GSTATE_DYNCAST(Parent, state_machine);
    if( ParentStateMachine )
    {
        //State machines can contain active and inactive nodes
        //The active item can also be a transition (in which case there are two
        //active nodes (start and end). For events there are rules about
        //whether to sample the start or end node, but for this we're just going to
        //check both nodes for the loop while the transition is active. 
        tokenized * Active = ParentStateMachine->GetActiveElement();

        bool WasLocallyActive = false;

        if( Node == Active )
        {
            WasLocallyActive = true;
        }
        else
        {
            transition * AsTransition = GSTATE_DYNCAST(Active, transition);
            if( AsTransition != NULL && (Node == AsTransition->GetStartNode() || Node == AsTransition->GetEndNode()) )
            {
                WasLocallyActive = true;
            }
        }

        //If Node is inactive in its container, then it's inactive. 
        //Even if it is active though, it's parent container may be inactive,
        //so we have to walk up the ancestors and check them all. 
        if( !WasLocallyActive )
        {
            return false;
        }

        if( Node == this )
        {
            //All the nodes from the base up to the caller were active, so we're active
            return true;
        }
    }

    blend_graph* ParentBlendGraph = GSTATE_DYNCAST(Parent, blend_graph);
    if( ParentBlendGraph )
    {
        //A node in a state machine is always active (actually maybe not
        //really true (see note in function comment above), but for now 
        //we're going to make that assumption. Just return true if we hit
        //the highest container. 
        if( Node == this )
        {
            //All the nodes from the base up to the caller were active, so we're active
            return true;
        }
    }

    //Check if Parent is active in it's container. 
    return IsNodeActive(Parent);
}
