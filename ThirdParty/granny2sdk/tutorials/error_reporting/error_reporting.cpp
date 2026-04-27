// =======================================================================================
// error_reporting.cpp: Shows how to intercept Granny's logging facilities
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

/* DEFTUTORIAL EXPGROUP(TutorialErrorReporting) (TutorialErrorReporting_Introduction, Introduction) */
/* This tutorial shows you how to intercept Granny's logging messages.  Granny provides
   extensive feedback on it's operation, from warning and efficiency messages (e.g., if a
   loaded file is an older format, which will require conversion at runtime) to feedback
   on failures when they occur (e.g., memory exhaustion, or file open failures).  Each
   message is tagged with a type, indicating it's severity and a code indicating the
   subsystem in Granny that encountered the error condition.

   $-

   Prerequisites to understand this demo:
   $* None
   $-

   New functions and types discussed in this tutorial:
   $* $log_callback
   $* $log_function
   $* $log_message_type
   $* $log_message_origin
   $* $GetLogCallback
   $* $SetLogCallback
   $* $GetLogMessageTypeString
   $* $GetLogMessageOriginString
   $-

   Functions and structures introduced incidentally:
   $* None
   $-

   Function index:
   $* $TutorialErrorReporting_main
   $* $TutorialErrorReporting_LogCallback
   $-
*/

/* DEFTUTORIAL EXPGROUP(TutorialErrorReporting) (TutorialErrorReporting_main, main) */
int main(int, char const*[])
{
    // Check the version
    if(!GrannyVersionsMatch)
    {
        printf("Warning: the Granny DLL currently loaded "
               "doesn't match the .h file used during compilation\n");
        return EXIT_FAILURE;
    }

    const char* GryphonFilename = "media/gryphon.gr2";

    /* We just call $SetLogCallback to install our callbacks in the Granny runtime.  If
       you want to save the current allocation settings to restore them later, call
       $GetLogCallback.  Note that while we're passing NULL as the UserData field, you may
       store anything you want here.  For instance, you might store a filehandle to the
       output log in UserData, to recover in your $log_function.  Granny treats this data
       as entirely opaque, if you need to clean it up before your application exits, you
       must handle that manually. */
    granny_log_callback NewCallback;
    NewCallback.Function = LogCallback;
    NewCallback.UserData = NULL;
    GrannySetLogCallback(&NewCallback);

    /* Ask Granny to load a non-existent file so we get some callbacks to our logging
       function. */
    granny_file* BadFile = GrannyReadEntireFile("FILE_DOESNT_EXIST.gr2");
    assert(BadFile == 0);
    BadFile = 0; // silence warning

    /* gryphon.gr2 was exported with Granny 2.7.0.6, and requires run-time conversion when
       we request the $file_info, which will send some efficiency warnings to our
       callback. */
    granny_file* GryphonFile = GrannyReadEntireFile(GryphonFilename);
    granny_file_info* GryphonInfo = GrannyGetFileInfo(GryphonFile);
    assert(GryphonInfo);

    /* Cleanup and exit */
    GryphonInfo = 0;
    GrannyFreeFile(GryphonFile);

    return EXIT_SUCCESS;
}


/* DEFTUTORIAL EXPGROUP(TutorialErrorReporting) (TutorialErrorReporting_LogCallback, LogCallback) */
/* Granny provides the type $log_function for this function.  You can either use the
   utility functions shown below to obtain formatted descriptions of the Type and Origin
   parameters, or use their integer values to filter for different message types.  In
   particular, Type is ordered from lowest to highest severity, starting at
   IgnoredLogMessage = 0, and ending with ErrorLogMessage.  Note that Origin is an
   enumerated value, rather than a bitfield, messages are considered to originate from a
   single subsystem. */
GRANNY_CALLBACK(void) LogCallback(granny_log_message_type Type,
                                  granny_log_message_origin Origin,
                                  char const* File, granny_int32x Line,
                                  char const * Message,
                                  void* /*UserData*/)
{
    assert(Message);

    /* Granny provides helper functions to get printable strings for the message type and
       origin. */
    char const* TypeString   = GrannyGetLogMessageTypeString(Type);
    char const* OriginString = GrannyGetLogMessageOriginString(Origin);

    /* We're just going to dump to STDOUT in this demo.  You can route the messages to the
       filesystem, or a network interface if you need to. */
    printf("Granny sez: %s (%s)\n"
        "  %s(%d): %s\n", TypeString, OriginString, File, Line, Message);
}


/* DEFTUTORIAL EXPGROUP(TutorialErrorReporting) (TutorialErrorReporting_Summary, Summary) */
/* From here, you might want to investigate:

   $* $TutorialHeapCheck: examine all currently active allocations in Granny
   $* $TutorialMultithreading: hide loading latency by doing loads and other work with
   worker threads.
   $-
*/

/* DEFTUTORIALEND */
