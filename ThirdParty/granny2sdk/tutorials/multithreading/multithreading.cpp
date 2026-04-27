// =======================================================================================
// multithreading.cpp: Demonstrates loading a Granny File
//
// To keep things simple, we're just going to load the specified file, and print a few basic
// statistics about it.
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "granny.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>

using namespace std;


#define ARRAY_LENGTH(x) (sizeof(x)/sizeof(x[0]))

char const* ModelFilename = "media/bare_pixo_mesh.gr2";
char const* AnimationFilenames[] = {
    "media/run_cycle_fast.gr2",
    "media/run_cycle_slow.gr2",
    "media/walk_cycle_fast.gr2",
    "media/walk_cycle_slow.gr2"
};

struct WorkTicket
{
    void* WorkArray;
    int   StartElement;
    int   EndElement;
};

static void SpawnWorkers(int NumWorkers,
                         LPTHREAD_START_ROUTINE WorkFunction,
                         void* WorkArray, int NumElements);

static DWORD WINAPI LoadRandomAnimation(void* Ticket);
static DWORD WINAPI SampleModels(void* Ticket);

/* DEFTUTORIAL EXPGROUP(TutorialMultithreading) (TutorialMultithreading_Introduction, Introduction) */
/*
   Prerequisites to this demo:
   $* $TutorialBasicLoading
   $* $TutorialCreateControl
   $-

   New functions and structures discussed in this tutorial:
   $* $SetAllowGlobalStateChanges
   $-

   Function index:
   $* $TutorialMultithreading_main
   $* $TutorialMultithreading_LoadRandomAnimation
   $* $TutorialMultithreading_SampleModels
   $-
*/

/* First, some preliminaries.  We need some helper structures that the worker threads will
   use to communicate with the coordinating main thread.
*/

struct LoadedFile
{
    char const*       ChosenFilename;
    granny_file*      AnimationFile;
    granny_file_info* AnimationInfo;
};

struct SamplingModel
{
    granny_model_instance* ModelInstance;
    granny_local_pose*     ModelPose;
};

/* Parameters to control how many files are loaded, and the minimum number of worker
   threads.
*/
int const NumInstances   = 1024;
int const MinimumWorkers = 2;
int const NumSamples     = 10;

/* DEFTUTORIAL EXPGROUP(TutorialMultithreading) (TutorialMultithreading_main, main) */
int main(int, char const*[])
{
    /* First, handle the standard startup details. */
    if(!GrannyVersionsMatch)
    {
        printf("Warning: the Granny DLL currently loaded "
               "doesn't match the .h file used during compilation\n");
        return EXIT_FAILURE;
    }

    /* We're going to assume that we're on the WinXX platform, which allows us to find out
       how many CPUs are available.  We'll use this as our worker thread count.  Use a
       minimum of 2, just to make sure that some threading takes place regardless.
    */
    int NumWorkers, NumCPUs;
    {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);

        NumCPUs = sysInfo.dwNumberOfProcessors;
        NumWorkers = max(NumCPUs, MinimumWorkers);
    }
    assert(NumCPUs > 0);
    assert(NumWorkers >= MinimumWorkers);

    /* We'll demonstate two multithreaded operations.  First, loading a $file and
       obtaining a $file_info pointer.  Second, we'll create and setup $(control)s for
       each loaded object, and sample the animation twice.  Once in a single thread, and
       once in multiple threads.

       The mesh and model objects are singular, so we'll load them upfront in the main
       before we get into the more complicated procedure for the animations.
    */
    granny_file* ModelFile = GrannyReadEntireFile(ModelFilename);
    if (!ModelFile)
        return EXIT_FAILURE;

    granny_file_info* ModelFileInfo = GrannyGetFileInfo(ModelFile);
    if (!ModelFileInfo)
        return EXIT_FAILURE;

    /* Ok, time to load the animations.  We're going to let the worker thread randomly
       pick one of the files listed in AnimationFilenames to load.  First, create the
       array the worker threads will write into.  There's one loaded file per instance in
       this application.

       Note that we disallow global state changes during the course of the file loading
       procedure, and reenable them afterwards.  Note that $SetAllowGlobalStateChanges is
       a <i>debugging</i> helper only.  Unless you have compiled Granny from source with
       GRANNY_THREAD_CHECKS defined, all Granny APIs are "rails off".
    */
    LoadedFile* FileArray = new LoadedFile[NumInstances];
    memset(FileArray, 0, sizeof(LoadedFile) * NumInstances);

    granny_system_clock Start, End;
    GrannyGetSystemSeconds(&Start);

    GrannySetAllowGlobalStateChanges(false);
    {
        // Call the spawn helper function.
        SpawnWorkers(NumWorkers, LoadRandomAnimation, FileArray, NumInstances);
    }

    GrannySetAllowGlobalStateChanges(true);
    GrannyGetSystemSeconds(&End);
    printf("Loaded %d animations.  Time elapsed: %4f secs\n\n",
           NumInstances, GrannyGetSecondsElapsed(&Start, &End));

    /* Create the $(model_instance)s.  Note that both $InstantiateModel and
       $PlayControlledAnimation modify global state, so $SetAllowGlobalStateChanges should
       be set to "true" when you're calling functions like these.

       Note also that for clarity in the documentation, we're re-calling
       $SetAllowGlobalStateChanges with true. ($GetSystemSeconds modifies global state on
       Win32, so we allowed state changes above.)  Sending true more than once is valid as
       long as it's done from the same thread.  You'll receive an error log if this is
       called from multiple threads at the same time.
    */
    GrannySetAllowGlobalStateChanges(true);

    SamplingModel* Models = new SamplingModel[NumInstances];
    {for(int ModelIdx = 0; ModelIdx < NumInstances; ++ModelIdx)
    {
        SamplingModel& ThisModel = Models[ModelIdx];

        ThisModel.ModelInstance = GrannyInstantiateModel(ModelFileInfo->Models[0]);
        ThisModel.ModelPose =
            GrannyNewLocalPose(ModelFileInfo->Models[0]->Skeleton->BoneCount);

        // Create a control for the selected animation for this model, with an offset to
        // set it apart from the other models.
        granny_control* Control =
            GrannyPlayControlledAnimation(ModelIdx / granny_real32(NumInstances - 1),
                                          FileArray[ModelIdx].AnimationInfo->Animations[0],
                                          ThisModel.ModelInstance);
        GrannyFreeControlOnceUnused(Control);
        assert(Control);
    }}


    /* For comparison purposes, we'll do two timed sets of sampling passes.  First, using
       a single worker (the main thread), and then using a series of worker threads.
       Again, note that we explictly mark $SetAllowGlobalStateChanges for the sampling
       phase.  $SetModelClock can modify global state, so it must be executed from one
       thread only, while no other Granny work is occuring.  (Ok, this isn't entire true,
       but write us for the details, or see $MultithreadingGranny.)
    */
    int const UseWorkers[2] = { 1, NumWorkers };
    for (int UseIdx = 0; UseIdx < ARRAY_LENGTH(UseWorkers); ++UseIdx)
    {
        for (int Sample = 0; Sample < NumSamples; ++Sample)
        {
            granny_real32 const SampleTime = Sample / granny_real32(NumSamples-1);

            granny_system_clock Set, SampleStart, SampleEnd;
            GrannyGetSystemSeconds(&Set);
            {for(int ModelIdx = 0; ModelIdx < NumInstances; ++ModelIdx)
            {
                SamplingModel& ThisModel = Models[ModelIdx];
                GrannySetModelClock(ThisModel.ModelInstance, SampleTime);
            }}

            GrannyGetSystemSeconds(&SampleStart);
            SpawnWorkers(UseWorkers[UseIdx], SampleModels, Models, NumInstances);
            GrannyGetSystemSeconds(&SampleEnd);

            printf("%d thread(s): SetModelClock %.3f ms, SampleModelAnimations %.3f ms\n",
                   UseWorkers[UseIdx],
                   GrannyGetSecondsElapsed(&Set, &SampleStart) * 1000.0,
                   GrannyGetSecondsElapsed(&SampleStart, &SampleEnd) * 1000.0);
        }

        printf("\n");
    }

    /* And it's all over, except the cleanup. */
    {for(int ModelIdx = 0; ModelIdx < NumInstances; ++ModelIdx)
    {
        SamplingModel& ThisModel = Models[ModelIdx];
        GrannyFreeModelInstance(ThisModel.ModelInstance);
        GrannyFreeLocalPose(ThisModel.ModelPose);

        ThisModel.ModelInstance = 0;
        ThisModel.ModelPose = 0;
    }}
    delete [] Models;
    Models = 0;

    {for(int FileIdx = 0; FileIdx < NumInstances; ++FileIdx)
    {
        LoadedFile& ThisFile = FileArray[FileIdx];
        GrannyFreeFile(ThisFile.AnimationFile);

        ThisFile.ChosenFilename = 0;
        ThisFile.AnimationFile  = 0;
        ThisFile.AnimationInfo  = 0;
    }}
    delete [] FileArray;
    FileArray = 0;

    GrannyFreeFile(ModelFile);
    ModelFileInfo = 0;
    ModelFile = 0;

    return EXIT_SUCCESS;
}


/* DEFTUTORIAL EXPGROUP(TutorialMultithreading) (TutorialMultithreading_LoadRandomAnimation, LoadRandomAnimation) */
/* This is the loading worker function.  Despite the name, we'll just use a modulus
   operator to ensure that equal numbers of each file are loaded.  $ReadEntireFile and
   $GetFileInfo are safe when global state changes are disallowed, which we ensure in the
   calling thread.

   In a real application, you'd of course return some sort of error indication to the main
   thread if the load failed, but to keep this simple, we're just going to hope for the
   best.
*/
static DWORD WINAPI
LoadRandomAnimation(void* TicketPtr)
{
    assert(TicketPtr);
    WorkTicket& Ticket = *(WorkTicket*)TicketPtr;

    LoadedFile* FileArray = (LoadedFile*)Ticket.WorkArray;
    {for(int ItemIdx = Ticket.StartElement; ItemIdx < Ticket.EndElement; ++ItemIdx)
    {
        LoadedFile& ThisFile = FileArray[ItemIdx];

        // Ok, so % isn't very random.  But it does vary!
        ThisFile.ChosenFilename =
            AnimationFilenames[ItemIdx % ARRAY_LENGTH(AnimationFilenames)];

        // We have no provision in this simple application for propagating errors back to
        // the main thread, so just assert that everything goes OK.
        ThisFile.AnimationFile = GrannyReadEntireFile(ThisFile.ChosenFilename);
        assert(ThisFile.AnimationFile);

        ThisFile.AnimationInfo = GrannyGetFileInfo(ThisFile.AnimationFile);
        assert(ThisFile.AnimationInfo);
    }}

    return 0;
}


/* DEFTUTORIAL EXPGROUP(TutorialMultithreading) (TutorialMultithreading_SampleModels, SampleModels) */
/* And the sampling worker function.  This function doesn't build the $world_pose, since
   in most cases, the recommendation is to build that on the fly, just before submission
   to the GPU.  You could use a function like $SampleModelAnimationsAccelerated, or even a
   bare $BuildWorldPose call here to create the $world_pose, if you'd prefer.
*/
static DWORD WINAPI
SampleModels(void* TicketPtr)
{
    assert(TicketPtr);
    WorkTicket& Ticket = *(WorkTicket*)TicketPtr;

    SamplingModel* ModelArray = (SamplingModel*)Ticket.WorkArray;
    {for(int ItemIdx = Ticket.StartElement; ItemIdx < Ticket.EndElement; ++ItemIdx)
    {
        SamplingModel& ThisModel = ModelArray[ItemIdx];

        GrannySampleModelAnimations(ThisModel.ModelInstance,
                                    0, GrannyGetSourceSkeleton(ThisModel.ModelInstance)->BoneCount,
                                    ThisModel.ModelPose);
    }}

    return 0;
}

/* DEFTUTORIAL EXPGROUP(TutorialMultithreading) (TutorialMultithreading_Summary, Summary) */
/* Multithreading FTW!
*/

/* DEFTUTORIALEND */


// Helper function to fan out worker threads.  Note that this is not entirely fair when
// comparing the 1 worker case to the N worker case, since CreateThread has non-trivial
// overhead.  (About 1ms in CreateThread versus 16ms total for the sampling function on my
// machine.)
static void
SpawnWorkers(int NumWorkers,
             LPTHREAD_START_ROUTINE WorkFunction,
             void* WorkArray, int NumElements)
{
    assert(NumWorkers > 0);
    assert(NumElements > 0);

    vector<WorkTicket> Tickets(NumWorkers);

    /* Fill out the work tickets */
    {
        int Current = 0;
        int Remaining = NumElements;

        {for(int Idx = 0; Idx < NumWorkers; ++Idx)
        {
            assert(Current >= 0 && Current < NumElements);
            assert(Remaining > 0 && Remaining <= NumElements);

            int const WorkersRemaining = NumWorkers - Idx;
            int const NumForThisWorker = Remaining / WorkersRemaining;

            WorkTicket& Ticket = Tickets[Idx];
            Ticket.WorkArray = WorkArray;
            Ticket.StartElement = Current;
            Ticket.EndElement   = Current + NumForThisWorker;

            assert(Ticket.StartElement >= 0 && Ticket.StartElement < NumElements);
            assert(Ticket.EndElement >= 0 && Ticket.EndElement <= NumElements);

            Current   += NumForThisWorker;
            Remaining -= NumForThisWorker;
        }}
    }

    /* Spawn the workers.  Note that we only spawn N-1 workers, we handle the last ticket
       in this thread to reduce overhead in low worker situtations.  In many cases, you'll
       want to immediately move onto other jobs in the main thread, and check back
       periodically to see if the worker threads have completed.
    */
    vector<HANDLE> WorkerHandles(NumWorkers - 1, INVALID_HANDLE_VALUE);
    {for(int Worker = 0; Worker < NumWorkers-1; ++Worker)
    {
        WorkerHandles[Worker] =
            CreateThread(NULL, 0,
                         WorkFunction, (LPVOID)&Tickets[Worker],
                         0, NULL);
    }}

    WorkFunction(&Tickets[NumWorkers-1]);

    /* Wait for the workers to complete, if we spawned any.  That's it!  The jobs are
       completed. */
    if (NumWorkers > 1)
    {
        WaitForMultipleObjects(NumWorkers-1, &WorkerHandles[0], TRUE, INFINITE);
    }
}

