/*!
  \file ml_argparse.cc
  \brief
  \author Dave Moore, <a href="http://onepartcode.com/">onepartcode.com</a>

  \attention This code is released into the public domain.

*/
#include "ml_argparse.h"
#include <vector>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>   
#include <stdlib.h>

#ifdef _WIN32
#define strcasecmp _stricmp
#define snprintf _snprintf
#endif

using namespace argparse;
using namespace std;

namespace
{
    bool IsValidArgType(const EArgType type)
    {
        return (type == eNoArg ||
                type == eBoolean ||
                type == eInteger ||
                type == eFloat ||
                type == eString ||
                type == eFilename);
    }

    bool IsValidDescription(const Description* pDescription)
    {
        assert(pDescription);

        // One of the arguments must be non-null
        if (pDescription->shortChar == '\0' && pDescription->pLongDesc == NULL)
            return false;

        // If a long arg is non-null, it must not be the empty string
        if (pDescription->pLongDesc && pDescription->pLongDesc[0] == '\0')
            return false;

        // If the shortChar is non-null, it must be alphanum
        if (pDescription->shortChar != '\0' && !isalnum(pDescription->shortChar))
            return false;

        // Must be a valid argument type
        if (IsValidArgType(pDescription->argumentType) == false)
            return false;

        // If the arg type isn't eNoArg, then the buffer must be
        //  correctly allocated
        switch (pDescription->argumentType)
        {
            case eBoolean:
            {
                if (pDescription->pBuffer == NULL ||
                    pDescription->bufferLength < sizeof(bool))
                {
                    return false;
                }
            } break;

            case eInteger:
            {
                if (pDescription->pBuffer == NULL ||
                    pDescription->bufferLength < sizeof(int))
                {
                    return false;
                }
            } break;

            case eFloat:
            {
                if (pDescription->pBuffer == NULL ||
                    pDescription->bufferLength < sizeof(float))
                {
                    return false;
                }
            } break;

            case eString:
            case eFilename:
            {
                // A string or filename must have room for at least one character in
                //  addition to the null char
                if (pDescription->pBuffer == NULL ||
                    pDescription->bufferLength <= 1)
                {
                    return false;
                }
            } break;

            case eNoArg:
            default:
                break;
        }

        return true;
    }

    void HandleError(ArgErrorCallback pArgErrorCallback,
                     const char* pErrorString)
    {
        if (pArgErrorCallback != 0)
            pArgErrorCallback(pErrorString);
    }

    void ShiftDown(int&         argc,
                   char const** argv,
                   int const    shiftFrom,
                   int const    shiftCount)
    {
        assert(argc > 0);
        assert(shiftFrom >= 0);
        assert(shiftFrom + shiftCount <= argc);

        memmove(&argv[shiftFrom],
                &argv[shiftFrom + shiftCount],
                (argc - (shiftFrom + shiftCount)) * sizeof(char const*));
        argc -= shiftCount;
    }

    bool GetArgument(Description* pDescription,
                     const char*  argString)
    {
        assert(pDescription && IsValidDescription(pDescription));
        assert(argString);

        switch (pDescription->argumentType)
        {
            case eBoolean:
            {
                // There are only four valid strings for a boolean argument:
                if (strcasecmp(argString, "true") == 0 ||
                    strcasecmp(argString, "t") == 0)
                {
                    *((bool*)pDescription->pBuffer) = true;
                    return true;
                }
                else if (strcasecmp(argString, "false") == 0 ||
                         strcasecmp(argString, "f") == 0)
                {
                    *((bool*)pDescription->pBuffer) = false;
                    return true;
                }

                return false;
            }

            case eInteger:
            {
                char* endptr;
                *((int*)pDescription->pBuffer) = (int)strtol(argString, &endptr, 10);

                // If the ptr is equal to the input pointer, then it was not an integer
                return (endptr != argString);
            }

            case eFloat:
            {
                char* endptr;
                *((float*)pDescription->pBuffer) = (float)strtod(argString, &endptr);

                // If the ptr is equal to the input pointer, then it was not a float
                return (endptr != argString);
            }

            case eString:
            case eFilename:
            {
                char* BufferAsChar = (char*)pDescription->pBuffer;
                assert(BufferAsChar);

                BufferAsChar[pDescription->bufferLength - 1] = '\0';
                strncpy(BufferAsChar, argString, pDescription->bufferLength);
                if (BufferAsChar[pDescription->bufferLength - 1] != '\0')
                {
                    // overflow...
                    return false;
                }
                else
                {
                    if (pDescription->argumentType == eFilename)
                    {
                        // If the argument is a filename, it must exist, and be readable
                        // at this point.  Files that aren't required to exist should be
                        // specified with eString.
                        FILE* readFile = fopen(argString, "r");
                        if (readFile == 0)
                            return false;

                        fclose(readFile);
                    }

                    // All is well.
                    return true;
                }
            } break;

            case eNoArg:
                return true;

            default:
                assert(false);
                return false;
        }
    }

    EResult CheckInputs(int const        argc,
                        char const**     argv,
                        Description*     pDescriptions,
                        int const        numDescriptions,
                        int*             pNumUnusedArgs,
                        ArgCallback      pArgCallback,
                        ArgErrorCallback pArgErrorCallback)
    {
        if (argc <= 0 || argv == NULL)
        {
            HandleError(pArgErrorCallback, "Internal error (0): argc, argv invalid");
            return eInvalidArguments;
        }

        for (int i = 0; i < argc; i++)
        {
            if (argv[i] == NULL || argv[i][0] == '\0')
            {
                HandleError(pArgErrorCallback, "Internal error (1): argv invalid");
                return eInvalidArguments;
            }
        }

        if (pDescriptions == NULL || numDescriptions < 0)
        {
            HandleError(pArgErrorCallback,
                        "Internal error (2): Invalid descriptions array for argparse");
            return eGeneralError;
        }

        for (int i = 0; i < numDescriptions; i++)
        {
            if (IsValidDescription(pDescriptions + i) == false)
            {
                HandleError(pArgErrorCallback,
                            "Internal error (3): Invalid descriptions array for argparse");
                return eGeneralError;
            }
        }

        if (pArgCallback == NULL)
        {
            HandleError(pArgErrorCallback, "Internal error (4): Invalid parser callback");
            return eGeneralError;
        }

        if (pNumUnusedArgs == NULL)
        {
            HandleError(pArgErrorCallback,
                        "Internal error (5): Invalid argument to parser");
            return eGeneralError;
        }

        return eNoError;
    }


    // Simple buffer for printing error strings...
    const int csg_errorBufferSize = 128;
    char      sg_errorBuffer[csg_errorBufferSize];

} // namespace {}


EResult argparse::Parse(int const        argc,
                        char const**     argv,
                        Description*     pDescriptions,
                        int const        numDescriptions,
                        int*             pNumUnusedArgs,
                        ArgCallback      pArgCallback,
                        ArgErrorCallback pArgErrorCallback)
{
    // Try to hanlde input errors relatively robustly...
    EResult InputCheck = CheckInputs(argc, argv,
                                     pDescriptions, numDescriptions,
                                     pNumUnusedArgs,
                                     pArgCallback, pArgErrorCallback);
    if (InputCheck != eNoError)
        return InputCheck;

    int ArgCount = argc;

    // Loop through our arguments.  It is assumed that the [0] entry
    //  is the program name...
    ShiftDown(ArgCount, argv, 0, 1);

    // TODO: clumsy.
    for (int i = 0; i < ArgCount; /*manual advance*/)
    {
        bool foundArgumentOption = false;

        if (argv[i][0] == '-' && argv[i][1] == '-')
        {
            bool argumentFound = false;

            // Long option
            for (int j = 0; j < numDescriptions && argumentFound == false; j++)
            {
                if (pDescriptions[j].pLongDesc != NULL)
                {
                    if (strcmp(argv[i] + 2, pDescriptions[j].pLongDesc) == 0)
                    {
                        argumentFound = true;

                        // We have a match.  Note that we aren't allowed to
                        //  have an option that takes an argument unless it's
                        //  the only option for this dash.
                        if (pDescriptions[j].argumentType == eNoArg)
                        {
                            // eNoArg just calls the arg-callback.  Not really sensible if
                            // it's not present, of course.
							assert(pArgCallback);
							ShiftDown(ArgCount, argv, i, 1);
						}
                        else
                        {
                            if (GetArgument(&pDescriptions[j], argv[i + 1]) == false)
                            {
                                snprintf(sg_errorBuffer, csg_errorBufferSize,
                                          "Invalid option: Misunderstood arg %s to option %s",
                                          argv[i+1], argv[i]);
                                sg_errorBuffer[csg_errorBufferSize - 1] = '\0';
                                HandleError(pArgErrorCallback, sg_errorBuffer);
                                return eInvalidOption;
                            }

                            foundArgumentOption = true;
                            ShiftDown(ArgCount, argv, i, 2);
                        }

                        if ((*pArgCallback)(&pDescriptions[j]) == false)
                            return eCallbackFailed;
                    }
                }
            }
            if (argumentFound == false)
            {
                snprintf(sg_errorBuffer, csg_errorBufferSize, "Invalid option (%s): Option not understood", argv[i]);
                sg_errorBuffer[csg_errorBufferSize - 1] = '\0';
                HandleError(pArgErrorCallback, sg_errorBuffer);
                return eOptionNotUnderstood;
            }
        }
        else if (argv[i][0] == '-')
        {
            // Short option

            int  currentTest = 1;
            while (argv[i][currentTest] != '\0' && foundArgumentOption == false)
            {
                bool argumentFound = false;
                for (int j = 0; j < numDescriptions && argumentFound == false; j++)
                {
                    if (pDescriptions[j].shortChar == argv[i][currentTest])
                    {
                        // We have a match.  Note that we aren't allowed to
                        //  have an option that takes an argument unless it's
                        //  the only option for this dash.
                        argumentFound = true;

                        if (pDescriptions[j].argumentType == eNoArg)
                        {
                            // As above, not sensible if the arg-callback doesn't exist.
                            // DO NOT shift the argument off the list at this point.
                            // Because multiple short options may be placed together
                            // "-lascv", for instance, we have to wait until we exit the
                            // loop to shift it.
                            assert(pArgErrorCallback);
                        }
                        else
                        {
                            if (strlen(argv[i]) != 2 || i >= ArgCount - 1)
                            {
                                snprintf(sg_errorBuffer, csg_errorBufferSize,
                                          "Invalid option (%s): Options which take arguments must appear alone",
                                          argv[i]);
                                sg_errorBuffer[csg_errorBufferSize - 1] = '\0';
                                HandleError(pArgErrorCallback, sg_errorBuffer);
                                return eInvalidOption;
                            }

                            if (GetArgument(&pDescriptions[j], argv[i + 1]) == false)
                            {
                                snprintf(sg_errorBuffer, csg_errorBufferSize,
                                          "Invalid option: Misunderstood arg %s to option %s",
                                          argv[i+1], argv[i]);
                                sg_errorBuffer[csg_errorBufferSize - 1] = '\0';
                                HandleError(pArgErrorCallback, sg_errorBuffer);
                                return eInvalidOption;
                            }

                            foundArgumentOption = true;
                            ShiftDown(ArgCount, argv, i, 2);
                        }

                        if ((*pArgCallback)(&pDescriptions[j]) == false)
                            return eCallbackFailed;

                        break;
                    }
                }

                if (argumentFound == false)
                {
                    snprintf(sg_errorBuffer, csg_errorBufferSize, "Invalid option (%s): Option not understood", argv[i]);
                    sg_errorBuffer[csg_errorBufferSize - 1] = '\0';
                    HandleError(pArgErrorCallback, sg_errorBuffer);
                    return eOptionNotUnderstood;
                }

                if (foundArgumentOption)
                {
                    break;
                }

                currentTest++;
            }

	        if (foundArgumentOption == false)
			{
				// If we didn't find an argument option, then we need to shift off the flag
				// option.
				ShiftDown(ArgCount, argv, i, 1);
			}
		}
        else
        {
            // Unclaimed argument, advance the pointer manually
			++i;
        }
    }

    *pNumUnusedArgs = ArgCount;
    return eNoError;
}
