// =======================================================================================
// replace_file_api.cpp: Shows how to override Granny's default file handlers.
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "granny.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


// Our file handling functions
GRANNY_CALLBACK(granny_file_reader*) VFSReaderOpen(char const * Filename );
GRANNY_CALLBACK(void) VFSReaderClose(granny_file_reader* Reader );
GRANNY_CALLBACK(granny_int32x) VFSReadAtMost(granny_file_reader *Reader,
                                             granny_int32x FilePosition,
                                             granny_int32x UInt8Count,
                                             void *Buffer);
GRANNY_CALLBACK(bool) VFSGetReaderSize(granny_file_reader* Reader,
                                       granny_int32x *ResultSize);

// Incidental helper functions
#include "replace_file_api_helper.h"

/* DEFTUTORIAL EXPGROUP(TutorialReplaceFileAPI) (TutorialReplaceFileAPI_Introduction, Introduction) */
/* This tutorial shows you how to override Granny's file manipulation functions, replacing
   them with custom versions that you provide.  You might do this in order to provide
   optimized versions that are specialized for your application.  If you want to redirect
   the file opening requests to an internal database or virtual filesystem independent of
   the OS file system, you can do that as well, which is what we'll demonstrate here.

   The support functions in replace_file_api_helper.cpp load 2 Granny files into a simple
   flat array.  (These functions aren't contained in the tutorial since they're incidental
   to what we're demonstrating here.)  We then provide reader functions to redirect read
   requests from $ReadEntireFile to the flat array.

   There are two ways to go about using the replacement functions.  First, you can replace
   Granny's default file handling with $SetDefaultFileReaderOpenCallback.  This causes
   every call to $ReadEntireFile to route to your functions.  (You can still access
   Granny's default file handlers with $CreatePlatformFileReader.)  Second, if you don't
   want to entirely replace the file system, you can create a custom $file_reader, and
   pass it to $ReadEntireFileFromReader.  We'll demonstrate both methods here.

   Note that you can override the writer functionality in exactly the same way, but we
   won't cover that here.

   Prerequisites to understand this demo:
   $* $TutorialBasicLoading
   $-

   New functions and structures discussed in this tutorial:
   $* $file_reader
   $* $open_file_reader_callback
   $* $close_file_reader_callback
   $* $read_at_most_callback
   $* $ReadEntireFileFromReader
   $* $SetDefaultFileReaderOpenCallback
   $-

   Functions and structures introduced incidentally:
   $* $CreatePlatformFileReader
   $* $GetDefaultFileReaderOpenCallback
   $-

   Function index:
   $* $TutorialReplaceFileAPI_main
   $* $TutorialReplaceFileAPI_VFSReaderOpen
   $* $TutorialReplaceFileAPI_VFSReaderClose
   $* $TutorialReplaceFileAPI_VFSReadAtMost
   $* $TutorialReplaceFileAPI_VFSGetReaderSize
   $-
*/

/* DEFTUTORIAL EXPGROUP(TutorialReplaceFileAPI) (TutorialReplaceFileAPI_main, main) */
int main(int, char const*[])
{
    // Check the version
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

    /* First, we'll initialize the VFS.  We're going to load just 2 files into memory to
       demonstrate the concept.  While this VFS is simulated in memory, nothing prevents
       you from interpreting the filenames to route to different devices, network streams,
       or completely synthetic data sources. */
    const char* FilesToLoad[] = { BASE_DIR "pixo_run.gr2", BASE_DIR "gryphon.gr2" };
    int const   NumFiles = sizeof(FilesToLoad) / sizeof(FilesToLoad[0]);

    if (InitializeVFS(FilesToLoad, NumFiles) == false)
    {
        printf("Error while creating the example VFS\n");
        return EXIT_FAILURE;
    }

    /* Now we'll install our replacement file handler.  See
       $TutorialReplaceFileAPI_VFSReaderOpen for a discussion of how this function must
       work.  If you want to store off the current value to restore it to the previous
       state, use $GetDefaultFileReaderOpenCallback. */
    GrannySetDefaultFileReaderOpenCallback(VFSReaderOpen);

    /* Now that our reader is installed, let's try to find "pixo_run.gr2".  (The VFS is
       setup to look for substrings, so we'll ignore paths.)  We'll route this through the
       normal Granny read functions.  Dump the file information if we're successful. */
    granny_file* PixoFile = GrannyReadEntireFile("pixo_run.gr2");
    if (!PixoFile)
    {
        printf("Error while reading pixo_run.gr2!\n");
        return EXIT_FAILURE;
    }

    granny_file_info* PixoInfo = GrannyGetFileInfo(PixoFile);
    if (!PixoInfo)
    {
        printf("Error obtaining a granny_file_info from pixo_run.gr2!\n");
        return EXIT_FAILURE;
    }

    printf("pixo_run.gr2\n"
           "============");
    PrintFileStatistics(PixoFile);
    PrintFileInfoStatistics(PixoInfo);

    /* Now we'll open gryphon.gr2, using the manual reader interface.  It's important to
       understand that this method does not require that our VFSReaderOpen function be
       installed with Granny.  You can have multiple ReaderOpen functions, and choose
       between them at runtime using this method. */
    granny_file_reader* GryphonReader = VFSReaderOpen("gryphon.gr2");
    granny_file* GryphonFile = NULL;
    if (GryphonReader)
    {
        GryphonFile = GrannyReadEntireFileFromReader(GryphonReader);
        if (!GryphonFile)
        {
            printf("Error while reading gryphon.gr2!\n");
            return EXIT_FAILURE;
        }

        granny_file_info* GryphonInfo = GrannyGetFileInfo(GryphonFile);
        if (!GryphonInfo)
        {
            printf("Error obtaining a granny_file_info from gryphon.gr2!\n");
            return EXIT_FAILURE;
        }

        // Close the file reader
        VFSReaderClose(GryphonReader);

        printf("gryphon.gr2\n"
               "===========");
        PrintFileStatistics(GryphonFile);
        PrintFileInfoStatistics(GryphonInfo);
    }
    else
    {
        printf("Error opening gryphon.gr2 manually!\n");
        return EXIT_FAILURE;
    }


    /* That's it!  Now all that remains is to cleanup the allocated structures and the VFS. */
    GrannyFreeFile(PixoFile);
    GrannyFreeFile(GryphonFile);
    CleanupVFS();

    return EXIT_SUCCESS;
}

/* DEFTUTORIAL EXPGROUP(TutorialReplaceFileAPI) (TutorialReplaceFileAPI_ReaderStructure, Custom Reader Structure) */
/* Granny expects to receive a pointer to a $file_reader as the return value from the
   ReaderOpen callback.  function.  Obviously you'll want to append your own data to this
   structure to be read by your Close and ReadAtMost callback functions.  The easiest way
   to do this in C++ is simply to derive your own structure from $file_reader.  When the
   reader is passed to your callbacks, you can recover your structure with a simple cast.
   In C, you can create a structure with a $file_reader as the first member, which amounts
   to the same thing.  (Remember to be careful of alignment issues, the $file_reader
   should be at offset 0 in your structure if you use this method!) */
struct vfs_file_reader : public granny_file_reader
{
    granny_uint8 const* BufferLocation;
    granny_int32x       BufferSize;
};


/* DEFTUTORIAL EXPGROUP(TutorialReplaceFileAPI) (TutorialReplaceFileAPI_VFSReaderOpen, VFSReaderOpen) */
/* This function is called by Granny to open "Filename" for reading.  The first two
   parameters, SourceFileName, and SourceLineNumber are for debugging purposes, they
   encode the file and line number that the open call originated from.  Granny defines a
   pointer type for this function, $open_file_reader_callback. */

GRANNY_CALLBACK(granny_file_reader*)
VFSReaderOpen( char const * Filename )
{
    assert(Filename);

    /* Try to find the file in our VFS */
    void* Data = NULL;
    int   DataSize = -1;
    if (LookupVFSFile(Filename, &Data, &DataSize) == false)
    {
        printf("Unable to load %s\n", Filename);
        return NULL;
    }

    /* Got it!  OK, now we create the reader we'll return to Granny, and use the helper
       function $InitializeFileReader to set the function pointers in the $file_reader
       portion of the structure. */
    vfs_file_reader* Reader = new vfs_file_reader;
    GrannyInitializeFileReader(VFSReaderClose, VFSReadAtMost, VFSGetReaderSize, Reader);

    // Initialize the custom fields, and return the object to Granny
    Reader->BufferLocation = (granny_uint8 const*)Data;
    Reader->BufferSize     = (granny_int32x)DataSize;

    return Reader;
}

/* DEFTUTORIAL EXPGROUP(TutorialReplaceFileAPI) (TutorialReplaceFileAPI_VFSReaderClose, VFSReaderClose) */
/* Called to close the custom reader, and deallocate any associated memory.  Like the
   ReaderOpen callback, the SourceFileName and SourceLineNumber encode the call-site.
   Granny defines the type $close_file_reader_callback for these functions. */
GRANNY_CALLBACK(void)
VFSReaderClose(granny_file_reader* Reader)
{
    assert(Reader);

    /* OK, ensure that this is one of our readers, and recover the custom interface.  At
       that point, we can simply delete the pointer, and we're done.  In some
       applications, you'd close filehandles, network streams, or free other resources
       here.  In this case, we don't own the pointers to VFS memory. */
    assert(Reader->CloseFileReaderCallback == VFSReaderClose);

    vfs_file_reader* CustomReader = static_cast<vfs_file_reader*>(Reader);
    delete CustomReader;
}


/* DEFTUTORIAL EXPGROUP(TutorialReplaceFileAPI) (TutorialReplaceFileAPI_VFSReadAtMost, VFSReadAtMost) */
/* This is the only reading function you need to provide.  Simple read the requested
   number of bytes from the specifed position, and return the number successfully read.
   Granny will frequently request reads that overflow the size of the file to simplify
   buffering. */
GRANNY_CALLBACK(granny_int32x)
VFSReadAtMost(granny_file_reader *Reader,
              granny_int32x FilePosition,
              granny_int32x UInt8Count,
              void *Buffer)
{
    assert(Reader);
    assert(Buffer);

    /* OK, ensure that this is one of our readers, and recover the custom interface. */
    assert(Reader->ReadAtMostCallback == VFSReadAtMost);

    vfs_file_reader* CustomReader = static_cast<vfs_file_reader*>(Reader);


    /* Compute the number of bytes we're actually able to read.  Note that Granny should
       never pass in a FilePosition value that exceeds the size fo the file, but we'll
       double-check that anyways. */
    if (FilePosition >= CustomReader->BufferSize)
    {
        UInt8Count = 0;
    }
    else if (FilePosition + UInt8Count > CustomReader->BufferSize)
    {
        UInt8Count = CustomReader->BufferSize - FilePosition;
    }

    /* Go ahead and do the copy, and tell Granny how many bytes were copied */
    if (UInt8Count > 0)
    {
        memcpy(Buffer,
               CustomReader->BufferLocation + FilePosition,
               UInt8Count);
    }

    return UInt8Count;
}

/* DEFTUTORIAL EXPGROUP(TutorialReplaceFileAPI) (TutorialReplaceFileAPI_VFSGetReaderSize, VFSGetReaderSize) */
/* You should also provide a function that returns the size of the stream. */
GRANNY_CALLBACK(bool)
VFSGetReaderSize(granny_file_reader* Reader, granny_int32x *ResultSize)
{
    assert(Reader);
    assert(ResultSize);

    /* OK, ensure that this is one of our readers, and recover the custom interface. */
    assert(Reader->GetReaderSizeCallback == VFSGetReaderSize);

    vfs_file_reader* CustomReader = static_cast<vfs_file_reader*>(Reader);
    *ResultSize = CustomReader->BufferSize;
    return true;
}

/* DEFTUTORIAL EXPGROUP(TutorialReplaceFileAPI) (TutorialReplaceFileAPI_Summary, Summary) */
/* You should now be able to route Granny's file requests wherever you want.  Remember
   that when you're working with the $file_reader directly, the interpretation of the
   input filename is entirely up to you!  You might want to interpret strings like
   "network:foo.gr2" as a request to open foo.gr2 from your website, while "disk:bar.gr2"
   would route to the local filesystem.  Because the function pointers are stored in the
   reader itself, rather than in the global Granny settings, you can return multiple
   different types of $file_reader objects from your open callback, and Granny will handle
   them transparently.

   From here, you might want to investigate:

   $* $TutorialReplaceMemoryAPI: replace Granny's allocation system in the
   same way.
   $* $TutorialCustomFormat or $TutorialTreeWalker: create custom formats and walk the
   granny metadata structures.
   $* $TutorialTransformFile: convert Granny files into your application's
   coordinate system.
   $* $TutorialMultithreading: hide loading latency by doing loads and other work with
   worker threads.
   $-
*/

/* DEFTUTORIALEND */
