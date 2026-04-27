// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_SAMPLE_RECORDER_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_BASE_H
#include "gstate_base.h"
#endif

#include "gstate_node.h"

BEGIN_GSTATE_NAMESPACE;

#if GSTATE_USE_SAMPLE_RECORDS
struct gstate_sample_record
{
    enum ESampleRecordCategory
    {
        eSampleRecordCategory_Pose,
        eSampleRecordCategory_Event,
        eSampleRecordCategory_Scalar,
        eSampleRecordCategory_IK,
        eSampleRecordCategory_Count
    };

    enum ESampleRecordType
    {
        eSampleRecordType_Animation,
        eSampleRecordType_RestPose,
        eSampleRecordType_PoseStorage,
        eSampleRecordType_EventSource,
        eSampleRecordType_IKPosition,
        eSampleRecordType_IKOrientTowards,
        eSampleRecordType_IKOrientMatch,
        eSampleRecordType_IKAimAt,
        eSampleRecordType_IKTwoBone,
        eSampleRecordType_Count
    };

    void ResetRecord(unsigned long FrameNumber, node * Node);
    void DestroyRecord();

    char * Name;             
    char * Action;             
    ESampleRecordCategory Category; 
    ESampleRecordType Type; 
    int UID;                //unique ID for records within this Type
    int MagicNumber;        //a magic number for differentiating records
    float Value;           
    float GlobalTime;       //application time (not animation time)
    unsigned long FrameNumber;
    unsigned long RenderedFrameNumber;
};

struct gstate_sample_recorder
{
    static void InitializeSampleRecordPool(float * GlobalClock);
    static void ShutdownSampleRecordPool();
    static void ResetSampleRecordPool();
    static gstate_sample_record* GetSampleRecord(int Record);
    static gstate_sample_record* GetSampleRecords(int * pFirstRecord, int * pLastRecord);
    static gstate_sample_record* GetNextSampleRecord(int * pCurrentRecord, int LastRecord);
    static gstate_sample_record* GetPrevSampleRecord(int * pCurrentRecord, int FirstRecord);
    static gstate_sample_record* AddSampleRecord(node*); //Returns the record. Caller fills it out. 
    static void SampleRecordPoolTick();
    static unsigned long GetSampleRecordPoolFrameNumber();
    static float * GetGlobalClock() {return s_GlobalClock;}

private:
    static gstate_sample_record * s_SampleRecords;
    static const int s_MaxSampleRecords;
    static bool s_SampleRecordsInitialized;
    static int s_FirstRecord;
    static int s_LastRecord;
    static unsigned long s_SampleRecordFrameNumber;
    static float * s_GlobalClock;
};

#endif //GSTATE_USE_SAMPLE_RECORDS

END_GSTATE_NAMESPACE;


#include "gstate_header_postfix.h"
#define GSTATE_SAMPLE_RECORDER_H
#endif /* GSTATE_NODE_H */
