// ========================================================================
// $Notice: $
// ========================================================================
#include "gstate_event_renamer.h"

#include "gstate_anim_utils.h"
#include "gstate_node_visitor.h"
#include "gstate_quick_vecs.h"
#include "gstate_token_context.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE event_renamer::event_renamerImpl
{
    granny_int32 RenameCount;  // %2 == 0
    char**       Renames;
};

granny_data_type_definition GSTATE
event_renamer::event_renamerImplType[] =
{
    { GrannyReferenceToArrayMember, "Renames", GrannyStringType },
    { GrannyEndMember },
};

// event_renamer is a concrete class, so we must create a slotted container
struct event_renamer_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(event_renamer);
};

granny_data_type_definition event_renamer::event_renamerTokenType[] =
{
    DECL_UID_MEMBER(event_renamer),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(event_renamer),

    { GrannyEndMember }
};

void GSTATE
event_renamer::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(event_renamer);

    // When we take token ownership, future code will assume that it needs to free our
    // owned pointers when they change.  Don't disappoint them.
    GStateCloneArray(m_event_renamerToken->Renames,
                     OldToken->Renames,
                     m_event_renamerToken->RenameCount);
    {for (int Idx = 0; Idx < m_event_renamerToken->RenameCount; ++Idx)
    {
        GStateCloneString(m_event_renamerToken->Renames[Idx], OldToken->Renames[Idx]);
    }}
}

void GSTATE
event_renamer::ReleaseOwnedToken_event_renamer()
{
    {for (int Idx = 0; Idx < m_event_renamerToken->RenameCount; ++Idx)
    {
        GStateDeallocate(m_event_renamerToken->Renames[Idx]);
    }}
    GStateDeallocate(m_event_renamerToken->Renames);
}

IMPL_CREATE_NODE_BOILERPLATE(event_renamer);

GSTATE
event_renamer::event_renamer(token_context*               Context,
                             granny_data_type_definition* TokenType,
                             void*                        TokenObject,
                             token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership)
{
    IMPL_INIT_FROM_TOKEN(event_renamer);

    if (EditorCreated())
    {
        // Add our default input/outputs
        AddOutputImpl(EventEdge, "Events");
        AddInputImpl(EventEdge, "Events");
    }
}


GSTATE
event_renamer::~event_renamer()
{
    DTOR_RELEASE_TOKEN(event_renamer);
}


bool GSTATE
event_renamer::FillDefaultToken(granny_data_type_definition* TokenType,
                                void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares event_renamerImpl*& Slot = // member
    GET_TOKEN_SLOT(event_renamer);

    // Our slot in this token should be empty.
    // Create a new event renamer Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}

granny_int32x GSTATE
event_renamer::NumRenames()
{
    GStateAssert((m_event_renamerToken->RenameCount % 2) == 0);
    return m_event_renamerToken->RenameCount / 2;
}

void GSTATE
event_renamer::AddRename(char const* NameInput,
                         char const* NameOutput)
{
    TakeTokenOwnership();

    QVecPushNewElement(m_event_renamerToken->RenameCount,
                       m_event_renamerToken->Renames);
    QVecPushNewElement(m_event_renamerToken->RenameCount,
                       m_event_renamerToken->Renames);

    char*& NewInput  = m_event_renamerToken->Renames[m_event_renamerToken->RenameCount - 2];
    char*& NewOutput = m_event_renamerToken->Renames[m_event_renamerToken->RenameCount - 1];

    GStateCloneString(NewInput,  NameInput);
    GStateCloneString(NewOutput, NameOutput);
}

void GSTATE
event_renamer::DeleteRename(granny_int32x Index)
{
    GStateAssert(GS_InRange(Index, m_event_renamerToken->RenameCount));

    TakeTokenOwnership();

    GStateDeallocate(m_event_renamerToken->Renames[Index*2 + 0]);
    GStateDeallocate(m_event_renamerToken->Renames[Index*2 + 1]);
    QVecRemoveElement(Index*2,
                      m_event_renamerToken->RenameCount,
                      m_event_renamerToken->Renames);
    QVecRemoveElement(Index*2,
                      m_event_renamerToken->RenameCount,
                      m_event_renamerToken->Renames);
}

char const* GSTATE
event_renamer::GetRenameInput(granny_int32x Index)
{
    granny_int32x RealIndex = Index*2 + 0;
    GStateAssert(GS_InRange(RealIndex, m_event_renamerToken->RenameCount));

    return m_event_renamerToken->Renames[RealIndex];
}

char const* GSTATE
event_renamer::GetRenameOutput(granny_int32x Index)
{
    granny_int32x RealIndex = Index*2 + 1;
    GStateAssert(GS_InRange(RealIndex, m_event_renamerToken->RenameCount));

    return m_event_renamerToken->Renames[RealIndex];
}


void GSTATE
event_renamer::SetRename(granny_int32x Index,
                         char const* NameInput,
                         char const* NameOutput)
{
    GStateAssert(GS_InRange(Index, m_event_renamerToken->RenameCount));

    TakeTokenOwnership();
    GStateReplaceString(m_event_renamerToken->Renames[Index*2 + 0], NameInput);
    GStateReplaceString(m_event_renamerToken->Renames[Index*2 + 1], NameOutput);
}

inline static void
ReplaceText(gstate_text_track_entry& Entry,
            char const** Renames,
            granny_int32x RenameCount)
{
    GStateAssert((RenameCount%2) == 0);

    for (int Idx = 0; Idx < RenameCount; Idx += 2)
    {
        if (Renames[Idx] && strcmp(Entry.Text, Renames[Idx]) == 0)
        {
            Entry.Text = Renames[Idx + 1];
            break;
        }
    }
}

bool GSTATE
event_renamer::SampleEventOutput(granny_int32x            OutputIdx,
                                 granny_real32            AtT,
                                 granny_real32            DeltaT,
                                 gstate_text_track_entry* EventBuffer,
                                 granny_int32x const      EventBufferSize,
                                 granny_int32x*           NumEvents)
{
    GStateAssert(OutputIdx == 0);
    GStateCheckPtrNotNULL(EventBuffer, return false);
    GStateCheckPtrNotNULL(NumEvents, return false);

    node* Node;
    granny_int32x EdgeIdx;
    GetInputConnection(0, &Node, &EdgeIdx);
    if (Node == 0)
    {
        *NumEvents = 0;
        return true;
    }

    if (!Node->SampleEventOutput(EdgeIdx, AtT, DeltaT, EventBuffer, EventBufferSize, NumEvents))
    {
        return false;
    }

    // O(m*n): awesome!
    for (int EventIdx = 0; EventIdx < *NumEvents; ++EventIdx)
    {
        ReplaceText(EventBuffer[EventIdx],
                    (char const**)m_event_renamerToken->Renames,
                    m_event_renamerToken->RenameCount);
    }

    return true;
}

bool GSTATE
event_renamer::GetNextEvent(granny_int32x OutputIdx,
                            granny_real32 AtT,
                            gstate_text_track_entry*  Event)
{
    GStateAssert(OutputIdx == 0);
    GStateCheckPtrNotNULL(Event, return false);

    node* Node;
    granny_int32x EdgeIdx;
    GetInputConnection(0, &Node, &EdgeIdx);
    if (Node == 0)
        return false;

    if (Node->GetNextEvent(EdgeIdx, AtT, Event) == false)
        return false;

    ReplaceText(*Event,
                (char const**)m_event_renamerToken->Renames,
                m_event_renamerToken->RenameCount);

    return true;
}


// Returns all possible events on this edge
bool GSTATE
event_renamer::GetAllEvents(granny_int32x            OutputIdx,
                            gstate_text_track_entry*  EventBuffer,
                            granny_int32x const      EventBufferSize,
                            granny_int32x*           NumEvents)
{
    GStateAssert(OutputIdx == 0);
    GStateCheckPtrNotNULL(EventBuffer, return false);
    GStateCheckPtrNotNULL(NumEvents, return false);

    node* Node;
    granny_int32x EdgeIdx;
    GetInputConnection(0, &Node, &EdgeIdx);
    if (Node == 0)
    {
        *NumEvents = 0;
        return true;
    }

    if (Node->GetAllEvents(EdgeIdx, EventBuffer, EventBufferSize, NumEvents) == false)
        return false;


    for (int Idx = 0; Idx < *NumEvents; ++Idx)
    {
        ReplaceText(EventBuffer[Idx],
                    (char const**)m_event_renamerToken->Renames,
                    m_event_renamerToken->RenameCount);
    }

    return true;
}


bool GSTATE
event_renamer::GetCloseEventTimes(granny_int32x  OutputIdx,
                                  granny_real32  AtT,
                                  char const*    TextToFind,
                                  granny_real32* PreviousTime,
                                  granny_real32* NextTime)
{
    GStateAssert(OutputIdx == 0);

    node* Node;
    granny_int32x EdgeIdx;
    GetInputConnection(0, &Node, &EdgeIdx);
    if (Node == 0)
    {
        return false;
    }

    for (int Idx = 0; Idx < m_event_renamerToken->RenameCount; Idx += 2)
    {
        // Note that we're going backwards here...
        if (strcmp(m_event_renamerToken->Renames[Idx + 1], TextToFind) == 0)
        {
            TextToFind = m_event_renamerToken->Renames[Idx];
            break;
        }
    }

    return Node->GetCloseEventTimes(EdgeIdx, AtT, TextToFind, PreviousTime, NextTime);
}

granny_int32x GSTATE
event_renamer::GetOutputPassthrough(granny_int32x OutputIdx) const
{
    return OutputIdx;
}

