// =======================================================================================
// Copyright 2011 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "gstate_test.h"

#include "gstate.h"

// The specific nodes we need
#include "gstate_state_machine.h"
#include "gstate_parameters.h"

#include <assert.h>
#include <map>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>


using namespace std;
USING_GSTATE_NAMESPACE;

// Our logging callback
static GRANNY_CALLBACK(granny_file_info*) SourceBindingCallback(gstate_character_info* BindingInfo,
                                                                char const* SourceName,
                                                                void* UserData);
GRANNY_CALLBACK(void) LogCallback(granny_log_message_type Type,
                                  granny_log_message_origin Origin,
                                  char const* File, granny_int32x Line,
                                  char const * Message,
                                  void * UserData);

#define SourcePrefix "media\\gstate\\"

char const* const GlobalSceneFilename     = SourcePrefix "sedna.gr2";
char const* const GlobalCharacterFilename = SourcePrefix "simple.gsf";

// ---------------------------------------------------------
// Scene instance objects
extern DemoScene* GlobalScene;

void ReleaseCharacter()
{
    GStateFreeCharacterInstance(GlobalScene->Instance);
    GrannyFreeFile(GlobalScene->CharacterFile);
    GlobalScene->Instance = 0;
    GlobalScene->CharacterFile = 0;
    GlobalScene->CharacterInfo = 0;
}

bool LoadAndBindCharacter()
{
    assert(GlobalScene->Models.size() == 1);

    if (!GStateReadCharacterInfo(GlobalCharacterFilename,
                                 GlobalScene->CharacterFile,
                                 GlobalScene->CharacterInfo))
    {
        return false;
    }

    if (!GStateBindCharacterFileReferences(GlobalScene->CharacterInfo, SourceBindingCallback, 0))
    {
        return false;
    }

    GlobalScene->Instance = GStateInstantiateCharacter(GlobalScene->CharacterInfo, 0, 0,
                                                       GrannyGetSourceModel(GlobalScene->Models[0]->ModelInstance));
    if (!GlobalScene->Instance)
        return false;

    // Success!
    return true;
}


// Tiny API for the Source map
void NoteSource(char const* SourceName,
                granny_file* File,
                granny_file_info* Info);
granny_file_info* LookupSource(char const* SourceName);

static GRANNY_CALLBACK(granny_file_info*)
SourceBindingCallback(gstate_character_info* BindingInfo,
                      char const*     SourceName,
                      void*           /*UserData*/)
{
    /* See if we've already loaded this source */
    granny_file_info* Source = LookupSource(SourceName);
    if (Source)
        return Source;

    string NameToLoad = SourcePrefix;
    NameToLoad += SourceName;

    granny_file* File = GrannyReadEntireFile(NameToLoad.c_str());
    if (!File)
        return 0;

    Source = GrannyGetFileInfo(File);
    if (Source)
    {
        // Make the new entry in the source map so we don't reload this later
        NoteSource(SourceName, File, Source);
    }

    return Source;
}

/* DEFTUTORIAL EXPGROUP(TutorialDAG2) (TutorialDAG2_KeyEvent, KeyEvent) */
/* Once we have the Graph loaded and bound, it's not much good unless we play around with
   the parameters at runtime, obviously.  dag2_helper.cpp routes key events to this
   function, where we'll just detect a few mnemonics, and use that to change the state of
   the "Motions" state machine node in the example Graph.  If you've replaced the example
   graph with your own blend structure, you may want to plug in some manipulation tests
   here for convenience.
 */
void KeyEvent(char Key)
{
    state_machine* Machine = GStateGetStateMachine(GlobalScene->Instance);
    assert(Machine);

    if (isalpha(Key))
    {
        char* StateName = 0;
        char* EdgeName  = 0;
        switch (toupper(Key))
        {
            case 'L': StateName = "Locomotion"; break;
            case 'I': StateName = "Idling";     break;
            case 'A': StateName = "SwingSpear"; break;
            case 'D': EdgeName  = "takedamage"; break;
            case 'S': StateName = "Spawn";      break;
            case 'K': StateName = "IK";         break;
        }

        if (StateName)
        {
            if (!Machine->RequestChangeToState(GStateInstanceTime(GlobalScene->Instance),
                                               GStateInstanceLastDeltaT(GlobalScene->Instance),
                                               StateName))
                Machine->ForceChangeToState(GStateInstanceTime(GlobalScene->Instance),
                                            StateName);
        }
        else if (EdgeName)
        {
            Machine->StartTransitionByName(GStateInstanceTime(GlobalScene->Instance), EdgeName);
        }

    }
    else if (Key == '`')
    {
        // Find the parameter node
        node* Parameter = Machine->FindChildByName("LocoParams");
        parameters* LocoParams = GSTATE_DYNCAST(Parameter, parameters);
        assert(LocoParams);

        // Normally you might look this up by name, but I know that Output 1 is the sprint
        // override...
        if (LocoParams->SampleScalarOutput(1, GStateInstanceTime(GlobalScene->Instance), NULL) != 0)
            LocoParams->SetParameter(1, 0.0f);
        else
            LocoParams->SetParameter(1, 1.0f);
    }
    else if (isdigit(Key))
    {
        node* Parameter = Machine->FindChildByName("LocoParams");
        parameters* LocoParams = GSTATE_DYNCAST(Parameter, parameters);
        assert(LocoParams);

        LocoParams->SetParameter(0, (Key - '0') * 0.1f);
    }
}

/* DEFTUTORIAL EXPGROUP(TutorialDAG2) (TutorialDAG2_RenderModel, SamplePosesForGraph) */
void SamplePosesForGraph()
{
    assert(GlobalScene->Models.size() == 1);

    GStateAdvanceTime(GlobalScene->Instance, GlobalScene->DeltaTime);
    granny_local_pose* Result = GStateSampleAnimation(GlobalScene->Instance,
                                                      GlobalScene->PoseCache);
    assert(Result);

    granny_skeleton* Skeleton = GrannyGetSourceSkeleton(GlobalScene->Models[0]->ModelInstance);

    GrannyBuildWorldPose(Skeleton,
                         0, Skeleton->BoneCount,
                         Result, NULL, GlobalScene->SharedWorldPose);
    GrannyReleaseCachePose(GlobalScene->PoseCache, Result);

    GStateClearMat4x4Cache(GlobalScene->Instance);
    granny_matrix_4x4* WPArray = GrannyGetWorldPose4x4Array(GlobalScene->SharedWorldPose);
    for( int BoneIdx=0; BoneIdx<Skeleton->BoneCount; BoneIdx++ )
    {
        GStateCacheMat4x4(GlobalScene->Instance, Skeleton->Bones[BoneIdx].Name, &((granny_real32*)WPArray[BoneIdx])[0]);
    }
}


/* DEFTUTORIAL EXPGROUP(TutorialDAG2) (TutorialDAG2_Summary, Summary) */
/* If you've looked at the source for this tutorial, you know we've elided a lot of glue
   code, but the small amount of Blend Graph specific material that sets this example
   apart from the basic DirectX rendering tutorial has been presented here in full.  Part
   of the redesign criteria for the Blend Graph was that it should be much easier to setup
   and work with the structures; there's simply not much more that you need to know to
   accurately sample the blend structures your artists create.  Please do read $BlendGraph
   for more information, and send us any suggestions!
 */

/* DEFTUTORIALEND */

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
    char Buffer[512];
    sprintf(Buffer,
            ("%s (%s)\n"
             "  %s(%d): %s\n"),
            TypeString, OriginString, File, Line, Message);
    OutputDebugString(Buffer);
}


struct source_entry
{
    granny_file*      File;
    granny_file_info* FileInfo;
};

typedef map<string, source_entry> source_map;
source_map* LoadedSources = 0;

// Free and release
void FlushSourceMap()
{
    if (!LoadedSources)
        return;

    {for(source_map::iterator Itr = LoadedSources->begin(); Itr != LoadedSources->end(); ++Itr)
    {
        source_entry& Entry = Itr->second;
        GrannyFreeFile(Entry.File);
        Entry.File = 0;
        Entry.FileInfo = 0;
    }}

    delete LoadedSources;
    LoadedSources = 0;
}

void NoteSource(char const*       SourceName,
                granny_file*      File,
                granny_file_info* Info)
{
    if (LoadedSources == 0)
    {
        LoadedSources = new source_map;
    }

    source_entry NewEntry = { File, Info };
    (*LoadedSources)[SourceName] = NewEntry;
}

granny_file_info* LookupSource(char const* SourceName)
{
    if (LoadedSources == 0)
    {
        LoadedSources = new source_map;
    }

    source_map::iterator Itr = LoadedSources->find(SourceName);
    if (Itr != LoadedSources->end())
        return Itr->second.FileInfo;

    return 0;
}

INT WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR, INT )
{
    if (!GrannyVersionsMatch)
    {
        printf("Warning: the Granny DLL currently loaded "
               "doesn't match the .h file used during compilation\n");
        return EXIT_FAILURE;
    }

    GlobalScene = new DemoScene;

    granny_log_callback NewCallback;
    NewCallback.Function = LogCallback;
    NewCallback.UserData = NULL;
    GrannySetLogCallback(&NewCallback);

    if (InitializeD3D() == false)
        return EXIT_FAILURE;

    char buffer[512];
    GetCurrentDirectory(sizeof(buffer), buffer);

    if ((GlobalScene->SceneFile     = GrannyReadEntireFile(GlobalSceneFilename)) == NULL ||
        (GlobalScene->SceneFileInfo = GrannyGetFileInfo(GlobalScene->SceneFile)) == NULL)
    {
        return EXIT_FAILURE;
    }

    if (CreateSimpleTextures(g_pD3DDevice, GlobalScene->SceneFileInfo, GlobalScene->Textures) == false ||
        CreateSimpleModels(g_pD3DDevice, GlobalScene->SceneFileInfo, GlobalScene->Textures, GlobalScene->Models) == false ||
        CreateSharedPoses() == false  || InitCameraAndLights() == false)
    {
        return EXIT_FAILURE;
    }

    if (!LoadAndBindCharacter())
        return EXIT_FAILURE;

    granny_skeleton* Skeleton = GrannyGetSourceSkeleton(GlobalScene->Models[0]->ModelInstance);
    if( Skeleton )
    {
        /* We just happen to be caching a 4x4 for each bone here, but depending how you use it, really the cache could be any size*/
        GStateAllocMat4x4Cache(GlobalScene->Instance,Skeleton->BoneCount);
    }

    /* Enter our "game" loop.  This can be mostly ignored if you've already seen this loop in $TutorialDX9Rendering. */
    granny_system_clock StartClock;
    GrannyGetSystemSeconds(&StartClock);
    granny_system_clock LastClock = StartClock;

    GlobalRunning = true;
    while (GlobalRunning)
    {
        // Extract the current time and the frame delta
        {
            granny_system_clock CurrClock;
            GrannyGetSystemSeconds(&CurrClock);

            // Ignore clock recentering issues for this example
            float Speed = 1;
            GlobalScene->DeltaTime = Speed * GrannyGetSecondsElapsed(&LastClock, &CurrClock);
            LastClock = CurrClock;
        }

        InvalidateRect(g_hwnd, 0, false);
        MSG Message;
        while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }
    }

    GStateFreeMat4x4Cache(GlobalScene->Instance);

    ReleaseCharacter();
    FlushSourceMap();
    CleanupD3D();
    delete GlobalScene;
    GlobalScene = 0;

    return EXIT_SUCCESS;
}


