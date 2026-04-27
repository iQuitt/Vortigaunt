// ========================================================================
// $Notice: $
// ========================================================================
#if !defined(GSTATE_EVENT_SOURCE_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class event_source : public node
{
    typedef node parent;

public:
    // The output edge has the same name as the source in a state machine...
    virtual bool SetName(char const* NewName);
    bool SetOutputName(granny_int32x OutputIdx, char const* NewEdgeName);

    granny_int32x NumPossibleEvents();
    void          AddPossibleEvent(char const* EventString);
    void          DeletePossibleEvent(granny_int32x Index);
    char const*   GetPossibleEvent(granny_int32x Index);
    void          SetPossibleEvent(granny_int32x Index, char const* NewString);

    bool PokeEvent(char const* Event);
    bool PokeEventByIdx(granny_int32x Index);

    DECL_SAMPLEEVENT_INTERFACE();
    DISALLOW_IO_MANIPULATION_INTERFACE();

    DECL_CONCRETE_NODE_TOKEN(event_source);
    IMPL_CASTABLE_INTERFACE(event_source);

    virtual bool AllowSyncInputsWithParentOutputs() const { return false; }
    virtual bool AllowSyncOutputsWithParentOutputs() const { return false; }

private:
    static const int sm_MinQueueSize;

    // Array is max(PossibleEventCount, sm_MinQueueSize), or 
    char const**  m_QueuedEvents;
    granny_int32x m_NumQueueSlots;
    granny_int32x m_NumEventsQueued;

    // This is not ideal, but prevents double-queries from retrieving no events, and is
    // unambiguous
    granny_real32 m_ProducedEventsAt;
    char const**  m_ProducedEvents;
    granny_int32x m_NumEventsProduced;
};

END_GSTATE_NAMESPACE;


#include "gstate_header_postfix.h"
#define GSTATE_EVENT_SOURCE_H
#endif /* GSTATE_EVENT_SOURCE_H */
