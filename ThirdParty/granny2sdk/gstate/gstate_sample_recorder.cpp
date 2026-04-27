// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_sample_recorder.h"
#include "gstate_node.h"

#include <string.h>

#include "gstate_cpp_settings.h"

USING_GSTATE_NAMESPACE;

#if GSTATE_USE_SAMPLE_RECORDS

/*static*/ GSTATE gstate_sample_record * gstate_sample_recorder::s_SampleRecords = NULL;
/*static*/ const int GSTATE gstate_sample_recorder::s_MaxSampleRecords = 20000;
/*static*/ bool GSTATE gstate_sample_recorder::s_SampleRecordsInitialized = false;
/*static*/ int GSTATE gstate_sample_recorder::s_FirstRecord = 0;
/*static*/ int GSTATE gstate_sample_recorder::s_LastRecord = 0;
/*static*/ unsigned long GSTATE gstate_sample_recorder::s_SampleRecordFrameNumber = 0;
/*static*/ float* GSTATE gstate_sample_recorder::s_GlobalClock = 0;

void GSTATE
gstate_sample_record::ResetRecord(unsigned long CurrentFrameNumber, node * Node)
{
    const char * blank = "";
    GStateReplaceString(Name,blank);
    GStateReplaceString(Action,blank);
    Type = eSampleRecordType_Count;
    Category = eSampleRecordCategory_Count;
    UID = Node->GetUID();
    Value = 0.0f;
    GlobalTime = *gstate_sample_recorder::GetGlobalClock();
    FrameNumber = CurrentFrameNumber;
    RenderedFrameNumber = 0;
    MagicNumber = 0;
}

void GSTATE
gstate_sample_record::DestroyRecord()
{
    GStateDeallocate(Name);
    GStateDeallocate(Action);
}

/*static*/ void GSTATE
gstate_sample_recorder::InitializeSampleRecordPool(float * GlobalClock)
{
    s_SampleRecords = GStateAllocArray(gstate_sample_record,s_MaxSampleRecords);
    memset(s_SampleRecords, 0, s_MaxSampleRecords * sizeof(gstate_sample_record)); //Zero fill
    s_SampleRecordsInitialized = true;
    s_GlobalClock = GlobalClock;
}

/*static*/ void GSTATE
gstate_sample_recorder::ShutdownSampleRecordPool()
{
    for( int i=0; i<s_MaxSampleRecords; i++ )
    {
        s_SampleRecords[i].DestroyRecord();
    }
    GStateDeallocate(s_SampleRecords);
    s_SampleRecordsInitialized = false;
}

/*static*/ void GSTATE
gstate_sample_recorder::ResetSampleRecordPool()
{
   s_FirstRecord = 0;
   s_LastRecord = 0;
}

/*static*/ gstate_sample_record* GSTATE
gstate_sample_recorder::AddSampleRecord(node * Node)
{
    if(!s_SampleRecordsInitialized)
        return NULL;

    gstate_sample_record* Record = &s_SampleRecords[s_LastRecord];
    Record->ResetRecord(s_SampleRecordFrameNumber, Node);

    s_LastRecord++;

    //Wrap the last record. 
    if( s_LastRecord == s_MaxSampleRecords )
    {
        s_LastRecord = 0;
    }

    //If the last record hits the first record, push the first record forward.  
    if( s_LastRecord == s_FirstRecord )
    {
        s_FirstRecord++;
    }

    //Wrap the first record if it got pushed past the end.
    if( s_FirstRecord == s_MaxSampleRecords )
    {
        s_FirstRecord = 0;
    }

    return Record;
}

/*static*/ gstate_sample_record* GSTATE
gstate_sample_recorder::GetSampleRecord(int Record)
{
    GStateAssert(Record >= 0 && Record < s_MaxSampleRecords);
    return &s_SampleRecords[Record];
}

/*static*/ gstate_sample_record* GSTATE
gstate_sample_recorder::GetSampleRecords(int * pFirstRecord, int * pLastRecord)
{
    GStateAssert(*pFirstRecord >= 0 && *pFirstRecord < s_MaxSampleRecords);
    GStateAssert(*pLastRecord >= 0 && *pLastRecord < s_MaxSampleRecords);

    *pFirstRecord = s_FirstRecord;
    *pLastRecord = s_LastRecord;
    return s_SampleRecords;
}

/*static*/ gstate_sample_record* GSTATE
gstate_sample_recorder::GetPrevSampleRecord(int * pCurrentRecord, int FirstRecord)
{
    GStateAssert(*pCurrentRecord >= 0 && *pCurrentRecord < s_MaxSampleRecords);
    GStateAssert(FirstRecord >= 0 && FirstRecord < s_MaxSampleRecords);

    (*pCurrentRecord)--;
    if( *pCurrentRecord < 0 )
    {
        *pCurrentRecord = s_MaxSampleRecords-1;
    }

    gstate_sample_record* Current = &s_SampleRecords[*pCurrentRecord];
    return Current;
}

/*static*/ gstate_sample_record* GSTATE
gstate_sample_recorder::GetNextSampleRecord(int * pCurrentRecord, int LastRecord)
{
    GStateAssert(*pCurrentRecord >= 0 && *pCurrentRecord < s_MaxSampleRecords);
    GStateAssert(LastRecord >= 0 && LastRecord < s_MaxSampleRecords);

    (*pCurrentRecord)++;
    if( *pCurrentRecord >= s_MaxSampleRecords )
    {
        *pCurrentRecord = 0;
    }

    gstate_sample_record* Current = &s_SampleRecords[*pCurrentRecord];
    return Current;
}

/*static*/ void GSTATE
gstate_sample_recorder::SampleRecordPoolTick()
{
    s_SampleRecordFrameNumber++;
}

/*static*/ unsigned long GSTATE
gstate_sample_recorder::GetSampleRecordPoolFrameNumber()
{
    return s_SampleRecordFrameNumber;
}

#else
namespace { char dummy; }; //Hush public symbol linker warnings. 
#endif //GSTATE_USE_SAMPLE_RECORDS
