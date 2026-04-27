// =======================================================================================
// grn.cpp: Demonstrates how to walk the Granny object metadata
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "granny.h"
#include "ml_argparse.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


// Controlled with -printlarge
bool g_alwaysPrintLarge = false;
bool g_includePointers  = true;

// Forward declarations for our object dumpers
void Indent(int Level);
void PrintFileInfo(char const *FileName, granny_file &GRNFile);

void PrintObject(int Level, granny_pointer_hash *ObjectHash, granny_pointer_hash *TypeHash,
                 void const *Object, granny_data_type_definition const *Type,
                 bool IgnoreHashing);
void PrintType(int Level, granny_pointer_hash *TypeHash,
               granny_data_type_definition const *Type);
void PrintArray(int Level, granny_pointer_hash *ObjectHash, granny_pointer_hash *TypeHash,
                char const *Name, int ArrayCount,
                granny_data_type_definition const *Type, void *ObjectInit,
                bool IgnoreHashing);
void PrintTransform(int Level, granny_transform const &Transform);

template <class scalar_type>
void PrintScalarMember(int Level,
                       granny_data_type_definition const* Type,
                       void const* Data,
                       char const* FormatCode);
void PrintReal16Member(int Level,
                       granny_data_type_definition const* Type,
                       void const* Data);


static bool
ArgCallback(const argparse::Description* Description)
{
    // We only care about the trigger options in this callback.
    if (Description->argumentType == argparse::eNoArg &&
        Description->shortChar == 'l')
    {
        g_alwaysPrintLarge = true;
    }
    else if (Description->argumentType == argparse::eNoArg &&
             Description->shortChar == 'p')
    {
        g_includePointers = false;
    }

    return true;
}

static void
ErrorCallback(char const* ErrorString)
{
    fprintf(stderr, "%s\n", ErrorString);
}


/* DEFTUTORIAL EXPGROUP(TutorialTreeWalker) (TutorialTreeWalker_Introduction, Introduction) */
/* The Granny file format is at its core, a way of storing a data tree rooted in a single
   object.  The object's data is written out together with type information that makes it
   possible to store arbitrary data at any point in the tree.  (You've already seen one
   example of this in $TutorialExtendedData.)  This allows you to write tools that are
   extremely robust with respect to file format changes, you can either request that
   Granny automatically perform the conversion, as we do when converting a file from a
   previous version of the exporter, or you can take full control, and use
   reflection-style programming to dynamically make determinations about what the file
   contains.

   As a for instance, $TransformFile makes an attempt to change the data contained in
   $mesh vertices and animation curves to match the requested coordinate system, but for
   efficiency reasons, doesn't try to find extended data members that may need to be
   converted as well.  Using the $data_type_definition metadata, you can easily write
   functions that traverse every data member in the file, looking for $(transform)s that
   will handle every ExtendedData transform, position, or orientation member.

   You could also use the metadata to create custom editors that allow your artists or
   programmers to modify values in the Granny file directly.  There is enough information
   here to feed to a GUI creation system that can create human-readable interfaces
   directly from your game-ready binary files.  The Granny Viewer uses this technique to
   create its detailed data views.

   <b>Note:</b> This tutorial contains the source for the grn.exe utility referenced in
   various places in the Granny documentation.  This is an extremely handy little utility,
   you should make sure to build this project, and add it to your toolbox.  This also
   means that there are some details like argument parsing that may be safely ignored for
   the purposes of learning about walking the data description tree.

   Prerequisites to this demo:
   $* $TutorialBasicLoading
   $* $TutorialExtendedData
   $-

   Function index:
   $* $TutorialTreeWalker_main
   $* $TutorialTreeWalker_PrintObject
   $* $TutorialTreeWalker_PrintType
   $* $TutorialTreeWalker_PrintScalarMember
   $-
*/


/* DEFTUTORIAL EXPGROUP(TutorialTreeWalker) (TutorialTreeWalker_main, main) */
int main(int argc, char const** argv)
{
    /* Application setup including argument parsing.  You can more or less ignore this
       section. */
    // Ensure the API version matches
    if(!GrannyVersionsMatch)
    {
        printf("Warning: the Granny DLL currently loaded "
               "doesn't match the .h file used during compilation\n");
        return EXIT_FAILURE;
    }

    int Result = EXIT_FAILURE;
    char const* EXEName = argv[0];

    char StringDatabaseName[512] = { 0 };
    argparse::Description ArgDesc[] = {
        { 'p', "pointerLess",    0, argparse::eNoArg },
        { 'l', "printLong",      0, argparse::eNoArg },
        { 0,   "stringDatabase", 0, argparse::eFilename, StringDatabaseName,  sizeof(StringDatabaseName) }
    };

    int RemainingArgs = -1;
    if (argparse::Parse(argc, argv, ArgDesc, 3, &RemainingArgs,
                        ArgCallback, ErrorCallback) != argparse::eNoError)
    {
        printf("Unable to parse argument list\n");
        return EXIT_FAILURE;
    }

    if (RemainingArgs == 1)
    {
        char const* FileName = argv[0];

        /* The tree walking functions use two hash tables to keep track of objects and
           types that have already been printed, to prevent loops, or duplication in the
           output.  A $pointer_hash essentially maps pointers to blocks of blind data.  In
           this case, we're simply storing NULL at each visited pointer, which allows us
           to test if the object has been visited with $HashedPointerKeyExists.
        */
        granny_pointer_hash *ObjectHash = GrannyNewPointerHash();
        granny_pointer_hash *TypeHash   = GrannyNewPointerHash();
        assert(ObjectHash && TypeHash);

        granny_file* GRNFile = GrannyReadEntireFile(FileName);
        granny_file* SDBFile = 0;
        if(GRNFile)
        {
            // Remap to the string database, if necessary.
            if (StringDatabaseName[0] != '\0')
            {
                SDBFile = GrannyReadEntireFile(StringDatabaseName);
                if (SDBFile)
                {
                    granny_string_database* SDB = GrannyGetStringDatabase(SDBFile);
                    assert(SDB);
                    GrannyRemapFileStrings(GRNFile, SDB);
                }
                else
                {
                    // warn
                }
            }

            // Dump the file and section info
            PrintFileInfo(FileName, *GRNFile);

            /* Note that we're not calling $GetFileInfo here.  By using
               $GetDataTreeFromFile, we can dump any data that's been properly written
               with a $file_builder.  In $TutorialCustomFormat, you'll see how to create
               data trees that make no reference to internal Granny structures.  This tool
               will still be able to dump them as text.
            */
            granny_variant Root;
            GrannyGetDataTreeFromFile(GRNFile, &Root);

            // We can't walk the type tree if it doesn't exist.
            if(Root.Type)
            {
                // Ditto for the object.  It's possible to create $file objects that
                // contain only type information using the $data_tree_writer API, though
                // they have limited application.
                if(Root.Object)
                {
                    Indent(1); printf("Object tree:\n");
                    PrintObject(2, ObjectHash, TypeHash,
                                Root.Object, Root.Type, false);
                }
                else
                {
                    Indent(1); printf("(no root object)\n");
                }

                Indent(1); printf("Type tree:\n");
                PrintType(2, TypeHash, Root.Type);

                /* You can safely ignore the rest of the function, we're just cleaning up,
                   and handling errors from this point on. */
            }
            else
            {
                Indent(1); printf("(no root type)\n");
            }

            GrannyFreeFile(GRNFile);

            Result = EXIT_SUCCESS;
        }
        else
        {
            printf("%s: unable to read %s\n", EXEName, FileName);
        }

        GrannyDeletePointerHash(ObjectHash);
        GrannyDeletePointerHash(TypeHash);
    }
    else
    {
        printf("Usage: %s [-l/--printLong] [-p/--pointerLess] [--stringDatabase <name>.gsd] <granny file>\n", EXEName);
    }

    return (Result);
}

static void CapWithPointer(void* Object)
{
    if (g_includePointers)
        printf("(ptr: %p)\n", Object);
    else
        printf("(ptr: omitted)\n");
}



/* DEFTUTORIAL EXPGROUP(TutorialTreeWalker) (TutorialTreeWalker_PrintObject, PrintObject) */
/* This is the meaty function in the example.  This takes in a pointer to the object, and
   the $data_type_definition array that describes it.  From there, it's just a big switch
   statement to route each member to the correct code to dump it to the output. */
void PrintObject(int Level,
                 granny_pointer_hash *ObjectHash,
                 granny_pointer_hash *TypeHash,
                 void const *Object,
                 granny_data_type_definition const *ObjectType,
                 bool IgnoreHashing)
{
    /* Before we print the object, we need to make sure that we haven't already visited
       it.  The reason for the IgnoreHashing test will become clear below.
    */
    if (!IgnoreHashing)
    {
        if(GrannyHashedPointerKeyExists(ObjectHash, Object))
        {
            Indent(Level); printf("(object already printed)\n");
            return;
        }
        else
        {
            GrannyAddPointerToHash(ObjectHash, Object, 0);
        }
    }

    /* Ok, now we walk the type array.  It's convenient to get a pointer to the Object
       data as bytes, since we have to advance it after each member is handled.

       $GetMemberTypeSize will tell you the correct size for the entire member, while the
       related function $GetMemberUnitSize, will tell you the size of one <i>element</i>
       of a member.  For instance if your member is: $int32 A[4], $GetMemberTypeSize ==
       16, while $GetMemberUnitSize == 4.
    */
    granny_uint8 *ObjectPtr = (granny_uint8 *)Object;
    {for(granny_data_type_definition const *Type = ObjectType;
         Type && Type->Type != GrannyEndMember;
         ObjectPtr += GrannyGetMemberTypeSize(Type), ++Type)
    {
        switch(Type->Type)
        {
            /* A $ReferenceMember entry indicates that the member is a pointer to another
               object in the file.  The $data_type_definition array that describes that
               object is found in the $data_type_definition::ReferenceType field.  We
               simply recover the pointer, and recurse to print the object, increasing the
               ident level.  For an example of this member type, see the "ArtToolInfo"
               field of $file_info.

               Note that we're printing the pointer values for object to make it easier to
               cross-reference results in the output.  These values are transient, and
               shouldn't be considered part of the file itself.  The pattern (ptr: xxx) is
               designed to make it easy to grep for the pointer values, or filter them out
               if you want to write a diff-ing utility.
            */
            case GrannyReferenceMember:
            {
                Indent(Level);
                void *InternalObject = *(void **)ObjectPtr;
                if(InternalObject)
                {
                    printf("%s: ", Type->Name);
                    CapWithPointer(InternalObject);
                    PrintObject(Level + 1, ObjectHash, TypeHash,
                        InternalObject, Type->ReferenceType, false);
                }
                else
                {
                    printf("%s = 0\n", Type->Name);
                }
            } break;

            /* Very similar to a $ReferenceMember, but this time, we're pointing to an
               array of objects of type "ReferenceType".  The member itself consist of two
               fields (int32 Count) and (void* Array).  For an example of this, check out
               the $skeleton data type.  The "BoneCount" and "Bones" field are a
               $ReferenceToArrayMember.

               In your own data types, we recommend that you use the "XCount", "Xs" naming
               convention, to make it clear from inspection that the fields are related.
            */
            case GrannyReferenceToArrayMember:
            {
                int ArrayCount = *(granny_int32 *)ObjectPtr;
                granny_uint8 *InternalObject =
                    *(granny_uint8 **)(ObjectPtr + sizeof(granny_int32));
                PrintArray(Level, ObjectHash, TypeHash,
                           Type->Name, ArrayCount,
                           Type->ReferenceType, InternalObject, false);
            } break;

            /* One more level of indirection.  A $ArrayOfReferencesMember points to an
               array of pointers to other objects.  For an example, see the "TextureCount"
               and "Textures" fields of $file_info.  The member is (int32 Count) plus
               (void** ObjectArray).  It's OK for individual entries in the array to be
               NULL, Granny will handle it in the export and recreate the NULL fields at
               load time.  When you're traversing one of these members, make sure to check
               all of the pointers as you encounter them.
            */
            case GrannyArrayOfReferencesMember:
            {
                Indent(Level);
                int ArrayCount = *(granny_int32 *)ObjectPtr;
                void **InternalObject =
                    *(void ***)(ObjectPtr + sizeof(granny_int32));

                if(InternalObject)
                {
                    printf("%s[%d]: ", Type->Name, ArrayCount);
                    CapWithPointer(InternalObject);
                    {for(int ArrayIndex = 0;
                         ArrayIndex < ArrayCount;
                         ++ArrayIndex)
                    {
                        Indent(Level + 1);
                        if(*InternalObject)
                        {
                            printf("%s[%d]: ", Type->Name, ArrayIndex);
                            CapWithPointer(*InternalObject);
                            PrintObject(Level + 2, ObjectHash, TypeHash,
                                        *InternalObject, Type->ReferenceType, false);
                        }
                        else
                        {
                            printf("%s[%d] = 0\n", Type->Name, ArrayIndex);
                        }
                        ++InternalObject;
                    }}
                }
                else
                {
                    printf("%s = 0\n", Type->Name);
                }
            } break;

            /* This member type will be familiar to you if you've already read
               $TutorialExtendedData.  $(VariantReferenceMember)s wrap data whose complete
               type isn't known at compile time.  The member (which you should declare as
               a $variant in your data structure) is a pointer to the object, followed by
               a pointer to the type information.  Since the $variant::Type field isn't a
               part of the tree reachable by traversing ReferenceType fields in the Root
               $data_type_definition array, we print that directly in the output here.

               See the "ExtendedData" field of $file_info for an example of this type.
            */
            case GrannyVariantReferenceMember:
            {
                Indent(Level);
                granny_data_type_definition *VariantType =
                    *(granny_data_type_definition **)ObjectPtr;
                void *InternalObject =
                    *(void **)(ObjectPtr + sizeof(granny_data_type_definition *));
                if(InternalObject)
                {
                    printf("%s: ", Type->Name);
                    CapWithPointer(InternalObject);

                    Indent(Level + 1); printf("Type: "); CapWithPointer(VariantType);
                    PrintType(Level + 2, TypeHash, VariantType);
                    Indent(Level + 1); printf("Value:\n");
                    PrintObject(Level + 2, ObjectHash, TypeHash,
                        InternalObject, VariantType, false);
                }
                else
                {
                    printf("%s = 0\n", Type->Name);
                }
            } break;

            /* This member will be encountered extremely rarely, Granny itself only uses
               it in a few situations.  As $ReferenceToArrayMember is to $ReferenceMember,
               $ReferenceToVariantArrayMember is to $VariantReferenceMember.  I.e., this
               is a pointer to an array of objects whose type is specified dynamically.
               Granny uses this for large blocks of dynamic, but identical data, such as
               vertex annotations in Max or Maya.

               See the "VertexType", "VertexCount", and "Vertices" members of $vertex_data
               for an example.
            */
            case GrannyReferenceToVariantArrayMember:
            {
                granny_data_type_definition *VariantType =
                    *(granny_data_type_definition **)ObjectPtr;
                if(VariantType)
                {
                    Indent(Level);
                    printf("%s type:\n", Type->Name);
                    PrintType(Level + 1, TypeHash, VariantType);
                }

                int Count =
                    *(int *)(ObjectPtr + sizeof(granny_data_type_definition *));
                void *InternalObject =
                    *(void **)(ObjectPtr + sizeof(granny_data_type_definition *) + sizeof(int));
                PrintArray(Level, ObjectHash, TypeHash,
                           Type->Name, Count, VariantType, InternalObject, false);
            } break;

            /* An $InlineMember is a structure described by a distinct
               $data_type_definition array, but contained directly in your structure,
               rather than referred to by pointer.  As an example: "PixelLayout" in
               $texture.

               When you have a contained structure like this, you can either use an inline
               member to describe it, or put its fields directly into the type array of
               the containing object.  $InlineMember is the recommended method, since you
               can leverage the type description to make entries in other types, rather
               than duplicating the type descriptors.

               Note that this is where the IgnoreHashing flag comes into play.  The
               $InlineMember isn't a pointer value that will come up again in the file, so
               there's no need to put it into the pointer hash we're using to track
               printed objects.
            */
            case GrannyInlineMember:
            {
                if(Type->ArrayWidth)
                {
                    PrintArray(Level, ObjectHash, TypeHash,
                               Type->Name, Type->ArrayWidth,
                               Type->ReferenceType, ObjectPtr, true);
                }
                else
                {
                    Indent(Level);
                    printf("%s:\n", Type->Name);

                    PrintObject(Level + 1, ObjectHash, TypeHash,
                                ObjectPtr, Type->ReferenceType, true);
                }
            } break;

            /* Granny has a special member type to describe transforms, since they're so
               fundamental.  Technically, we could achieve the same results with an
               $InlineMember referencing an appropriate type array, but a transform is
               really a primitive type for our purposes.  Check "LocalTransform" in $bone
               for an example.  If you use the $TransformMember type, make sure to declare
               the member as a $transform.
            */
            case GrannyTransformMember:
            {
                granny_transform &Transform =
                    *(granny_transform *)ObjectPtr;
                Indent(Level); printf("%s (granny_transform):\n", Type->Name);
                PrintTransform(Level + 1, Transform);
            } break;

            /* char* member.  Fairly self explanatory, see "FromFileName" in
               $file_info. */
            case GrannyStringMember:
            {
                char *String = *(char **)ObjectPtr;
                Indent(Level);
                if (String)
                    printf("%s: \"%s\"\n", Type->Name, String);
                else
                    printf("%s: (null)\n", Type->Name);
            } break;

            /* You won't find templates anywhere in the Granny API, but we've used them
               here to make the example clearer.  The next several cases deal with
               primitive types.  Check $TutorialTreeWalker_PrintScalarMember for the
               details of how we're dumping them.  The scalar types encode members of the
               form (int Foo) or (int Foo[Count]).  For an example, see the "ParentIndex"
               field of $skeleton, or the "ShiftForComponent" field of $pixel_layout.
            */
            case GrannyUInt8Member:
            case GrannyNormalUInt8Member:
                PrintScalarMember<granny_uint8>(Level, Type, ObjectPtr, "%u");
                break;

            case GrannyInt8Member:
            case GrannyBinormalInt8Member:
                PrintScalarMember<granny_int8>(Level, Type, ObjectPtr, "%d");
                break;

            case GrannyUInt16Member:
            case GrannyNormalUInt16Member:
                PrintScalarMember<granny_uint16>(Level, Type, ObjectPtr, "%u");
                break;

            case GrannyInt16Member:
            case GrannyBinormalInt16Member:
                PrintScalarMember<granny_int16>(Level, Type, ObjectPtr, "%d");
                break;

            case GrannyUInt32Member:
                PrintScalarMember<granny_uint32>(Level, Type, ObjectPtr, "%u");
                break;

            case GrannyInt32Member:
                PrintScalarMember<granny_int32>(Level, Type, ObjectPtr, "%d");
                break;

            case GrannyReal32Member:
            {
                PrintScalarMember<granny_real32>(Level, Type, ObjectPtr, "%f");
            } break;

            case GrannyReal16Member:
            {
                PrintReal16Member(Level, Type, ObjectPtr);
            } break;

            case GrannyEmptyReferenceMember:
            {
                Indent(Level);
                printf("(void*)%s = [empty reference]\n", Type->Name);
            } break;

            /* Catch any bogus values in the Type field.  We should never actually execute
               this code.
            */
            default:
            {
                Indent(Level); printf("(ERROR! unrecognized member)\n");
            } break;
        }
    }}
}


/* DEFTUTORIAL EXPGROUP(TutorialTreeWalker) (TutorialTreeWalker_PrintType, PrintType) */
/* Create a human-readable version of a $data_type_definition array.  This is much less
   involved than $TutorialTreeWalker_PrintObject, so we'll keep the comments inline.
*/
void PrintType(int Level,
               granny_pointer_hash *TypeHash,
               granny_data_type_definition const *TypeArray)
{
    if(GrannyHashedPointerKeyExists(TypeHash, TypeArray))
    {
        Indent(Level); printf("(type already printed)\n");
        return;
    }
    GrannyAddPointerToHash(TypeHash, TypeArray, 0);

    {for (granny_data_type_definition const* Type = TypeArray;
          Type && Type->Type != GrannyEndMember;
          ++Type)
    {
        // Given a granny_member_type enumerant, GrannyGetMemberCTypeName returns a
        // readable description of the type, for instance "void*", "granny_real32", etc.
        const char* TypeString = GrannyGetMemberCTypeName(Type->Type);

        Indent(Level); printf("%s %s", TypeString, Type->Name);
        if(Type->ArrayWidth)
        {
            printf("[%d]", Type->ArrayWidth);
        }

        // If the type entry contains type information about the referenced data, print
        // that out as well, at a higher indent level.
        if(GrannyMemberHasPointers(Type) && Type->ReferenceType)
        {
            printf(":\n");
            PrintType(Level + 1, TypeHash, Type->ReferenceType);
        }
        else
        {
            printf("\n");
        }
    }}
}


/* DEFTUTORIAL EXPGROUP(TutorialTreeWalker) (TutorialTreeWalker_PrintScalarMember, PrintScalarMember) */
/* Another small function, designed to flexibly dump scalar data found in the file.
   There's a subtlety to be aware of here, it's not possible to distinguish between
   "$int32 Foo" and "$int32 Foo[1]".  Granny allows the ArrayWidth field of the type
   descriptor to be either 0 or 1 to describe a unitary member.

   If you examine the Granny source, you'll find that typically, members such as "int Foo"
   leave the ArrayWidth field uninitialized, which causes the compiler to zero them out.
   Just something to be aware of.
*/
template <class scalar_type>
void PrintScalarMember(int Level,
                       granny_data_type_definition const* Type,
                       void const* Data,
                       char const* FormatCode)
{
    Indent(Level); printf("%s: ", Type->Name);

    if(Type->ArrayWidth)
    {
        printf("{");

        scalar_type const *Integer = (scalar_type const*)Data;
        {for(int Index = 0; Index < Type->ArrayWidth; ++Index, ++Integer)
        {
            printf("%s", Index ? ", " : "");
            printf(FormatCode, *Integer);
        }}

        printf("}\n");
    }
    else
    {
        scalar_type Integer = *(scalar_type const*)Data;
        printf(FormatCode, Integer);
        printf("\n");
    }
}


/* Real16 members need to be decoded before handing them to printf, so handle that
   here. */
void
PrintReal16Member(int Level,
                  granny_data_type_definition const* Type,
                  void const* Data)
{
    Indent(Level); printf("%s: ", Type->Name);

    if(Type->ArrayWidth)
    {
        printf("{");

        granny_real16 const* Array = (granny_real16 const*)Data;
        {for(int Index = 0; Index < Type->ArrayWidth; ++Index, ++Array)
        {
            printf("%s", Index ? ", " : "");

            granny_real32 Scalar;
            GrannyReal16ToReal32(*Array, &Scalar);
            printf("%f (16b)", Scalar);
        }}

        printf("}\n");
    }
    else
    {
        granny_real32 Scalar;
        GrannyReal16ToReal32(*(granny_real16 const*)Data, &Scalar);
        printf("%f (16b)\n", Scalar);
    }
}


/* DEFTUTORIAL EXPGROUP(TutorialTreeWalker) (TutorialTreeWalker_Summary, Summary) */
/* On the list of underutilized Granny features, the file api is right at the top.  By
   referring to this example, it should be possible for you to write any sort of data
   inspection or editing utility you might need.

   From here, you might want to check out:
   $* $TutorialCustomFormat: take full control of the Granny file format
   $* $TutorialPreprocessor: use the preprocessor to filter out parts of the object tree
      that you don't want.
   $-
*/

/* DEFTUTORIALEND */



void Indent(int Level)
{
    while(Level--)
    {
        printf("    ");
    }
}

// Uninteresting dump of the file data.  Not much to see here.
void PrintFileInfo(char const *FileName, granny_file &GRNFile)
{
    granny_int32x PointerSize;
    bool IsLittleEndian;
    GrannyGetMagicValueProperties(GRNFile.SourceMagicValue,
                                  &PointerSize, &IsLittleEndian);
    printf("File: %s (%d-bit pointers, %s-endian)\n",
           FileName,
           PointerSize,
           IsLittleEndian ? "little" : "big");
    Indent(1); printf("Version: %d\n", GRNFile.Header->Version);
    Indent(1); printf("Tag: 0x%x\n", GRNFile.Header->TypeTag);
    Indent(1); printf("Total Size: %d\n", GRNFile.Header->TotalSize);
    Indent(1); printf("CRC: 0x%x\n", GRNFile.Header->CRC);

    int const SectionCount = GRNFile.Header->SectionArrayCount;
    Indent(1); printf("Sections [%d]:\n", SectionCount);
    {for(int SectionIndex = 0;
         SectionIndex < SectionCount;
         ++SectionIndex)
    {
        granny_grn_section const &Section =
            GrannyGetGRNSectionArray(GRNFile.Header)[SectionIndex];

        Indent(2); printf("Section %d: (Base addr: %p)\n", SectionIndex, GRNFile.Sections[SectionIndex]);
        Indent(3); printf("Format: %d\n", Section.Format);
        Indent(3); printf("DataOffset: 0x%x\n", Section.DataOffset);
        Indent(3); printf("DataSize: %d\n", Section.DataSize);
        Indent(3); printf("ExpandedDataSize: %d\n", Section.ExpandedDataSize);
        Indent(3), printf("InternalAlignment: %d\n", Section.InternalAlignment);
        Indent(3); printf("First16Bit: %d\n", Section.First16Bit);
        Indent(3); printf("First8Bit: %d\n", Section.First8Bit);
        Indent(3); printf("PointerFixupArrayOffset: 0x%x\n",
                          Section.PointerFixupArrayOffset);
        Indent(3); printf("PointerFixupArrayCount: %d\n",
                          Section.PointerFixupArrayCount);
        Indent(3); printf("MixedMarshallingFixupArrayOffset: 0x%x\n",
                          Section.MixedMarshallingFixupArrayOffset);
        Indent(3); printf("MixedMarshallingFixupArrayCount: %d\n",
                          Section.MixedMarshallingFixupArrayCount);
    }}
}

void PrintArray(int Level,
                granny_pointer_hash *ObjectHash, granny_pointer_hash *TypeHash,
                char const *Name, int ArrayCount,
                granny_data_type_definition const *Type,
                void *ObjectInit,
                bool IgnoreHashing)
{
    unsigned char *Object = (unsigned char *)ObjectInit;

    Indent(Level);
    if(Object)
    {
        printf("%s[%d]: ", Name, ArrayCount);
        CapWithPointer(Object);

        if (g_alwaysPrintLarge || ArrayCount < 512)
        {
            int ObjectSize = GrannyGetTotalObjectSize(Type);
            {for(int ArrayIndex = 0;
                 ArrayIndex < ArrayCount;
                 ++ArrayIndex)
            {
                Indent(Level + 1);
                printf("%s[%d]:\n", Name, ArrayIndex);
                PrintObject(Level + 2, ObjectHash, TypeHash,
                            Object, Type, IgnoreHashing);
                Object += ObjectSize;
            }}
        }
        else
        {
            Indent(Level + 1);
            printf("(array suppressed due to length)\n");
        }
    }
    else
    {
        printf("%s = 0\n", Name);
    }
}


// Straightforward dump of a $transform.
void PrintTransform(int Level, granny_transform const &Transform)
{
    Indent(Level);
    printf("Flags: 0x%x\n", Transform.Flags);

    Indent(Level);
    printf("Position: {%f, %f, %f}\n",
           Transform.Position[0],
           Transform.Position[1],
           Transform.Position[2]);

    Indent(Level);
    printf("Orientation: {%f, %f, %f, %f}\n",
           Transform.Orientation[0],
           Transform.Orientation[1],
           Transform.Orientation[2],
           Transform.Orientation[3]);

    Indent(Level);
    printf("Scale: {{%f, %f, %f}, {%f, %f, %f}, {%f, %f, %f}}\n",
           Transform.ScaleShear[0][0],
           Transform.ScaleShear[0][1],
           Transform.ScaleShear[0][2],
           Transform.ScaleShear[1][0],
           Transform.ScaleShear[1][1],
           Transform.ScaleShear[1][2],
           Transform.ScaleShear[2][0],
           Transform.ScaleShear[2][1],
           Transform.ScaleShear[2][2]);
}





