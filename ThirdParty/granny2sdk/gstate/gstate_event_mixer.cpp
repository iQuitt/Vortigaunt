// ========================================================================
// $Notice: $
// ========================================================================
#include "gstate_event_mixer.h"

#include "gstate_anim_utils.h"
#include "gstate_node_visitor.h"
#include "gstate_quick_vecs.h"
#include "gstate_token_context.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE event_mixer::event_mixerImpl
{
    granny_int32 DummyMember;
};

granny_data_type_definition GSTATE
event_mixer::event_mixerImplType[] =
{
    { GrannyInt32Member, "DummyMember" },
    { GrannyEndMember },
};

// event_mixer is a concrete class, so we must create a slotted container
struct event_mixer_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(event_mixer);
};

granny_data_type_definition event_mixer::event_mixerTokenType[] =
{
    DECL_UID_MEMBER(event_mixer),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(event_mixer),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(event_mixer);
IMPL_CREATE_NODE_BOILERPLATE(event_mixer);

GSTATE
event_mixer::event_mixer(token_context*               Context,
                         granny_data_type_definition* TokenType,
                         void*                        TokenObject,
                         token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_event_mixerToken(0)
{
    IMPL_INIT_FROM_TOKEN(event_mixer);

    if (EditorCreated())
    {
        // Add our default input/outputs (safe to route to parent, impl disallows certain manips)
        AddInputImpl(EventEdge,  "Events");
        AddInputImpl(EventEdge,  "Events");
        AddOutputImpl(EventEdge, "Combined");
    }
}


GSTATE
event_mixer::~event_mixer()
{
    DTOR_RELEASE_TOKEN(event_mixer);
}


bool GSTATE
event_mixer::FillDefaultToken(granny_data_type_definition* TokenType,
                              void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares event_mixerImpl*& Slot = // member
    GET_TOKEN_SLOT(event_mixer);

    // Our slot in this token should be empty.
    // Create a new event mixer Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}

granny_int32x GSTATE
event_mixer::AddInput(node_edge_type EdgeType, char const* EdgeName)
{
    if (EdgeType != EventEdge)
        return -1;

    return parent::AddInputImpl(EdgeType, EdgeName);
}


bool GSTATE
event_mixer::DeleteInput(granny_int32x InputIndex)
{
    if (InputIndex < 0 || InputIndex >= GetNumInputs())
        return false;

    if (GetNumInputs() <= 2)
        return false;

    return parent::DeleteInputImpl(InputIndex);
}

granny_int32x GSTATE
event_mixer::AddOutput(node_edge_type EdgeType, char const* EdgeName)
{
    return -1;
}

bool GSTATE
event_mixer::DeleteOutput(granny_int32x OutputIdx)
{
    return false;
}


bool GSTATE
event_mixer::SampleEventOutput(granny_int32x            OutputIdx,
                               granny_real32            AtT,
                               granny_real32            DeltaT,
                               gstate_text_track_entry* EventBuffer,
                               granny_int32x const      EventBufferSize,
                               granny_int32x*           NumEvents)
{
    GStateAssert(OutputIdx == 0);
    GStateCheckPtrNotNULL(EventBuffer, return false);
    GStateCheckPtrNotNULL(NumEvents, return false);

    granny_int32x CurrIdx = 0;
    for (int InputIdx = 0; InputIdx < GetNumInputs(); ++InputIdx)
    {
        INPUT_CONNECTION(InputIdx, Event);
        if (EventNode)
        {
            granny_int32x UsedEvents = 0;
            if (!EventNode->SampleEventOutput(EventEdge, AtT, DeltaT,
                                              EventBuffer + CurrIdx,
                                              EventBufferSize - CurrIdx,
                                              &UsedEvents))
            {
                *NumEvents = -1;
                return false;
            }

            CurrIdx += UsedEvents;
        }
    }

    // Sort the events into timestamp order
    SortEventsByTimestamp(EventBuffer, CurrIdx);
    *NumEvents = CurrIdx;

    return true;
}

bool GSTATE
event_mixer::GetNextEvent(granny_int32x OutputIdx,
                          granny_real32 AtT,
                          gstate_text_track_entry*  Event)
{
    GStateAssert(OutputIdx == 0);
    GStateCheckPtrNotNULL(Event, return false);

    bool Found = false;
    for (int InputIdx = 0; InputIdx < GetNumInputs(); ++InputIdx)
    {
        INPUT_CONNECTION(InputIdx, Event);
        if (EventNode)
        {
            gstate_text_track_entry Proposed;
            if (EventNode->GetNextEvent(EventEdge, AtT, &Proposed))
            {
                if (!Found)
                {
                    *Event = Proposed;
                    Found = true;
                }
                else
                {
                    if (Proposed.TimeStamp < Event->TimeStamp)
                        *Event = Proposed;
                }
            }
        }
    }
    
    return Found;
}

// Returns all possible events on this edge
bool GSTATE
event_mixer::GetAllEvents(granny_int32x            OutputIdx,
                          gstate_text_track_entry*  EventBuffer,
                          granny_int32x const      EventBufferSize,
                          granny_int32x*           NumEvents)
{
    GStateAssert(OutputIdx == 0);
    GStateCheckPtrNotNULL(EventBuffer, return false);
    GStateCheckPtrNotNULL(NumEvents, return false);

    granny_int32x CurrIdx = 0;
    for (int InputIdx = 0; InputIdx < GetNumInputs(); ++InputIdx)
    {
        INPUT_CONNECTION(InputIdx, Event);
        if (EventNode)
        {
            granny_int32x UsedEvents = 0;
            if (!EventNode->GetAllEvents(EventEdge,
                                         EventBuffer + CurrIdx,
                                         EventBufferSize - CurrIdx,
                                         &UsedEvents))
            {
                *NumEvents = -1;
                return false;
            }

            CurrIdx += UsedEvents;
        }
    }

    // Sort the events into timestamp order
    SortEventsByTimestamp(EventBuffer, CurrIdx);
    *NumEvents = CurrIdx;

    return true;
}

bool GSTATE
event_mixer::GetCloseEventTimes(granny_int32x  OutputIdx,
                                granny_real32  AtT,
                                char const*    TextToFind,
                                granny_real32* PreviousTime,
                                granny_real32* NextTime)
{
    GStateAssert(OutputIdx == 0);

    bool Found = false;
    for (int InputIdx = 0; InputIdx < GetNumInputs(); ++InputIdx)
    {
        INPUT_CONNECTION(InputIdx, Event);
        if (EventNode)
        {
            granny_real32 Prev, Next;
            if (EventNode->GetCloseEventTimes(EventEdge, AtT, TextToFind, &Prev, &Next))
            {
                if (!Found)
                {
                    *PreviousTime = Prev;
                    *NextTime     = Next;
                    Found = true;
                }
                else
                {
                    if (*PreviousTime < Prev)
                        *PreviousTime = Prev;

                    if (*NextTime > Next)
                        *NextTime = Next;
                }
            }
        }
    }

    return Found;
}

