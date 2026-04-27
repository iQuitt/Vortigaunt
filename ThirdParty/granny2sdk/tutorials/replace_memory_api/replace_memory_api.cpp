// =======================================================================================
// replace_memory_api.cpp: Shows how to override Granny's default allocators.
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "granny.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Our custom allocator
GRANNY_CALLBACK(void*) CustomAllocator(const char* SourceFile,
                                       granny_int32x SourceLine,
                                       granny_uintaddrx Alignment,
                                       granny_uintaddrx Size,
                                       granny_int32x    Intent);
GRANNY_CALLBACK(void) CustomDeallocator(const char* SourceFile,
                                         granny_int32x SourceLine,
                                         void* Memory);


/* DEFTUTORIAL EXPGROUP(TutorialReplaceMemoryAPI) (TutorialReplaceMemoryAPI_Introduction, Introduction) */
/* This tutorial shows you how to override Granny's memory allocators, replacing them with
   custom versions that you provide.  By default, Granny requests memory from the default
   system heap for whatever platform you're running on, but you may want to redirect to a
   custom heap to integrate with your debugging or just to have 100% memory control.
   $-

   Prerequisites to understand this demo:
   $* None
   $-

   New functions and types discussed in this tutorial:
   $* $allocate_callback
   $* $deallocate_callback
   $* $intaddrx
   $* $GetAllocator
   $* $SetAllocator
   $-

   Functions and structures introduced incidentally:
   $* None
   $-

   Function index:
   $* $TutorialReplaceMemoryAPI_main
   $* $TutorialReplaceMemoryAPI_CustomAllocator
   $* $TutorialReplaceMemoryAPI_CustomDeallocator
   $-
*/

/* DEFTUTORIAL EXPGROUP(TutorialReplaceMemoryAPI) (TutorialReplaceMemoryAPI_main, main) */
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

    /* This is pretty straightforward.  We just call $SetAllocator to install our
       callbacks in the Granny runtime.  If you want to save the current allocation
       settings to restore them later, just call $GetAllocator. */
    GrannySetAllocator(CustomAllocator, CustomDeallocator);

    /* Ask Granny to load a file so we get some callbacks to our allocator.  Free it immediately to clean up the allocations. */
    granny_file* PixoFile = GrannyReadEntireFile(BASE_DIR "pixo_run.gr2");
    if (PixoFile == NULL)
    {
        printf("Error loading pixo_run.gr2!\n");
        return EXIT_FAILURE;
    }

    GrannyFreeFile(PixoFile);

    return EXIT_SUCCESS;
}

/* DEFTUTORIAL EXPGROUP(TutorialReplaceMemoryAPI) (TutorialReplaceMemoryAPI_CustomAllocator, CustomAllocator) */
/* Granny defines the type $allocate_callback for this function.  SourceFile and
   SourceLine help you track where in Granny the allocations are coming from. */
GRANNY_CALLBACK(void*)
CustomAllocator(const char* SourceFile,
                const granny_int32x SourceLine,
                granny_uintaddrx Alignment,
                granny_uintaddrx Size,
                granny_int32x Intent)
{
    assert(SourceFile);
    assert(Alignment > 0);

    // Track the requests for the demo
    printf("Allocate: %u bytes requested as \"%s\"\n"
           "          %s:%d\n",
           (granny_uint32)Size, GrannyAllocationIntentString(Intent), SourceFile, SourceLine);

    /* Size == 0 is a valid allocation, if a little strange.  Granny will never pass this
       value, but we'll imitate the behavior for new or malloc: simply return NULL. */
    if (Size == 0)
        return NULL;

    /* We have to ensure that the returned address is aligned to the requested boundary.
       We'll be storing the base pointer directly behind the returned pointer so we can
       find it later, which means we need to allocate (Alignment + sizeof(void*))
       bytes extra to be able to satisfy the alignment requirement in all cases.  If you
       know that your allocator returns addresses aligned to a specific boundary, this can
       obviously be much easier.  Here, we'll assume that the return value from malloc()
       is entirely unaligned.

       The $intaddrx type is defined for each platform to be an integer type such that
       (sizeof($intaddrx) == sizeof(void*)).  This is necessary to properly support 64-bit
       platforms like Win64.
    */
    granny_intaddrx ActualSize = Size + Alignment + sizeof(void*);

    granny_uint8* BaseAddress = (granny_uint8*)malloc(ActualSize);
    granny_uint8* BasePointerOffset = BaseAddress + sizeof(void*);

    granny_intaddrx BPOModAlign = ((granny_intaddrx)BasePointerOffset) % Alignment;
    if (BPOModAlign != 0)
    {
        BasePointerOffset += (Alignment - BPOModAlign);
    }

    granny_uint8* StoreBaseAddress = BasePointerOffset - sizeof(void*);

    /* Having now jumped through all the necessary hoops, store off the base address, and
       return the memory to Granny.  You may want to write garbage ontop of the returned
       buffer in debug mode in order to help catch uninitialized data errors. */
    *((void**)StoreBaseAddress) = BaseAddress;

#ifndef NDEBUG
    memset(BasePointerOffset, 0xCC, Size);
#endif

    printf("          returned pointer = %p\n", BasePointerOffset);
    return BasePointerOffset;
}


/* DEFTUTORIAL EXPGROUP(TutorialReplaceMemoryAPI) (TutorialReplaceMemoryAPI_CustomDeallocator, CustomDeallocator) */
/* Granny defines the type $deallocate_callback for this function.  SourceFile and
   SourceLine help you track where in Granny the requests are coming from. */
GRANNY_CALLBACK(void)
CustomDeallocator(const char* SourceFile,
                  const granny_int32x SourceLine,
                  void* Memory)
{
    assert(SourceFile);

    /* Deallocations of NULL are legal, and common in Granny.  Simply ignore these requests. */
    if (Memory == NULL)
        return;

    // Track the requests for the demo
    printf("    Free: Address %p released\n"
           "          %s:%d\n",
           Memory, SourceFile, SourceLine);



    /* Recover the real base address for the allocation, and free the memory. */
    granny_uint8* OffsetAddress    = (granny_uint8*)Memory;
    granny_uint8* StoreBaseAddress = OffsetAddress - sizeof(void*);

    void* RealBaseAddress = *((void**)StoreBaseAddress);
    free(RealBaseAddress);
}


/* DEFTUTORIAL EXPGROUP(TutorialReplaceMemoryAPI) (TutorialReplaceMemoryAPI_Summary, Summary) */
/* From here, you might want to investigate:

   $* $TutorialHeapCheck: examine all currently active allocations in Granny
   $* $TutorialErrorReporting: get detailed error messages from Granny when it all goes wrong.
   $* $TutorialCustomFormat or $TutorialTreeWalker: create custom formats and walk the
   granny metadata structures.
   $* $TutorialMultithreading: hide loading latency by doing loads and other work with
   worker threads.
   $-
*/

/* DEFTUTORIALEND */
