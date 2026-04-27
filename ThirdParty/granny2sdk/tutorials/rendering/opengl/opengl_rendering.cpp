// =======================================================================================
// opengl_rendering.cpp: Demonstrates rendering an animated mesh in OpenGL
//
// $Notice: $
// =======================================================================================
#include "opengl_rendering.h"
#include <string.h>
#include <vector>

using namespace std;

#define HARDCODED_DISPLAY_WIDTH  800
#define HARDCODED_DISPLAY_HEIGHT 450

#if defined(_MACOSX) || defined(linux)
#define _stricmp strcasecmp
#define BASE_DIR "../../"
#else
#define BASE_DIR ""
#endif

// Note that we're using extremely simple objects, and a global scene
// database that you'll want to completely ignore when you build your
// own app on top of Granny.  This simply keeps everything very
// straightfoward.
char const* const GlobalSceneFilename     =  BASE_DIR "media/pixo_dance.gr2";

char const* const GlobalRigidShaderFile    = BASE_DIR "media/shaders/Rigid.glsl";
char const* const GlobalSkinnedShaderFile  = BASE_DIR "media/shaders/Skinned.glsl";
char const* const GlobalFragmentShaderFile = BASE_DIR "media/shaders/Fragment.glsl";

bool CreateDemoTextures();
bool CreateDemoModels();
bool CreateSharedPoses();
bool InitCameraAndLights();
void RenderModel(DemoModel*);
void Update(granny_real32 const CurrentTime,
            granny_real32 const DeltaTime);


// ---------------------------------------------------------
// Scene instance objects
DemoScene GlobalScene;

GLhandleARB RigidVertexProgram;
GLhandleARB SkinnedVertexProgram;
GLhandleARB FragmentProgram;

GLhandleARB ProgramSkinned;
GLhandleARB ProgramRigid;

GLint BoneMatricesLocSkinned = -1;
GLint MyNormalLocSkinned = -1;
GLint BlendWeightsLocSkinned = -1;
GLint BlendIndicesLocSkinned = -1;

int CurrentWidth  = HARDCODED_DISPLAY_WIDTH;
int CurrentHeight = HARDCODED_DISPLAY_HEIGHT;
granny_system_clock GlobalStartClock;


void ChangeSize(int w, int h)
{
    // Prevent a divide by zero, when window is too short
    if(h == 0) h = 1;

    CurrentWidth  = w;
    CurrentHeight = h;
    glViewport(0, 0, w, h);
}

void ProcessNormalKeys(unsigned char key,
                       int /*x*/, int /*y*/)
{
    if (key == 27 || key == 'Q' || key == 'q')
        exit(0);
}

/* DEFTUTORIAL EXPGROUP(TutorialOpenGLRendering) (TutorialOpenGLRendering_Introduction, Introduction) */
/* Who's your Granny?  The rest of the world is treating you like a second-class citizen
   just because you're not using DirectX, but here, you get your own sample application.
   Now, granted, this is going to look a lot like the DirectX sample.  Well, OK, it's
   going to look <i>exactly</i> like the DirectX sample.  I assure you, that's a clear cut
   case of parallel evolution.  It's not that we wrote the OpenGL sample last.  Nope.  Not
   that at all.
   $-

   Shouts out to the authors of the GLUT and GLEW frameworks, for making working with the
   extensions and window setup quite pleasant.  In order to run this demo, you'll need a
   up-to-date card and driver set that supports vertex and fragment shaders.  We'll be
   making use of both the ARB_vertex_shader and ARB_fragment_shader extension.  To keep
   things simple, I've made the assumption that your setup has space in its constant
   registers for at least 32 bone matrices, as well as the standard matrices and lights.
   $-

   This is going to be a full sample application that loads, preps, and renders a
   complicated scene with multiple models, both skinned and rigid.  We'll handle sending
   textures to the rendering API, control the clock and position the camera using Granny's
   built-in camera utilities.  Because this is a large sample app, it's going to be hard
   to structure the autoextracted tutorial page to read as a cohesive whole from start to
   finish.  Please, work from the source, and use the function index below as a guide when
   reading this page.
   $-

   In rough outline, the app will load our scene (pixo_dance.gr2), create instanced
   objects that will hold the information about each $model_instance, it's bound $(mesh)es
   and $(material)s, construct the texture objects in OpenGL that will hold our image
   buffers, and play the single animation on the scene, which will control all of the demo
   objects.  Note that because this demo is playing the equivalant of an in-game cutscene,
   there is no animation blending going on in this tutorial.  See $TutorialBlending for
   examples of that functionality.
   $-

   Prerequisites to this demo:
   $* $TutorialBasicLoading
   $* $TutorialCreateControl
   $-

   New functions and structures discussed in this tutorial:
   $* Many
   $-

   Function index:
   $* $TutorialOpenGLRendering_main
   $* $TutorialOpenGLRendering_CreateDemoTextures
   $* $TutorialOpenGLRendering_CreateBoundMesh
   $* $TutorialOpenGLRendering_CreateDemoModels
   $* $TutorialOpenGLRendering_CreateSharedPoses
   $* $TutorialOpenGLRendering_InitCameraAndLights
   $* $TutorialOpenGLRendering_Update
   $* $TutorialOpenGLRendering_Render
   $* $TutorialOpenGLRendering_RenderModel
   $-
*/

/* DEFTUTORIAL EXPGROUP(TutorialOpenGLRendering) (TutorialOpenGLRendering_main, main) */
int main(int argc, char** argv)
{
    // Check the versions
    if(!GrannyVersionsMatch)
    {
        printf("Warning: the Granny DLL currently loaded "
               "doesn't match the .h file used during compilation\n");
        return EXIT_FAILURE;
    }

    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DEPTH | GLUT_STENCIL | GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowPosition(100,100);
    glutInitWindowSize(HARDCODED_DISPLAY_WIDTH, HARDCODED_DISPLAY_HEIGHT);
    glutCreateWindow("OpenGL GrannySample");
    glutDisplayFunc(Render);
    glutIdleFunc(Render);
    glutReshapeFunc(ChangeSize);
    glutKeyboardFunc(ProcessNormalKeys);

    // Initialize the GL Extension Wrangler
    glewInit();

    /* Load the scene file, and bind the meshes, textures, etc. */
    {
        GlobalScene.SceneFile = GrannyReadEntireFile(GlobalSceneFilename);
        if (GlobalScene.SceneFile == NULL)
            return EXIT_FAILURE;

        GlobalScene.SceneFileInfo = GrannyGetFileInfo(GlobalScene.SceneFile);
        if (GlobalScene.SceneFileInfo == NULL)
            return EXIT_FAILURE;
    }

    /* Create our demo objects.  Note that the order is important here, CreateDemoModels
       will be looking up textures as it iterates, and CreateSharedPoses will scan the
       created models. */
    if (CreateDemoTextures() == false ||
        CreateDemoModels()   == false ||
        CreateSharedPoses()  == false ||
        InitCameraAndLights() == false)
    {
        return EXIT_FAILURE;
    }

    if (!(GLEW_ARB_vertex_shader && GLEW_ARB_fragment_shader) ||
        !(GLEW_ARB_vertex_buffer_object) ||
        !InitializeOpenGL())
    {
        printf("Error: unable to initialize OpenGL device.  "
               "OpenGL renderer with shader capabilities required.\n");
        return EXIT_FAILURE;
    }


    /* Mark the start time so we can compute update intervals, and enter the render
       loop */
    GrannyGetSystemSeconds(&GlobalStartClock);
    glutMainLoop();
    CleanupOpenGL();

    return EXIT_SUCCESS;
}


/* DEFTUTORIAL EXPGROUP(TutorialOpenGLRendering) (TutorialOpenGLRendering_CreateDemoTextures, CreateDemoTextures) */
/* This function creates the texture objects for every texture listed in the Granny file.
   We're going to make some assumptions about the textures contained in the demo file,
   namely that the FromFileName field of the $texture uniquely identifies the texture, and
   that there is only one $texture_image per $texture.  Neither of these are hard
   requirements in Granny, cube maps for instance have 6 images per $texture, but making
   them removes a lot of non-essential details from this function.

   We'll also be ignoring the initial format of the $(texture_image)s.  We'll demonstrate
   how to use S3TC textures in another sample.  ($TutorialS3TC_OpenGL) We use
   $CopyTextureImage to convert the texture from the stored format to a standard RGB8 or
   RGBA8 format.
*/
bool CreateDemoTextures()
{
    assert(GlobalScene.SceneFileInfo);

    /* Iterate across all the textures */
    granny_file_info* Info = GlobalScene.SceneFileInfo;
    {for(granny_int32x TexIdx = 0; TexIdx < Info->TextureCount; ++TexIdx)
    {
        granny_texture* GrannyTexture = Info->Textures[TexIdx];
        assert(GrannyTexture);

        if (GrannyTexture->TextureType == GrannyColorMapTextureType &&
            GrannyTexture->ImageCount == 1)
        {
            /* We select the appropriate 8-bit/channel format based on whether or not the
               image has an alpha channel. */
            GLint TexFormat;
            int BytesPerPixel;
            granny_pixel_layout const* GrannyTexFormat;
            if (GrannyTextureHasAlpha(GrannyTexture))
            {
                TexFormat = GL_RGBA;
                BytesPerPixel = 4;
                GrannyTexFormat = GrannyRGBA8888PixelFormat;
            }
            else
            {
                TexFormat = GL_RGB;
                BytesPerPixel = 3;
                GrannyTexFormat = GrannyRGB888PixelFormat;
            }

            /* Create and lock the texture surface, and use $CopyTextureImage to move the
               pixels.  This function will handle format conversion, if necessary,
               including adding or removing alpha components, decompressing from Bink or
               S3TC formats, etc. */
            granny_uint8* Pixels = new granny_uint8[GrannyTexture->Width  *
                                                    GrannyTexture->Height *
                                                    BytesPerPixel];
            GrannyCopyTextureImage(GrannyTexture, 0, 0,
                                   GrannyTexFormat,
                                   GrannyTexture->Width,
                                   GrannyTexture->Height,
                                   GrannyTexture->Width * BytesPerPixel,
                                   Pixels);

            DemoTexture* NewTex = new DemoTexture;
            glGenTextures(1, &NewTex->TextureName);
            glBindTexture(GL_TEXTURE_2D, NewTex->TextureName);

            // Note that if the Granny texture already contains mipmaps, it's much more
            // efficient to copy them into the texture object with glTexImage2D rather
            // than using GLU to rebuild them, of course.
            gluBuild2DMipmaps(GL_TEXTURE_2D, BytesPerPixel,
                              GrannyTexture->Width,
                              GrannyTexture->Height,
                              TexFormat,
                              GL_UNSIGNED_BYTE,
                              Pixels);
            delete [] Pixels;

            /* Add the texture to our demo list, after copying the file name. */
            NewTex->Name = new char[strlen(GrannyTexture->FromFileName) + 1];
            strcpy(NewTex->Name, GrannyTexture->FromFileName);
            GlobalScene.Textures.push_back(NewTex);
        }
    }}

    /* Now that we've created all the surfaces required to texture our scene, we can get
       rid of the copy of the pixels still stored in the $file_info.  We'll free the file
       section that contains pixel data.  By default, the exporter creates a $file section
       exclusively for the pixel data ($StandardTextureSection), and stores the $texture
       metadata elsewhere.  So we can free the bulk data, but still make use of the Name,
       Height, Width, etc.
    */
    if (GlobalScene.SceneFile->SectionCount >= GrannyStandardTextureSection)
    {
        GrannyFreeFileSection(GlobalScene.SceneFile,
                              GrannyStandardTextureSection);
    }

    return true;
}

/* DEFTUTORIAL EXPGROUP(TutorialOpenGLRendering) (TutorialOpenGLRendering_CreateBoundMesh, CreateBoundMesh) */
/* This function creates a mesh binding, and the vertex and index buffers used to render
   the model.  We make a distinction in this demo between meshes that are bound only to
   one bone (i.e, rigid meshes) and those bound to multiple bones (skinned meshes).  We
   select a simple vertex format for each case, and convert the Granny vertices using
   $CopyMeshVertices.
*/
DemoMesh* CreateBoundMesh(granny_mesh* GrannyMesh,
                          granny_model_instance* ModelInstance)
{
    assert(GrannyMesh);
    assert(ModelInstance);

    granny_model* SourceModel = GrannyGetSourceModel(ModelInstance);

    /* Create the binding.  $NewMeshBinding takes two $skeleton parameters to allow you to
       attach the mesh to a $skeleton other than the one it was modeled on.  Check
       $BindingAnimationsToMeshes_BindingMeshes for more details.  In this case, we're
       binding the mesh to the original $skeleton, so we just pass it twice. */
    DemoMesh* NewMesh = new DemoMesh;
    NewMesh->Mesh = GrannyMesh;
    NewMesh->MeshBinding = GrannyNewMeshBinding(GrannyMesh,
                                                SourceModel->Skeleton,
                                                SourceModel->Skeleton);

    if (GrannyMeshIsRigid(NewMesh->Mesh))
    {
        GLint bufferSize = GrannyGetMeshVertexCount(NewMesh->Mesh) * sizeof(granny_pnt332_vertex);
        char* VertexBuffer = new char[bufferSize];

        GrannyCopyMeshVertices(NewMesh->Mesh,
                               GrannyPNT332VertexType,
                               VertexBuffer);

        glGenBuffersARB(1, &NewMesh->VertexBufferObject);
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, NewMesh->VertexBufferObject);
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, bufferSize, VertexBuffer, GL_STATIC_DRAW_ARB);

        delete [] VertexBuffer;
    }
    else
    {
        GLint bufferSize = GrannyGetMeshVertexCount(NewMesh->Mesh) * sizeof(granny_pwnt3432_vertex);
        char* VertexBuffer = new char[bufferSize];

        GrannyCopyMeshVertices(NewMesh->Mesh,
                               GrannyPWNT3432VertexType,
                               VertexBuffer);

        glGenBuffersARB(1, &NewMesh->VertexBufferObject);
        glBindBufferARB(GL_ARRAY_BUFFER, NewMesh->VertexBufferObject);
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, bufferSize, VertexBuffer, GL_STATIC_DRAW_ARB);

        delete [] VertexBuffer;
    }

    // Create the index buffer
    {
        GLint bufferSize = GrannyGetMeshIndexCount(NewMesh->Mesh) * sizeof(granny_int32);
        char* IndexBuffer = new char[bufferSize];

        GrannyCopyMeshIndices(NewMesh->Mesh, 4, IndexBuffer);

        glGenBuffersARB(1, &NewMesh->IndexBufferObject);
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, NewMesh->IndexBufferObject);
        glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, bufferSize, IndexBuffer, GL_STATIC_DRAW_ARB);

        delete [] IndexBuffer;
    }

    /* Lookup the texture bindings in the list we created in CreateDemoTextures.  Note
       that this process can insert NULL pointers in to the MaterialBindings array, which
       we will handle in Render(). */
    {for(granny_int32x MatIdx = 0; MatIdx < GrannyMesh->MaterialBindingCount; ++MatIdx)
    {
        granny_material* Material = GrannyMesh->MaterialBindings[MatIdx].Material;
        DemoTexture* Found = NULL;
        if (Material->MapCount >= 1)
        {
            {for(granny_int32x MapIdx = 0; MapIdx < Material->MapCount; ++MapIdx)
            {
                granny_material_map& Map = Material->Maps[MapIdx];

                if (_stricmp(Map.Usage, "color") == 0 && Map.Material->Texture)
                {
                    {for(granny_uint32x i = 0; i < GlobalScene.Textures.size(); ++i)
                    {
                        if (_stricmp(Map.Material->Texture->FromFileName, GlobalScene.Textures[i]->Name) == 0)
                        {
                            Found = GlobalScene.Textures[i];
                            break;
                        }
                    }}
                }
            }}
        }

        NewMesh->MaterialBindings.push_back(Found);
    }}

    return NewMesh;
}

/* DEFTUTORIAL EXPGROUP(TutorialOpenGLRendering) (TutorialOpenGLRendering_CreateDemoModels, CreateDemoModels) */
/* This function creates the $model_instance objects that we'll bind animations to and
   render.  We're only going to concern ourselves with models that are bound to renderable
   meshes in this function.  Real games may want to look a little closer, using the data
   stored on the $(model)s to create lights, cameras, etc.
*/
bool CreateDemoModels()
{
    assert(GlobalScene.SceneFileInfo);

    /* Iterate across all the models */
    granny_file_info* Info = GlobalScene.SceneFileInfo;
    {for(granny_int32x ModelIdx = 0; ModelIdx < Info->ModelCount; ++ModelIdx)
    {
        granny_model* GrannyModel = Info->Models[ModelIdx];
        assert(GrannyModel);

        if (GrannyModel->MeshBindingCount > 0)
        {
            DemoModel* NewModel = new DemoModel;
            GlobalScene.Models.push_back(NewModel);

            NewModel->ModelInstance = GrannyInstantiateModel(GrannyModel);
            GrannyBuildCompositeTransform4x4(&GrannyModel->InitialPlacement,
                                             NewModel->InitialMatrix);

            /* Create the meshes that are bound to this model. */
            {for(granny_int32x MeshIdx = 0;
                 MeshIdx < GrannyModel->MeshBindingCount;
                 ++MeshIdx)
            {
                granny_mesh* GrannyMesh = GrannyModel->MeshBindings[MeshIdx].Mesh;
                assert(GrannyMesh);

                DemoMesh* NewMesh = CreateBoundMesh(GrannyMesh, NewModel->ModelInstance);
                assert(NewMesh);
                NewModel->BoundMeshes.push_back(NewMesh);
            }}

            /* We're going to use a "fire-and-forget" animation technique here, since
               we're simply going to loop a single animation until the application
               finishes.  Once we create the control, we tell Granny that it's allowed to
               free it when we free the $model_instance, and set the loop count to 0 to
               indicate that we want it to loop forever. */
            {for(granny_int32x AnimIdx = 0; AnimIdx < Info->AnimationCount; ++AnimIdx)
            {
                granny_animation* Animation = Info->Animations[AnimIdx];

                // Simple control, starting at t = 0.0f
                granny_control *Control =
                    GrannyPlayControlledAnimation(0.0f, Animation,
                                                  NewModel->ModelInstance);
                if (Control)
                {
                    // Loop forever
                    GrannySetControlLoopCount(Control, 0);

                    // Allow Granny to free the control when we delete the instance
                    GrannyFreeControlOnceUnused(Control);

                    // Only bind one animation to the model
                    break;
                }
            }}
        }
    }}

    return true;
}

/* DEFTUTORIAL EXPGROUP(TutorialOpenGLRendering) (TutorialOpenGLRendering_CreateSharedPoses, CreateSharedPoses) */
/* This is a small utility function that scans our $(model_instance)s, looking for the
   maximum bone count.  If we create a $local_pose and a $world_pose that have at least
   this many bones, we can share it among all of the instances, since we're sampling
   serially. */
bool CreateSharedPoses()
{
    GlobalScene.MaxBoneCount = -1;
    {for(granny_uint32x i = 0; i < GlobalScene.Models.size(); ++i)
    {
        granny_skeleton* Skeleton =
            GrannyGetSourceSkeleton(GlobalScene.Models[i]->ModelInstance);

        if (Skeleton->BoneCount > GlobalScene.MaxBoneCount)
            GlobalScene.MaxBoneCount = Skeleton->BoneCount;
    }}

    if (GlobalScene.MaxBoneCount != -1)
    {
        GlobalScene.SharedLocalPose = GrannyNewLocalPose(GlobalScene.MaxBoneCount);

        // More on this later (in the Render() description).  GrannyNewWorldPose would work
        // equally well here.
        GlobalScene.SharedWorldPose =
            GrannyNewWorldPoseNoComposite(GlobalScene.MaxBoneCount);

        return true;
    }
    else
    {
        // Very odd.  An error loading the file most likely.
        return false;
    }
}


/* DEFTUTORIAL EXPGROUP(TutorialOpenGLRendering) (TutorialOpenGLRendering_InitCameraAndLights, InitCameraAndLights) */
/* Our test scene contains a model "persp" that contains the camera information that we'll
   extract, and set into our $camera utility object.  This demo uses a simple directional
   light that is hardcoded here, but you can easily extract light info from exported
   models and use that instead.

   We define a structure and $data_type_definition array for the camera fields we want to
   extract.  In the case of the pixo_dance.gr2 source file, the original source art is a
   Maya file, so the parameter names will be different if you're starting with a Max or
   XSI file. */
struct CameraFields
{
    // Ignore everything but the clip planes, position will be extracted from the
    // skeleton, and the FOV will be manually set
    granny_real32 nearClipPlane;
    granny_real32 farClipPlane;
};
granny_data_type_definition CameraFieldsType[] =
{
    { GrannyReal32Member, "nearClipPlane" },
    { GrannyReal32Member, "farClipPlane"  },
    { GrannyEndMember }
};


bool InitCameraAndLights()
{
    assert(GlobalScene.SceneFileInfo);
    granny_file_info* Info = GlobalScene.SceneFileInfo;

    bool FoundCamera = false;
    {for(granny_int32x ModelIdx = 0; ModelIdx < Info->ModelCount; ++ModelIdx)
    {
        if (_stricmp("persp", Info->Models[ModelIdx]->Name) == 0)
        {
            granny_skeleton* CameraSkel = Info->Models[ModelIdx]->Skeleton;

            granny_variant CameraVariant;
            if (CameraSkel->BoneCount == 1 &&
                GrannyFindMatchingMember(CameraSkel->Bones[0].ExtendedData.Type,
                                         CameraSkel->Bones[0].ExtendedData.Object,
                                         "CameraInfo",
                                         &CameraVariant))
            {
                // This should be a reference to the data
                assert(CameraVariant.Type[0].Type == GrannyReferenceMember);

                granny_data_type_definition const *CameraStoredType = CameraVariant.Type[0].ReferenceType;
                void const *CameraObject = *((void const **)(CameraVariant.Object));

                CameraFields CameraFields;
                GrannyConvertSingleObject(CameraStoredType, CameraObject,
                                          CameraFieldsType, &CameraFields, 0);

                // Ok, now we can setup the camera...
                GrannyInitializeDefaultCamera(&GlobalScene.DemoCamera);
                GlobalScene.DemoCamera.NearClipPlane = CameraFields.nearClipPlane;
                GlobalScene.DemoCamera.FarClipPlane  = CameraFields.farClipPlane;
                GlobalScene.DemoCamera.FOVY = 0.2f * 3.14f;


                memcpy(GlobalScene.DemoCamera.Orientation,
                       Info->Models[ModelIdx]->InitialPlacement.Orientation,
                       sizeof(GlobalScene.DemoCamera.Orientation));
                memcpy(GlobalScene.DemoCamera.Position,
                       Info->Models[ModelIdx]->InitialPlacement.Position,
                       sizeof(GlobalScene.DemoCamera.Position));

                FoundCamera = true;
                break;
            }
        }
    }}

    /* Setup our hardcoded light.  This points it in the same general direction as the camera, but offset slightly. */
    GlobalScene.DirFromLight[0]  = -0.8660f;
    GlobalScene.DirFromLight[1]  = 0.5f;
    GlobalScene.DirFromLight[2]  = 0;
    GlobalScene.LightColor[0]   = 0.8f;
    GlobalScene.LightColor[1]   = 0.8f;
    GlobalScene.LightColor[2]   = 0.8f;
    GlobalScene.LightColor[3]   = 0.8f;
    GlobalScene.AmbientColor[0] = 0.2f;
    GlobalScene.AmbientColor[1] = 0.2f;
    GlobalScene.AmbientColor[2] = 0.2f;
    GlobalScene.AmbientColor[3] = 0.2f;

    return FoundCamera;
}

/* DEFTUTORIAL EXPGROUP(TutorialOpenGLRendering) (TutorialOpenGLRendering_Update, Update) */
void Update(granny_real32 const CurrentTime)
{
    /* All that's necessary here is to set the current time for each of the models.  We
       don't make use of DeltaTime in this function, but if we were tracking extracted
       root motion, we'd be using it with $UpdateModelMatrix.  You can call
       $SampleModelAnimations (or one of that family of functions) at this point if you
       are sampling at update time, but we delay building the $local_pose and $world_pose
       until rendering in this app. */
    for (size_t Idx = 0; Idx < GlobalScene.Models.size(); Idx++)
    {
        // Set the model clock
        DemoModel* Model = GlobalScene.Models[Idx];
        GrannySetModelClock(Model->ModelInstance, CurrentTime);
    }
}


/* DEFTUTORIAL EXPGROUP(TutorialOpenGLRendering) (TutorialOpenGLRendering_Render, Render) */
/* This routine just sets up some global camera matrices, clears the framebuffer, and then
   forwards the real work to $TutorialOpenGLRendering_RenderModel. */
void Render()
{
    // Extract the current time and the frame delta
    granny_real32 CurrentTime;
    {
        granny_system_clock CurrClock;
        GrannyGetSystemSeconds(&CurrClock);

        // Ignore clock recentering issues for this example
        CurrentTime = GrannyGetSecondsElapsed(&GlobalStartClock, &CurrClock);
    }
    Update(CurrentTime);

    /* Note that I set the camera aspect ratios every frame, because the width and height
       of the window may have changed which affects the aspect ratio correction.  However,
       if you're a full screen game and you know when you're changing screen modes and
       such, then you'd only have to call GrannySetCameraAspectRatios() when you actually
       change modes. */
    GrannySetCameraAspectRatios(&GlobalScene.DemoCamera,
                                GrannyGetMostLikelyPhysicalAspectRatio(CurrentWidth,
                                                                       CurrentHeight),
                                (float)CurrentWidth, (float)CurrentHeight,
                                (float)CurrentWidth, (float)CurrentHeight);
    GrannyBuildCameraMatrices(&GlobalScene.DemoCamera);

    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // This is something of an abuse of the modelview/projection stack, but it makes the
    // lighting slightly easier in the shader.  You probably do NOT want to setup your
    // camera matrices this way.
    {
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf((GLfloat*)GlobalScene.DemoCamera.Projection4x4);
        glMultMatrixf((GLfloat*)GlobalScene.DemoCamera.View4x4);
    }

    /* Bind our light location.  This is a fairly braindead way to do it, but it doesn't
       clutter up the render calls.
    */
    {
        glUseProgramObjectARB(ProgramRigid);
        glUniform3fvARB(glGetUniformLocationARB(ProgramRigid, "DirFromLight"), 1,
                        GlobalScene.DirFromLight);
        glUniform4fvARB(glGetUniformLocationARB(ProgramRigid, "LightColor"), 1,
                        GlobalScene.LightColor);
        glUniform4fvARB(glGetUniformLocationARB(ProgramRigid, "AmbientLightColor"), 1,
                        GlobalScene.AmbientColor);

        glUseProgramObjectARB(ProgramSkinned);
        glUniform3fvARB(glGetUniformLocationARB(ProgramSkinned, "DirFromLight"), 1,
                        GlobalScene.DirFromLight);
        glUniform4fvARB(glGetUniformLocationARB(ProgramSkinned, "LightColor"), 1,
                        GlobalScene.LightColor);
        glUniform4fvARB(glGetUniformLocationARB(ProgramSkinned, "AmbientLightColor"), 1,
                        GlobalScene.AmbientColor);
    }

    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Loop over the models, and render their meshes
    for (size_t Idx = 0; Idx < GlobalScene.Models.size(); Idx++)
    {
        RenderModel(GlobalScene.Models[Idx]);
    }

    // Present the backbuffer contents to the display
    glutSwapBuffers();
}


/* DEFTUTORIAL EXPGROUP(TutorialOpenGLRendering) (TutorialOpenGLRendering_RenderModel, RenderModel) */
#define Offset(type, member) (&(((type*)NULL)->member))

void RenderModel(DemoModel* Model)
{
    /* We sample the model's animations "just in time", delaying until we know that we're
       going to render the $model_instance.  In the case of this demo app, we render all
       of the models, all of the time, but if you cull at the object level, you can avoid
       doing a lot of work by simply never calling $SampleModelAnimations (or similar
       functions) on the culled $model_instance.  Here, since we don't need the
       $local_pose, we use $SampleModelAnimationsAccelerated to build the $world_pose in
       whatever Granny decides is the most efficient manner.

       We've enabled "move to origin" for all these matrices (which is a very good idea,
       see: $RootMotion), so we need to send in a matrix representing the initial
       placement to ensure that the models show up in the expected location.  We computed
       that when we created the DemoModel object.  */
    GrannySampleModelAnimationsAccelerated(
        Model->ModelInstance,
        GrannyGetSourceSkeleton(Model->ModelInstance)->BoneCount,
        Model->InitialMatrix,
        GlobalScene.SharedLocalPose,
        GlobalScene.SharedWorldPose);

    /* Iterate across all meshes */
    {for (size_t MeshIndex = 0; MeshIndex < Model->BoundMeshes.size(); ++MeshIndex)
    {
        DemoMesh* Mesh = Model->BoundMeshes[MeshIndex];

        /* The ToBoneIndices holds the mapping from the contiguously index mesh bones to
           original source indices in the bound skeleton.  This is necessary for both
           rigid and skinned meshes.  There is an analogous call to get the mapping to the
           source skeleton, $GetMeshBindingFromBoneIndices, but you'll almost always want
           to use $GetMeshBindingToBoneIndices.  In our case, the two mappings are
           identical, but this is not always true. */
        int const *ToBoneIndices =
            GrannyGetMeshBindingToBoneIndices(Mesh->MeshBinding);

        GLhandleARB UsingProgram;
        if (GrannyMeshIsRigid(Mesh->Mesh))
        {
            /* It's a rigid mesh, so load the appropriate program.  Note that this is
               pretty slow, normally you'd order your meshes to minimize the number of
               state switches. */
            UsingProgram = ProgramRigid;
            glUseProgramObjectARB(ProgramRigid);

            /* Now I look up the transform for this mesh, and load it onto the modelview
               stack. */
            granny_matrix_4x4* CompositeMatrix = (granny_matrix_4x4*)GrannyAllocateCompositeBuffer(1);
            GrannyBuildIndexedCompositeBuffer(
                GrannyGetMeshBindingToSkeleton(Mesh->MeshBinding),
                GlobalScene.SharedWorldPose,
                ToBoneIndices, 1,
                CompositeMatrix);

            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glMultMatrixf((GLfloat*)CompositeMatrix);

            // Setup our vertex arrays.
            glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mesh->VertexBufferObject);
            glEnableClientState(GL_VERTEX_ARRAY);
            glEnableClientState(GL_NORMAL_ARRAY);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glVertexPointer(3, GL_FLOAT,
                            sizeof(granny_pnt332_vertex),
                            Offset(granny_pnt332_vertex, Position[0]));
            glNormalPointer(GL_FLOAT,
                            sizeof(granny_pnt332_vertex),
                            Offset(granny_pnt332_vertex, Normal[0]));
            glTexCoordPointer(2, GL_FLOAT,
                              sizeof(granny_pnt332_vertex),
                              Offset(granny_pnt332_vertex, UV[0]));

            GrannyFreeCompositeBuffer(CompositeMatrix);
        }
        else
        {
            /* It's a skinned mesh, activate the appropriate technique. */
            int const NumMeshBones = GrannyGetMeshBindingBoneCount(Mesh->MeshBinding);
            assert(NumMeshBones <= 32);
            UsingProgram = ProgramSkinned;
            glUseProgramObjectARB(ProgramSkinned);

            /* Load the matrices into the constant registers.  This is about the simplest
               way to go about this.  Remember that I said we'd be talking about the call
               to $NewWorldPoseNoComposite?  Here's the payoff.  When you call that
               routine, it creates a $world_pose without a Composite array attached, which
               allows Granny to skip those matrices when $BuildWorldPose is called.  Since
               we're just going to copy a few of the matrices into the constant buffer, we
               delay until this point, at which we know exactly which matrices we need,
               and exactly where they need to go.  $BuildIndexedCompositeBuffer is
               designed to do this for you.  (If you need the matrices transposed, use
               $BuildIndexedCompositeBufferTransposed.)
            */
            granny_matrix_4x4* CompositeBuffer = (granny_matrix_4x4*)GrannyAllocateCompositeBuffer(NumMeshBones);

            GrannyBuildIndexedCompositeBuffer(
                GrannyGetMeshBindingToSkeleton(Mesh->MeshBinding),
                GlobalScene.SharedWorldPose,
                ToBoneIndices, NumMeshBones,
                CompositeBuffer);

            glUniformMatrix4fvARB(BoneMatricesLocSkinned,
                                  NumMeshBones,
                                  false,
                                  (GLfloat*)CompositeBuffer);

            // Setup our vertex arrays.  (See the documentation for tyhe shader extensions
            // for information on how to bind shader parameters to vertex arrays.)  In a
            // real program, remember, you'd be using Vertex Buffer Objects, so this would
            // be slightly different.
            glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mesh->VertexBufferObject);
            glEnableClientState(GL_VERTEX_ARRAY);
            glEnableClientState(GL_NORMAL_ARRAY);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glEnableVertexAttribArrayARB(MyNormalLocSkinned);
            glEnableVertexAttribArrayARB(BlendWeightsLocSkinned);
            glEnableVertexAttribArrayARB(BlendIndicesLocSkinned);

            glVertexPointer(3, GL_FLOAT,
                            sizeof(granny_pwnt3432_vertex),
                            Offset(granny_pwnt3432_vertex, Position[0]));
            glTexCoordPointer(2, GL_FLOAT,
                              sizeof(granny_pwnt3432_vertex),
                              Offset(granny_pwnt3432_vertex, UV[0]));
            glVertexAttribPointerARB(MyNormalLocSkinned,
                                     3, GL_FLOAT,
                                     false,
                                     sizeof(granny_pwnt3432_vertex),
                                     Offset(granny_pwnt3432_vertex, Normal[0]));
            glVertexAttribPointerARB(BlendWeightsLocSkinned,
                                     4, GL_UNSIGNED_BYTE,
                                     true, // normalized
                                     sizeof(granny_pwnt3432_vertex),
                                     Offset(granny_pwnt3432_vertex, BoneWeights[0]));
            glVertexAttribPointerARB(BlendIndicesLocSkinned,
                                     4, GL_UNSIGNED_BYTE,
                                     false, // NOT normalized
                                     sizeof(granny_pwnt3432_vertex),
                                     Offset(granny_pwnt3432_vertex, BoneIndices[0]));

            GrannyFreeCompositeBuffer(CompositeBuffer);
        }


        /* Loop through each of the $mesh's material groups, and render them. */
        int GroupCount = GrannyGetMeshTriangleGroupCount(Mesh->Mesh);
        granny_tri_material_group *Group =
            GrannyGetMeshTriangleGroups(Mesh->Mesh);

        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mesh->IndexBufferObject);
        while(GroupCount--)
        {
            GLuint texName = 0;
            if (Group->MaterialIndex < int(Mesh->MaterialBindings.size()) &&
                Mesh->MaterialBindings[Group->MaterialIndex] != NULL)
            {
                DemoTexture* Texture = Mesh->MaterialBindings[Group->MaterialIndex];
                texName = Texture->TextureName;
            }

            glBindTexture(GL_TEXTURE_2D, texName);
            glUniform1iARB(glGetUniformLocationARB(UsingProgram, "tex"), 0);

            glDrawElements(GL_TRIANGLES,
                           Group->TriCount*3,
                           GL_UNSIGNED_INT,
                           ((char*)0) + (Group->TriFirst * 12));
            ++Group;
        }

        /* Clean up our vertex array states and the modelview stack, if necessary. */
        if (GrannyMeshIsRigid(Mesh->Mesh))
        {
            glPopMatrix();
        }
        else
        {
            glDisableVertexAttribArrayARB(MyNormalLocSkinned);
            glDisableVertexAttribArrayARB(BlendWeightsLocSkinned);
            glDisableVertexAttribArrayARB(BlendIndicesLocSkinned);
        }
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }}
}


/* DEFTUTORIALEND */

#include <string>
#include <stdio.h>

bool ReadFile(const char*  Filename,
              char*& Contents)
{
    FILE* f = fopen(Filename, "rb");
    if (!f)
        return false;

    fseek(f, 0, SEEK_END);
    int FileSize = ftell(f);

    Contents = new char[FileSize+1];
    fseek(f, 0, SEEK_SET);
    fread(Contents, FileSize, 1, f);
    Contents[FileSize] = 0;

    return true;
}

void printInfoLog(GLhandleARB obj)
{
    GLint infologLength = 0;
    GLint charsWritten  = 0;
    glGetObjectParameterivARB(obj, GL_OBJECT_INFO_LOG_LENGTH_ARB,
                              &infologLength);
    if (infologLength > 0)
    {
        char* infoLog = new char[infologLength];
        glGetInfoLogARB(obj, infologLength, &charsWritten, infoLog);
        printf("%s\n",infoLog);
        delete [] infoLog;
    }
}

bool InitializeOpenGL()
{
    char* RigidContents = NULL;
    char* SkinnedContents = NULL;
    char* FragmentContents = NULL;
    if (!ReadFile(GlobalRigidShaderFile, RigidContents) ||
        !ReadFile(GlobalSkinnedShaderFile, SkinnedContents) ||
        !ReadFile(GlobalFragmentShaderFile, FragmentContents))
    {
        printf("Error: couldn't load the shaders\n");
        return false;
    }

    RigidVertexProgram = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
    SkinnedVertexProgram = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
    FragmentProgram  = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
    glShaderSourceARB(RigidVertexProgram, 1, (const GLcharARB**)&RigidContents, NULL);
    glShaderSourceARB(SkinnedVertexProgram, 1, (const GLcharARB**)&SkinnedContents, NULL);
    glShaderSourceARB(FragmentProgram, 1, (const GLcharARB**)&FragmentContents, NULL);
    delete [] RigidContents;
    delete [] SkinnedContents;
    delete [] FragmentContents;

    glCompileShaderARB(RigidVertexProgram);
    glCompileShaderARB(SkinnedVertexProgram);
    glCompileShaderARB(FragmentProgram);
    printInfoLog(RigidVertexProgram);
    printInfoLog(SkinnedVertexProgram);
    printInfoLog(FragmentProgram);

    ProgramSkinned = glCreateProgramObjectARB();
    glAttachObjectARB(ProgramSkinned,SkinnedVertexProgram);
    glAttachObjectARB(ProgramSkinned,FragmentProgram);
    glLinkProgramARB(ProgramSkinned);
    printf("Skinned Shader:\n");
    printInfoLog(ProgramSkinned);
    printf("\n\n");

    ProgramRigid = glCreateProgramObjectARB();
    glAttachObjectARB(ProgramRigid,RigidVertexProgram);
    glAttachObjectARB(ProgramRigid,FragmentProgram);
    glLinkProgramARB(ProgramRigid);
    printf("Rigid Shader:\n");
    printInfoLog(ProgramRigid);
    printf("\n\n");

    // Get the location of our attributes...
    BoneMatricesLocSkinned = glGetUniformLocationARB(ProgramSkinned, "BoneMatrices");
    MyNormalLocSkinned = glGetAttribLocationARB(ProgramSkinned, "MyNormal");
    BlendWeightsLocSkinned = glGetAttribLocationARB(ProgramSkinned, "BlendWeights");
    BlendIndicesLocSkinned = glGetAttribLocationARB(ProgramSkinned, "BlendIndices");

    return true;
}

void CleanupOpenGL()
{
    glDeleteObjectARB(ProgramRigid);
    glDeleteObjectARB(ProgramSkinned);
    glDeleteObjectARB(RigidVertexProgram);
    glDeleteObjectARB(SkinnedVertexProgram);
    glDeleteObjectARB(FragmentProgram);
}

