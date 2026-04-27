// =======================================================================================
// custom_format.cpp: Demonstrates using Granny's file API with your own data
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "granny.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// Forward decls.
struct EntityDatabase;

EntityDatabase* CreateTestDatabase();
void FreeTestDatabase(EntityDatabase*);
void DumpDatabaseInfo(EntityDatabase*);


/* DEFTUTORIAL EXPGROUP(TutorialCustomFormat) (TutorialCustomFormat_Introduction, Introduction) */
/* This tutorial shows you how to use Granny's file API with your own data.  This
   obviously isn't necessary in order to use Granny for just animation and model data, but
   there are several good reasons you might want to consider using the Granny file format
   for more than just what comes out of the box.  You get automatic versioning, so you
   almost never have to throw away data from a previous version of your toolchain just
   because you've changed your structures.  You also get pointer-fixups and endian
   conversion, which allow you to deal with your data as vanilla C-structures in memory,
   without worrying about allocations or byte-swapping.  Granny automatically flattens
   your data when it's written out, which lets you load your files with only a single
   allocation, if that's important to you.
   $-
   In this demo, we'll create an entity database, a list of structures that represent game
   objects to be created on level load.  We'll keep it simple, giving each entity a base
   transform, a flags member, and a string indicating the runtime class of the object.  To
   demonstrate Granny's ability to handle arbitrary reference patterns in the file
   routines, each Entity will have a SiblingObject field, that will refer to another
   object in the database.
   $-

   Prerequisites to this demo:
   $* $TutorialBasicLoading
   $* $TutorialTreeWalker
   $-

   New functions and structures discussed in this tutorial:
   $* $data_type_definition
   $* $file_builder
   $* $file_data_tree_writer
   $* $grn_file_magic_value
   $* $BeginFile
   $* $GetDataTreeFromFile
   $* $ConvertTree
   $* $GetFileTypeTag
   $-

   Functions and structures introduced incidentally:
   $* $BeginFileInMemory
   $* $GetTemporaryDirectory
   $-

   Function index:
   $* $TutorialCustomFormat_main
   $-
*/

/* First, our entity structure.  We're going to use a $transform to represent the
   position of the object, but you can easily use raw float values to encode your own
   matrices.  In general, you should always use the Granny typedefs to represent the data
   in a custom structure.  Any type except those with "x" designators may be used.  (For
   instance, $int32x.) */
struct Entity
{
    granny_transform Transform;
    granny_uint32    Flags;
    char const*      Name;
    char const*      Class;

    Entity*          SiblingObject;
};


/* We need to describe the Entity structure to Granny.  You do that with an array of
   $data_type_definition objects.  In general, the $member_type will cover just about any
   type you want, if you feel there's something missing, please let us know!  Ultimately,
   the idea is to break down each strucuture into basic datatypes, so you'll need one of
   these per user data type.  Structures can refer to each other using the
   $ReferenceMember field, or contain sub-members of a known type with $InlineMember.

   Note that the Sibling object refers to it's own data type in the $ReferenceMember
   declaration.
*/
granny_data_type_definition EntityType[] = {
    { GrannyTransformMember, "Transform" },
    { GrannyUInt32Member,    "Flags" },
    { GrannyStringMember,    "Name" },
    { GrannyStringMember,    "Class" },
    { GrannyReferenceMember, "SiblingObject",  EntityType },

    // Always the last entry in a type definition
    { GrannyEndMember }
};


/* Next we need to create the container that will act as our root object.  Each Granny
   file is essentially a graph that is rooted in a single object of a specified type.  In
   this case, we want to have a simple list of references to Entity object.  We'll add a
   second list of pointers to the objects that have the "Camera" class to demonstrate how
   Granny can handle arbitrary reference graph structures.
   $-
   Note that Granny arrays are always prefixed with a count to make iteration and
   allocation easier.  In general, it's a good idea to follow the Granny convention of
   naming these variables "[Item]Count" and "[Item]s", though this is not required.
*/
struct EntityDatabase
{
    granny_int32 EntityCount;
    Entity**     Entities;

    granny_int32 CameraCount;
    Entity**     Cameras;
};

granny_data_type_definition EntityDatabaseType[] = {
    { GrannyArrayOfReferencesMember, "Entities",  EntityType },
    { GrannyArrayOfReferencesMember, "Cameras",   EntityType },

    // Always the last entry in a type definition
    { GrannyEndMember }
};


/* DEFTUTORIAL EXPGROUP(TutorialCustomFormat) (TutorialCustomFormat_main, main) */
int main(int, char const*[])
{
    // Ensure the API version matches
    if(!GrannyVersionsMatch)
    {
        printf("Warning: the Granny DLL currently loaded "
               "doesn't match the .h file used during compilation\n");
        return EXIT_FAILURE;
    }

    /* When you're working with Granny data definitions, it's a very good idea to make
       sure that the actual size of the object is what Granny thinks it is.  In general
       each member in a Granny structure should be aligned to a 4-byte boundary.  (This
       includes doubles, and pointers on 64-bit platforms.)  One pitfall to be aware of is
       that you should use the granny_bool32 type for boolean values, rather than a
       primitive bool, since the size of the builtin type can vary from compiler to
       compiler. */
    assert(sizeof(Entity) == GrannyGetTotalObjectSize(EntityType));
    assert(sizeof(EntityDatabase) == GrannyGetTotalObjectSize(EntityDatabaseType));


    /* Now we need to create a $file_builder to write out our Database.  We'll create a
       very simple output file, so we specify a single section.  The tag is your way of
       indicating what the file type and version are at the time of writing.  We recommend
       that you choose 5 random hex digits, and represent the version in the lower 12 bits
       of the id.  This gives you 4096 versions before you run into trouble, which should
       be sufficient.

       $GRNFileMV_ThisPlatform indicates that we want the file optimized for the current
       platform.  You can specify any $grn_file_magic_value enumerant here to customize
       the output for a given platform.  For instance, if we were creating this file to be
       loaded on an Xbox 360, we would use $GRNFileMV_32Bit_BigEndian for optimum
       performance when loading on the console.  Any Granny file will load on any
       platform, but specifying the correct platform allows Granny to skip the endian
       marshalling or pointer size conversion operations, which take time.

       $GetTemporaryDirectory and "Prefix" simply indicate to Granny where temp files
       created during the file writing process should be stored.  If you don't want to
       create temp files in the filesystem, you can use $BeginFileInMemory to stage the
       entire file creation process in RAM. */

    // Create the database we'll write out.  (See the source for the details of this
    // function.)
    EntityDatabase* Database = CreateTestDatabase();

    granny_uint32 const CustomTag = 0x12345000 + 0x001;
    granny_file_builder *Builder = GrannyBeginFile(1, CustomTag,
                                                   GrannyGRNFileMV_ThisPlatform,
                                                   GrannyGetTemporaryDirectory(),
                                                   "Prefix");

    /* The actual data is written by a $file_data_tree_writer object, which we initialize
       simply by passing it the pointer to the Database, and the $data_type_definition
       array that represents the type of the Root object.  The last two parameters to
       $BeginFileDataTreeWriting indicate that by default, we want objects and type
       information placed in section 0 of the file.  Since we requested a single section
       in $BeginFile, this is the only sensible option.  It's possible to create much more
       elaborate structures, such as the sectioning scheme that the exporter creates by
       default to separate permanent from "fire-and-forget" data, but we'll leave that to
       another tutorial. */
    granny_file_data_tree_writer *Writer =
        GrannyBeginFileDataTreeWriting(EntityDatabaseType, Database, 0, 0);

    /* Write the file to "entity_database.edb" in the current directory.  It's a good idea
       to give custom formats their own extension to prevent confusion with Granny files
       created by the exporter. */
    char const* OutputFile = "entity_database.edb";

    GrannyWriteDataTreeToFileBuilder(Writer, Builder);
    GrannyEndFileDataTreeWriting(Writer);
    if (GrannyEndFile(Builder, OutputFile) == false)
    {
        fprintf(stderr, "TransformFile: failed to write to %s\n", OutputFile);
        return EXIT_FAILURE;
    }

    // We're done with the original copy of the database
    FreeTestDatabase(Database);

    /* Now we'll load the file back in, and dump the information to make sure the file was
       properly written.  This works more or less like the $ReadEntireFile/$GetFileInfo
       sequence you're already familiar with, but with the modification that we have to
       obtain a $variant that represents the root object that we wrote to the file.  If
       necessary, we also use $ConvertTree to upgrade the file from older versions. */
    EntityDatabase* LoadedDB = NULL;
    void* ConversionBuffer = NULL;

    granny_file* File = GrannyReadEntireFile(OutputFile);
    granny_variant RootObject;
    GrannyGetDataTreeFromFile(File, &RootObject);

    // Check for version
    if (GrannyGetFileTypeTag(File) == CustomTag)
    {
        // Version is current.  We can take the object field and cast it to our type
        LoadedDB = (EntityDatabase*)RootObject.Object;
    }
    else
    {
        // This is an older version, convert.
        ConversionBuffer = GrannyConvertTree(RootObject.Type, RootObject.Object,
                                             EntityDatabaseType, 0, 0);
        LoadedDB = (EntityDatabase*)ConversionBuffer;
    }

    // Print the info contained in the database
    DumpDatabaseInfo(LoadedDB);

    /* Cleanup is like loading a regular file, you simply need to call $FreeFile on the
       result of $ReadEntireFile.  There is a caveat that you must call $FreeBuilderResult
       on the conversion buffer if you used $ConvertTree to upgrade the file's version.
       When using standard Granny files (i.e, with $GetFileInfo), this is handled
       automatically.  For custom formats, you must add the check yourself. */
    if (ConversionBuffer != NULL)
        GrannyFreeBuilderResult(ConversionBuffer);
    GrannyFreeFile(File);

    return EXIT_SUCCESS;
}

/* DEFTUTORIAL EXPGROUP(TutorialCustomFormat) (TutorialCustomFormat_Summary, Summary) */
/* You can now use Granny's versioning, pointer-fixup, and endian marshalling routines to
   reprsent just about anything you want in your engine.  Remember that while we replaced
   the entire format with our own, in this example, you can always use the same concepts
   to attach smaller pieces of data to the existing Granny format by leveraging the
   ExtendedData fields in many of the Granny structures.  From here, you might want to
   explore:

   $* $TutorialPreprocessor: use the Granny preprocessor for common toolchain tasks.
   $-
*/

/* DEFTUTORIALEND */


EntityDatabase* CreateTestDatabase()
{
    EntityDatabase* RetVal = new EntityDatabase;

    // This is hardcoded just for simplicity.  We'll create 3 objects, 2 of which are
    // cameras, which must also be noted in the Cameras member of the Database.
    RetVal->EntityCount = 3;
    RetVal->Entities = new Entity*[3];

    RetVal->CameraCount = 2;
    RetVal->Cameras = new Entity*[2];

    // Object name: "Camera01", no sibling
    RetVal->Entities[0] = new Entity;
    GrannyMakeIdentity(&RetVal->Entities[0]->Transform);
    RetVal->Entities[0]->Transform.Position[0] = 1.0f;
    RetVal->Entities[0]->Flags = 0x0;
    RetVal->Entities[0]->Name  = "Camera01";
    RetVal->Entities[0]->Class = "Camera";
    RetVal->Entities[0]->SiblingObject = NULL;

    // Object name: "Spawn01", sibling = Camera01
    RetVal->Entities[1] = new Entity;
    GrannyMakeIdentity(&RetVal->Entities[1]->Transform);
    RetVal->Entities[1]->Transform.Position[1] = 1.0f;
    RetVal->Entities[1]->Flags = 0xbaddf00d;
    RetVal->Entities[1]->Name  = "Spawn01";
    RetVal->Entities[1]->Class = "SpawnPoint";
    RetVal->Entities[1]->SiblingObject = RetVal->Entities[0];


    // Object name: "Camera02", sibling = Camera02 (self reference)
    RetVal->Entities[2] = new Entity;
    GrannyMakeIdentity(&RetVal->Entities[2]->Transform);
    RetVal->Entities[2]->Transform.Position[2] = 1.0f;
    RetVal->Entities[2]->Flags = 0xdeadbeef;
    RetVal->Entities[2]->Name  = "Camera02";
    RetVal->Entities[2]->Class = "Camera";
    RetVal->Entities[2]->SiblingObject = RetVal->Entities[2];

    // Normally, you'd extract these programatically, but we happen to know exactly which
    // are the cameras.
    RetVal->Cameras[0] = RetVal->Entities[0];
    RetVal->Cameras[1] = RetVal->Entities[2];

    return RetVal;
}

void FreeTestDatabase(EntityDatabase* Database)
{
    assert(Database);

    // Only the entities are important, the cameras point to those objects, so we can
    // safely ignore them.
    {for(granny_int32x i = 0; i < Database->EntityCount; ++i)
    {
        delete Database->Entities[i];
    }}

    delete [] Database->Entities;
    delete [] Database->Cameras;
    delete Database;
}

void DumpDatabaseInfo(EntityDatabase* Database)
{
    assert(Database);

    printf("%d Entities\n", Database->EntityCount);
    {for(granny_int32x Idx = 0; Idx < Database->EntityCount; ++Idx)
    {
        Entity* ThisEntity = Database->Entities[Idx];
        assert(ThisEntity);

        printf(
            "  %s (%s) [%p]\n"
            "   Flags:   0x%x\n"
            "   Pos:     [%f %f %f]\n"
            "   Sibling: %s\n\n",
            ThisEntity->Name, ThisEntity->Class, ThisEntity,
            ThisEntity->Flags,
            ThisEntity->Transform.Position[0],
            ThisEntity->Transform.Position[1],
            ThisEntity->Transform.Position[2],
            ThisEntity->SiblingObject ? ThisEntity->SiblingObject->Name : "NULL");
    }}

    printf("%d Cameras\n", Database->CameraCount);
    {for(granny_int32x Idx = 0; Idx < Database->CameraCount; ++Idx)
    {
        Entity* ThisEntity = Database->Cameras[Idx];
        assert(ThisEntity);

        printf("  %s (%s) [%p]\n",
               ThisEntity->Name, ThisEntity->Class, ThisEntity);
    }}
}

