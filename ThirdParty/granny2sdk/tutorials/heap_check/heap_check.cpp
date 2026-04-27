// =======================================================================================
// heap_check.cpp: demonstrate the memory diagnostics in granny
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "granny.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Our logging callback
GRANNY_CALLBACK(void) LogCallback(granny_log_message_type Type,
                                  granny_log_message_origin Origin,
                                  char const* File, granny_int32x Line,
                                  char const * Message,
                                  void * UserData);

/* DEFTUTORIAL EXPGROUP(TutorialHeapCheck) (TutorialHeapCheck_Introduction, Introduction) */
/* This tutorial shows you how to track allocations in Granny.  If you're using the
   default Granny allocator (i.e., you haven't replaced it with $SetAllocator), all
   allocations are tracked, both in release and debug.  This incurs very little overhead
   and provides you with an extremely useful debugging tool.
   $-

   Prerequisites to understand this demo:
   $* None
   $-

   New functions and types discussed in this tutorial:
   $* $allocation_information
   $* $AllocationsBegin
   $* $NextAllocation
   $* $AllocationsEnd
   $* $GetAllocationInformation
   $* $BeginAllocationCheck
   $* $EndAllocationCheck
   $-

   Functions and structures introduced incidentally:
   $* None
   $-

   Function index:
   $* $TutorialHeapCheck_main
   $-
*/

/* DEFTUTORIAL EXPGROUP(TutorialHeapCheck) (TutorialHeapCheck_main, main) */
int main(int, char const*[])
{
    // Check the version, and install our log monitor
    if(!GrannyVersionsMatch)
    {
        printf("Warning: the Granny DLL currently loaded "
               "doesn't match the .h file used during compilation\n");
        return EXIT_FAILURE;
    }

#ifdef _MACOSX
    #define BASE_DIR "../../../media/"
#else
    #define BASE_DIR "media/"
#endif

    granny_log_callback NewCallback;
    NewCallback.Function = LogCallback;
    NewCallback.UserData = NULL;
    GrannySetLogCallback(&NewCallback);


    /* Place a marker in the heap so we can iterate all allocations that come after.  You
       can have as many of these outstanding as you want, which can be handy to detect
       memory leaks that originate from an unknown function in a known call-tree.  Once
       you're done with the check, you release the marker with $EndAllocationCheck, which
       will log all of the still outstanding allocations that have occured since the
       marker was placed. */
    void* AllocationMarker = GrannyBeginAllocationCheck();

    /* We'll load a few files just to get some outstanding allocations.  We're going to
       leak the second file, so we can demonstrate how to detect memory leaks in your
       code. */
    granny_file* PixoFile = GrannyReadEntireFile(BASE_DIR "pixo_run.gr2");
    GrannyReadEntireFile(BASE_DIR "gryphon.gr2");


    /* Here we iterate through all of the allocations currently active.  You don't need to
       use the $BeginAllocationCheck API to do this, $AllocationsBegin returns the global
       head of the allocation list.  Each $allocation_information structure contains
       information on where the allocation originated, as well as allocator overhead
       information.  (Caused by alignment restrictions, the bookeeping overhead assocated
       with the tracking, etc.)

       Note that if you're multithreading Granny, there is an addition detail to this loop
       that is required, see $TutorialMultithreading for details. */
    printf("allocation_list\n"
           "===============\n");
    for(granny_allocation_header *Header = GrannyAllocationsBegin();
        Header != GrannyAllocationsEnd();
        Header = GrannyNextAllocation(Header))
    {
        granny_allocation_information Info;
        GrannyGetAllocationInformation(Header, &Info);

        printf("%s(%d): %u bytes allocated at address %p\n",
               Info.SourceFileName, Info.SourceLineNumber,
               (granny_uint32)Info.RequestedSize, Info.Memory);
    }
    printf("end allocation_list\n"
           "===================\n\n");


    /* Now we'll demonstrate Granny's auto-leak detection.  The call to $ReadEntireFile
       that loaded gryphon.gr2 has generated a memory leak, so we should receive log
       warnings that memory allocated after $BeginAllocationCheck has not been freed.  If
       you've compiled the Granny library in debug mode, you'll also receive an assert
       notifying you about the problem. */
    GrannyFreeFile(PixoFile);
    GrannyEndAllocationCheck(AllocationMarker);

    return EXIT_SUCCESS;
}


/* DEFTUTORIAL EXPGROUP(TutorialHeapCheck) (TutorialHeapCheck_Summary, Summary) */
/* From here, you might want to investigate:

   $* $TutorialReplaceMemoryAPI: replace Granny's allocation system
   $* $TutorialMultithreading: hide loading latency by doing loads and other work with
   worker threads.
   $-
*/

/* DEFTUTORIALEND */


// See the tutorial "Error Reporting" for details on log callbacks.
GRANNY_CALLBACK(void) LogCallback(granny_log_message_type Type,
                                  granny_log_message_origin Origin,
                                  char const* File, granny_int32x Line,
                                  char const * Message,
                                  void* /*UserData*/)
{
    assert(Message);
    char const* TypeString   = GrannyGetLogMessageTypeString(Type);
    char const* OriginString = GrannyGetLogMessageOriginString(Origin);

    printf("Granny sez: %s (%s)\n"
        "  %s(%d): %s\n", TypeString, OriginString, File, Line, Message);
}
