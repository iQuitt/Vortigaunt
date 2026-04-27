#if !defined(ML_ARGPARSE_H)
// ========================================================================
//  \file ml_argparse.h
//  \brief Commandline argument parsing
//  \author Dave Moore, <a href="http://onepartcode.com/">onepartcode.com</a>
//
//  \attention This code is released into the public domain.
//
//  An argument parser somewhat like the GNU argument parser GetOpt,
//  but redesigned and reimplemented for license reasons.  In particular,
//  I do not want this subject to the GPL.
// ========================================================================
#include "granny.h"

namespace argparse
{
    // Valid argument types
    enum EArgType
    {
        eNoArg,
        eBoolean,
        eInteger,
        eFloat,
        eString,
        eFilename
    };

    enum EResult
    {
        eNoError,

        eInvalidArguments,
        eInvalidOption,
        eBufferToSmall,
        eCallbackFailed,
        eOptionNotUnderstood,

        eGeneralError
    };

    /*! \brief Description of an argument

      Note that the structure owns only the memory pointed to by
      buffer, which is the location that any arguments will be
      stored.  Note that either of shortChar and pLongDesc may be
      null, but obviously one of them must be non-null.  Note that
      null means '\0' for shortChar

      If the type is eNoArg, then pBuffer may be NULL, otherwise, it
      must point to a buffer large enough to hold the argument.  If
      a string arg exceeds the space allocated (bufferLength), the
      driver will return with an error.

      \todo Do we need to support multi-argument options?  (ie: int, int)
    */
    struct Description
    {
        char        shortChar;
        const char* pLongDesc;
        int         argumentId;
        EArgType    argumentType;

        void*         pBuffer;
        granny_uint32 bufferLength;
    };

    /*! Argument Callback function.  The application must supply
      one of these.  Function returns true if the parser should
      continue.  If the callback returns false, the parser will
      halt, and return an error to the original calling function
    */
    typedef bool (*ArgCallback)(const Description*);

    /*! Error callback function.  Passed a descriptive string, to
      be used however the caller likes.  Note that it is not
      necessary to supply an error callback, the Parse function
      will still pass back the error enum at the call site.  Note
      that this function should NOT call exit, it should return
      normally
    */
    typedef void (*ArgErrorCallback)(const char*);

    /*! Main driver function.  Note that argv will be modified to
      copy any non-option arguments to the end of the array.  No
      other guarantees are made about the other pointers in the
      array.
    */
    EResult Parse(const int        argc,
                  const char**     argv,
                  Description*     pDescriptions,
                  const int        numDescriptions,
                  int*             pNumUnusedArgs,
                  ArgCallback      pArgCallback,
                  ArgErrorCallback pArgErrorCallback = 0);

} // namespace argparse

#define ML_ARGPARSE_H
#endif
