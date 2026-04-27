// =======================================================================================
// s3tc_opengl.cpp: Demonstrates downloading an S3TC compressed texture
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "granny.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>



#if defined(_MACOSX) || defined(linux)
  #include <GL/glew.h>
  #include <GL/glut.h>
#else
  #include <windows.h>
  #include <gl/gl.h>
  #include "glut/glut.h"
#endif

#if defined(_MACOSX) || defined(linux)
#define _stricmp strcasecmp
#define BASE_DIR "../../"
#else
#define BASE_DIR ""
#endif


using namespace std;

// For the purposes of this demo, texture.gr2 contains nothing but
// the compressed texture
char const* const GlobalSceneFilename     =  BASE_DIR "media/s3tc_texture.gr2";

// Some global parameters...
const char* MainWindowTitle = "S3TC/OpenGL GrannySample";
GLuint g_texName = 0;



bool CreateDemoTexture(granny_file_info*);
void Render();

#ifdef _WIN32
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT                   0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT                  0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT                  0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT                  0x83F3

typedef void (APIENTRY* PFNGLCOMPRESSEDTEXIMAGE2DARBPROC) (GLenum target, GLint level,
                                                           GLenum internalformat,
                                                           GLsizei width, GLsizei height,
                                                           GLint border,
                                                           GLsizei imageSize,
                                                           const GLvoid *data);
PFNGLCOMPRESSEDTEXIMAGE2DARBPROC glCompressedTexImage2DARB;
#endif

/* DEFTUTORIAL EXPGROUP(TutorialS3TC_OpenGL) (TutorialS3TC_OpenGL_Introduction, Introduction) */
/* This tutorial demonstrates how to download an S3TC (also called: DXTC) compressed
   texture directly to an OpenGL texture handle.  This prevents both the runtime costs,
   and quality problems associated with putting the texture through a decompress
   recompress cycle.  The bits found in the miplevels of an S3TC compressed $texture can
   be directly sent to the card.  In order to determine the correct format (DXT[1-5]), you
   simply need to intepret the value of SubFormat found in the $texture as a
   $s3tc_texture_format, and map that to the appropriate OpenGL enumerant.  You need to
   ensure that the OpenGL driver supports the GL_EXT_texture_compression_s3tc extension to
   use this technique.

   Prerequisites to this demo:
   $* $TutorialBasicLoading
   $-

   Function index:
   $* $TutorialS3TC_OpenGL_main
   $* $TutorialS3TC_OpenGL_CreateDemoTexture
   $* $TutorialS3TC_OpenGL_Render
   $-

*/


/* DEFTUTORIAL EXPGROUP(TutorialS3TC_OpenGL) (TutorialS3TC_OpenGL_main, main) */
int main( int argc, char** argv )
{
    // Check the versions
    if(!GrannyVersionsMatch)
    {
        printf("Warning: the Granny DLL currently loaded "
               "doesn't match the .h file used during compilation\n");
        return EXIT_FAILURE;
    }

    /* Initialize the GLUT application, and grab the function pointer for the
       CompressedTeximage2DARB function. */
    glutInit(&argc, argv);
    glutInitWindowSize(1024, 1024);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutCreateWindow(MainWindowTitle);
    glutDisplayFunc(Render);

#ifdef _WIN32
    glCompressedTexImage2DARB = (PFNGLCOMPRESSEDTEXIMAGE2DARBPROC)wglGetProcAddress("glCompressedTexImage2DARB");
#else
     // Initialize the GL Extension Wrangler
    glewInit();
#endif

    if( !glCompressedTexImage2DARB )
    {
        return EXIT_FAILURE;
    }

    
    /* Load the scene file. */
    granny_file*      SceneFile     = NULL;
    granny_file_info* SceneFileInfo = NULL;
    {
        SceneFile = GrannyReadEntireFile(GlobalSceneFilename);
        if (SceneFile == NULL)
            return EXIT_FAILURE;

        SceneFileInfo = GrannyGetFileInfo(SceneFile);
        if (SceneFileInfo == NULL)
            return EXIT_FAILURE;
    }
    assert(SceneFile && SceneFileInfo);

    /* Create our demo objects.  Note that the order is important here, CreateDemoModels
       will be looking up textures as it iterates, and CreateSharedPoses will scan the
       created models. */
    if (CreateDemoTexture(SceneFileInfo) == false)
    {
        return EXIT_FAILURE;
    }

    /* In this demo, once we've created the texture, we have no further need for the $file
       or $file_info, so simply free that memory now.  As always, freeing the $file
       removes the memory backing the $file_info, so we can simply NULL out that pointer.
       (See $TutorialBasicLoading for details.)

       Before we do that, we'll demonstrate one important feature of the Granny file API,
       which is that you can selectively throw away portions of the $file that you no
       longer need.  In this case, once we've downloaded the pixels to the hardware,
       there's no point in keeping the bits around in main memory as well, so we'll free
       the $StandardTextureSection.  This will remove the pixels from memory, while
       leaving the rest of the file untouched.  There are a few things to remember about
       this process:
       $* Only the pixel bytes are freed.  You are still able to access the $texture
          members like Width and Height after freeing the texture section.
       $* This only works if you've maintained the default Granny file structure in your
          toolchain.  If you write tools that change the section layout from the default
          created by the exporter, this may cause unexpected problems.  If you want to
          restore the default section layout, you can always use the Preprocessor command
          "ResectionFile"
       $* If you have "surface lost" conditions in your game, you may have to reload the
          file to recover the missing pieces.
       $-

       After a $FreeFileSection call, you can still bin the rest of the memory with
       $FreeFile, as always.
    */
    GrannyFreeFileSection(SceneFile, GrannyStandardTextureSection);
    GrannyFreeFile(SceneFile);
    SceneFile = NULL;
    SceneFileInfo = NULL;

    /* Enter the GLUT event loop. */
    glutShowWindow();
    glutMainLoop();  // never actually returns...

    return EXIT_SUCCESS;
}


/* DEFTUTORIAL EXPGROUP(TutorialS3TC_OpenGL) (TutorialS3TC_OpenGL_CreateDemoTexture, CreateDemoTexture) */
/* This function creates the Direct3D texture objects for the first (and, we assume, the
   only) texture contained in the Granny file.  Included here is a small function for
   transforming the SubFormat (which in the case of an S3TC encoded texture is a
   $s3tc_texture_format) into the appropriate DirectX constant.
*/
GLint GetOpenGLFormat(granny_s3tc_texture_format SubFormat)
{
    switch (SubFormat)
    {
        // DXT1 (no alpha)
        case GrannyS3TCBGR565:
        {
            return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        } break;

        // DXT1 (w/alpha)
        case GrannyS3TCBGRA5551:
        {
            return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        } break;

        // DXT2/3
        case GrannyS3TCBGRA8888MappedAlpha:
        {
            return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        } break;

        // DXT4/5
        case GrannyS3TCBGRA8888InterpolatedAlpha:
        {
            return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        } break;

        default:
            // Invalid subformat, should never reach here
            assert(false);
            return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
    }
}

bool CreateDemoTexture(granny_file_info* FileInfo)
{
    assert(FileInfo);

    if (FileInfo->TextureCount == 0)
        return false;

    granny_texture* Texture = FileInfo->Textures[0];
    assert(Texture != NULL);

    /* We only want to consider S3TC textures for this demo, so ensure that the texture is
       in the appropriate format */
    if (Texture->Encoding != GrannyS3TCTextureEncoding)
        return false;

    /* Grab only the first image of the texture, we're not going to worry about cube maps,
       or other exotica. */
    assert(Texture->ImageCount > 0);
    granny_texture_image* Image = &Texture->Images[0];

    /* Select the correct D3DFORMAT enumeration from the SubFormat */
    GLint TexFormat = GetOpenGLFormat(granny_s3tc_texture_format(Texture->SubFormat));

    /* Create the texture surface and copy the pixels. */
    GLuint TexName;
    glGenTextures(1, &TexName);
    glBindTexture(GL_TEXTURE_2D, TexName);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

    {for(granny_int32x Mip = 0; Mip < Image->MIPLevelCount; ++Mip)
    {
        granny_int32x MipWidth  = (Texture->Width >> Mip);
        granny_int32x MipHeight = (Texture->Height >> Mip);
		if (MipWidth  == 0) MipWidth  = 1;
		if (MipHeight == 0) MipHeight = 1;

        glCompressedTexImage2DARB(GL_TEXTURE_2D,
                                  Mip,
                                  TexFormat,
                                  MipWidth, MipHeight,
                                  0,
                                  Image->MIPLevels[Mip].PixelByteCount,
                                  Image->MIPLevels[Mip].PixelBytes);
    }}

    /* We're storing the texture in a global pointer, so just stuff it in there... */
    g_texName = TexName;
    return true;
}

/* DEFTUTORIAL EXPGROUP(TutorialS3TC_OpenGL) (TutorialS3TC_OpenGL_Render, Render) */
/* This routine just sets draws a full screen quad using our global texture with the fixed
   function pipeline. */
void Render()
{
    glClear(GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT |
            GL_STENCIL_BUFFER_BIT);
    
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_texName);

    glBegin(GL_QUADS);
    {
        glVertex2f(-1, -1); glTexCoord2f(0, 0);
        glVertex2f(-1,  1); glTexCoord2f(1, 0);
        glVertex2f( 1,  1); glTexCoord2f(1, 1);
        glVertex2f( 1, -1); glTexCoord2f(0, 1);
    }
    glEnd();

    glutSwapBuffers();
	glutPostRedisplay();
}


/* DEFTUTORIAL EXPGROUP(TutorialS3TC_OpenGL) (TutorialS3TC_OpenGL_Summary, Summary) */
/* Well that was fairly straightforward!  In a real application, you'll want to handle
   textures of any format, of course, but this will allow you to send your S3TC textures
   to the hardware with maximal efficiency.
*/

/* DEFTUTORIALEND */

