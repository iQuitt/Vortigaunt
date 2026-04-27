// ========================================================================
// $Notice: $
// ========================================================================
#include "gstate_event_source.h"

// #include "gstate_anim_utils.h"
#include "gstate_node_visitor.h"
#include "gstate_quick_vecs.h"
#include "gstate_state_machine.h"
#include "gstate_token_context.h"
#include "gstate_sample_recorder.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

const int GSTATE event_source::sm_MinQueueSize = 4;


struct GSTATE event_source::event_sourceImpl
{
    granny_int32 PossibleEventCount;
    char**       PossibleEvents;
};

granny_data_type_definition GSTATE
event_source::event_sourceImplType[] =
{
    { GrannyReferenceToArrayMember, "PossibleEvents", GrannyStringType },
    { GrannyEndMember },
};

// event_source is a concrete class, so we must create a slotted container
struct event_source_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(event_source);
};

granny_data_type_definition event_source::event_sourceTokenType[] =
{
    DECL_UID_MEMBER(event_source),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(event_source),

    { GrannyEndMember }
};

void GSTATE
event_source::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(event_source);

    // When we take token ownership, future code will assume that it needs to free our
    // owned pointers when they change.  Don't disappoint them.
    GStateCloneArray(m_event_sourceToken->PossibleEvents,
                     OldToken->PossibleEvents,
                     m_event_sourceToken->PossibleEventCount);
    {for (int Idx = 0; Idx < m_event_sourceToken->PossibleEventCount; ++Idx)
    {
        GStateCloneString(m_event_sourceToken->PossibleEvents[Idx], OldToken->PossibleEvents[Idx]);
    }}

    // Always zero this out, since we could have changed something critical here.
    m_NumEventsQueued   = 0;
    m_NumEventsProduced = 0;
    m_ProducedEventsAt  = -1;
}

void GSTATE
event_source::ReleaseOwnedToken_event_source()
{
    for (int Idx = 0; Idx < m_event_sourceToken->PossibleEventCount; ++Idx)
    {
        GStateDeallocate(m_event_sourceToken->PossibleEvents[Idx]);
    }
    GStateDeallocate(m_event_sourceToken->PossibleEvents);
}

IMPL_CREATE_NODE_BOILERPLATE(event_source);

GSTATE
event_source::event_source(token_context*               Context,
                           granny_data_type_definition* TokenType,
                           void*                        TokenObject,
                           token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_QueuedEvents(0),
    m_NumQueueSlots(0),
    m_NumEventsQueued(0),
    m_ProducedEventsAt(-1),
    m_ProducedEvents(0),
    m_NumEventsProduced(0)
{
    IMPL_INIT_FROM_TOKEN(event_source);

    if (EditorCreated())
    {
        // Add our default input/outputs
        AddOutputImpl(EventEdge, "Events");
    }

    if (m_event_sourceToken->PossibleEventCount >= sm_MinQueueSize)
        m_NumQueueSlots = m_event_sourceToken->PossibleEventCount;
    else
        m_NumQueueSlots = sm_MinQueueSize;

    GStateAllocZeroedArray(m_QueuedEvents, m_NumQueueSlots);
    m_NumEventsQueued = 0;

    GStateAllocZeroedArray(m_ProducedEvents, m_NumQueueSlots);
    m_NumEventsProduced = 0;

}


GSTATE
event_source::~event_source()
{
    DTOR_RELEASE_TOKEN(event_source);
    GStateDeallocate(m_QueuedEvents);
    GStateDeallocate(m_ProducedEvents);
}


bool GSTATE
event_source::FillDefaultToken(granny_data_type_definition* TokenType,
                              void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares event_sourceImpl*& Slot = // member
    GET_TOKEN_SLOT(event_source);

    // Our slot in this token should be empty.
    // Create a new event source Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}

bool GSTATE
event_source::SetOutputName(granny_int32x OutputIdx,
                            char const* NewEdgeName)
{
    if (!parent::SetOutputName(OutputIdx, NewEdgeName))
        return false;

    state_machine* ParentSM = GSTATE_DYNCAST(GetParent(), state_machine);
    if (ParentSM)
        ParentSM->NoteParameterNameChange(this, OutputIdx);

    return true;
}

bool GSTATE
event_source::SetName(char const* NewName)
{
    if (parent::SetName(NewName) == false)
        return false;

    //APTODO: really?!?!?
    //SetOutputName(0, NewName);
    return true;
}

granny_int32x GSTATE
event_source::NumPossibleEvents()
{
    return m_event_sourceToken->PossibleEventCount;
}

void GSTATE
event_source::AddPossibleEvent(char const* EventString)
{
    TakeTokenOwnership();

    char*& NewEvent = QVecPushNewElement(m_event_sourceToken->PossibleEventCount,
                                         m_event_sourceToken->PossibleEvents);
    GStateCloneString(NewEvent, EventString);

    if (m_NumQueueSlots < m_event_sourceToken->PossibleEventCount)
    {
        GStateDeallocate(m_QueuedEvents);
        GStateDeallocate(m_ProducedEvents);

        m_NumQueueSlots = m_event_sourceToken->PossibleEventCount;
        GStateAllocZeroedArray(m_QueuedEvents, m_NumQueueSlots);
        GStateAllocZeroedArray(m_ProducedEvents, m_NumQueueSlots);
        m_NumEventsQueued = 0;
        m_NumEventsProduced = 0;
    }
}

void GSTATE
event_source::DeletePossibleEvent(granny_int32x Index)
{
    GStateAssert(GS_InRange(Index, m_event_sourceToken->PossibleEventCount));

    TakeTokenOwnership();

    GStateDeallocate(m_event_sourceToken->PossibleEvents[Index]);
    QVecRemoveElement(Index,
                      m_event_sourceToken->PossibleEventCount,
                      m_event_sourceToken->PossibleEvents);
}

char const* GSTATE
event_source::GetPossibleEvent(granny_int32x Index)
{
    GStateAssert(GS_InRange(Index, m_event_sourceToken->PossibleEventCount));
    return m_event_sourceToken->PossibleEvents[Index];
}

void GSTATE
event_source::SetPossibleEvent(granny_int32x Index, char const* NewString)
{
    GStateAssert(GS_InRange(Index, m_event_sourceToken->PossibleEventCount));

    TakeTokenOwnership();
    GStateReplaceString(m_event_sourceToken->PossibleEvents[Index], NewString);
}

bool GSTATE
event_source::PokeEvent(char const* Event)
{
    for (int Idx = 0; Idx < m_event_sourceToken->PossibleEventCount; ++Idx)
    {
        if (_stricmp(Event, m_event_sourceToken->PossibleEvents[Idx]) == 0)
            return PokeEventByIdx(Idx);
    }

    return false;
}

bool GSTATE
event_source::PokeEventByIdx(granny_int32x EventIdx)
{
    GStateAssert(m_NumEventsQueued <= m_NumQueueSlots);
    if (m_NumEventsQueued >= m_NumQueueSlots)
    {
        GStateWarning("Event registration caused window slide and event loss, is the node connected? (%s)",
                      GetName());

        // Well, that's really bad, no slide possible
        if (m_NumQueueSlots <= 0)
        {
            GStateError("Event poke with no available slots.");
            return false;
        }

        memmove(m_QueuedEvents, m_QueuedEvents+1, sizeof(m_QueuedEvents[0])*(m_NumQueueSlots-1));
        m_NumEventsQueued = m_NumQueueSlots - 1;
    }

    m_QueuedEvents[m_NumEventsQueued] = m_event_sourceToken->PossibleEvents[EventIdx];
    ++m_NumEventsQueued;
    GStateAssert(m_NumEventsQueued <= m_NumQueueSlots);

    return true;
}

bool GSTATE
event_source::SampleEventOutput(granny_int32x            OutputIdx,
                               granny_real32            AtT,
                               granny_real32            DeltaT,
                               gstate_text_track_entry* EventBuffer,
                               granny_int32x const      EventBufferSize,
                               granny_int32x*           NumEvents)
{
    GStateAssert(OutputIdx == 0);
    GStateCheckPtrNotNULL(EventBuffer, return false);
    GStateCheckPtrNotNULL(NumEvents, return false);

    if (m_ProducedEventsAt != AtT)
    {
        char const** Temp = m_QueuedEvents;
        m_QueuedEvents = m_ProducedEvents;
        m_ProducedEvents = Temp;

        m_NumEventsProduced = m_NumEventsQueued;
        m_NumEventsQueued = 0;

        m_ProducedEventsAt = AtT;
    }

    if (EventBufferSize < m_NumEventsProduced)
        return false;

    for (int Idx = 0; Idx < m_NumEventsProduced; ++Idx)
    {
        EventBuffer[Idx].Name = GetName();
        EventBuffer[Idx].Text = m_ProducedEvents[Idx];
        EventBuffer[Idx].TimeStamp = AtT;
        EventBuffer[Idx].Weight = 1.0f;

#if GSTATE_USE_SAMPLE_RECORDS
        gstate_sample_record * Record = gstate_sample_recorder::AddSampleRecord(this);
        if( Record )
        {
            GStateReplaceString(Record->Name, GetName());
            GStateReplaceString(Record->Action, m_ProducedEvents[Idx]);
            Record->Category = gstate_sample_record::eSampleRecordCategory_Event;
            Record->Type = gstate_sample_record::eSampleRecordType_EventSource;
        }
#endif //GSTATE_USE_SAMPLE_RECORDS
    }

    *NumEvents = m_NumEventsProduced;


    return true;
}

bool GSTATE
event_source::GetNextEvent(granny_int32x OutputIdx,
                          granny_real32 AtT,
                          gstate_text_track_entry*  Event)
{
    GStateAssert(OutputIdx == 0);
    GStateCheckPtrNotNULL(Event, return false);

    return false;
}

// Returns all possible events on this edge
bool GSTATE
event_source::GetAllEvents(granny_int32x            OutputIdx,
                          gstate_text_track_entry*  EventBuffer,
                          granny_int32x const      EventBufferSize,
                          granny_int32x*           NumEvents)
{
    GStateAssert(OutputIdx == 0);
    GStateCheckPtrNotNULL(EventBuffer, return false);
    GStateCheckPtrNotNULL(NumEvents, return false);

    if (EventBufferSize < m_event_sourceToken->PossibleEventCount)
        return false;

    for (int Idx = 0; Idx < m_event_sourceToken->PossibleEventCount; ++Idx)
    {
        EventBuffer[Idx].Name = GetName();
        EventBuffer[Idx].Text = m_event_sourceToken->PossibleEvents[Idx];
        EventBuffer[Idx].TimeStamp = 0;
        EventBuffer[Idx].Weight = 1.0f;
    }

    *NumEvents = m_event_sourceToken->PossibleEventCount;

    return true;
}

bool GSTATE
event_source::GetCloseEventTimes(granny_int32x  OutputIdx,
                                granny_real32  AtT,
                                char const*    TextToFind,
                                granny_real32* PreviousTime,
                                granny_real32* NextTime)
{
    GStateAssert(OutputIdx == 0);

    // Not appropriate for this node.
    return false;
}

