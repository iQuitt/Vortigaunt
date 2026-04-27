// =======================================================================================
// ycocg_dx9.cpp: Demonstrates downloading an YCOCG compressed texture
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "ycocg_dx9.h"

#include <d3dx9.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>

using namespace std;

// For the purposes of this demo, texture.gr2 contains nothing but
// the compressed texture
char const* const GlobalSceneFilename     = "media\\ycocg_texture.gr2";

// ---------------------------------------------------------
// Global D3D Objects: Note that we only have a few shader
// objects, so we'll just make them global.
IDirect3D9*       g_pD3D = NULL;
IDirect3DDevice9* g_pD3DDevice = NULL;
IDirect3DTexture9* g_pTexture = NULL;
IDirect3DPixelShader9* g_pPixelShader = 0;

D3DXHANDLE g_hDiffuseTexture;

// ---------------------------------------------------------
// Win32 objects
const char* MainWindowTitle = "YCOCG/DX9 GrannySample";
HWND g_hwnd = NULL;
bool GlobalRunning = false;

bool CreateDemoTexture(granny_file_info*);

/* DEFTUTORIAL EXPGROUP(TutorialYCOCG_DX9) (TutorialYCOCG_DX9_Introduction, Introduction) */
/* This tutorial demonstrates how to download an YCOCG (also called: DXTC) compressed
   texture directly to a DirectX9 Texture object.  This prevents both runtime costs, and
   quality problems associated with putting the texture through a decompress recompress
   cycle.  The bits found in the miplevels of an YCOCG compressed $texture can be directly
   sent to the card.  In order to determine the correct format (DXT[1-5]), you simply need
   to intepret the value of SubFormat found in the $texture as a $ycocg_texture_format, and
   map that to the appropriate DirectX enumerant.

   Prerequisites to this demo:
   $* $TutorialBasicLoading
   $-

   Function index:
   $* $TutorialYCOCG_DX9_WinMain
   $* $TutorialYCOCG_DX9_CreateDemoTexture
   $* $TutorialYCOCG_DX9_Render
   $-

*/

/* DEFTUTORIAL EXPGROUP(TutorialYCOCG_DX9) (TutorialYCOCG_DX9_WinMain, WinMain) */
INT WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR, INT )
{
    // Check the versions
    if(!GrannyVersionsMatch)
    {
        printf("Warning: the Granny DLL currently loaded "
               "doesn't match the .h file used during compilation\n");
        return EXIT_FAILURE;
    }

    /* Create the D3D objects */
    if (InitializeD3D() == false)
    {
        printf("Error: unable to initialize D3D device.  "
               "DX9 compliant renderer required.\n");
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

    /* We don't care about time in this demo, since we're not updating anything, so we can
       dispense with the checks to $GetSystemSeconds.  Simply mark the whole window as
       invalid, so our WM_PAINT handler will run continuously. */
    GlobalRunning = true;
    while (GlobalRunning)
    {
        InvalidateRect(g_hwnd, 0, false);
        MSG Message;
        while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }
    }

    CleanupD3D();
    return EXIT_SUCCESS;
}


/* DEFTUTORIAL EXPGROUP(TutorialYCOCG_DX9) (TutorialYCOCG_DX9_CreateDemoTexture, CreateDemoTexture) */
/* This function creates the Direct3D texture objects for the first (and, we assume, the
   only) texture contained in the Granny file.  Included here is a small function for
   transforming the SubFormat (which in the case of an YCOCG encoded texture is a
   $ycocg_texture_format) into the appropriate DirectX constant.
*/
D3DFORMAT GetDirectXFormat()
{
    return D3DFMT_DXT5;
}

bool CreateDemoTexture(granny_file_info* FileInfo)
{
    assert(FileInfo);

    if (FileInfo->TextureCount == 0)
        return false;

    granny_texture* Texture = FileInfo->Textures[0];
    assert(Texture != NULL);

    /* We only want to consider YCOCG textures for this demo, so ensure that the texture is
       in the appropriate format */
    if (Texture->Encoding != GrannyYCoCgTextureEncoding)
        return false;

    /* Grab only the first image of the texture, we're not going to worry about cube maps,
       or other exotica. */
    assert(Texture->ImageCount > 0);
    granny_texture_image* Image = &Texture->Images[0];

    /* Select the correct D3DFORMAT enumeration from the SubFormat */
    D3DFORMAT D3DTexFormat = GetDirectXFormat();

    /* Create and lock the texture surface, and simply memcpy the pixels. */
    IDirect3DTexture9* NewTex = NULL;
    g_pD3DDevice->CreateTexture(Texture->Width, Texture->Height,
                                Image->MIPLevelCount,
                                0, D3DTexFormat,
                                D3DPOOL_MANAGED,
                                &NewTex, NULL);
    if (!NewTex)
        return false;

    {for(granny_int32x Mip = 0; Mip < Image->MIPLevelCount; ++Mip)
    {
        granny_int32x const MipHeight = (Texture->Height >> Mip);
        assert(MipHeight > 0);

        D3DLOCKED_RECT LockedRect;
        NewTex->LockRect(Mip, &LockedRect, 0, 0);

        // Here's a detail to pay attention to.  The Granny Stride field is the number of
        // bits per row of pixels.  For YCOCG, what we care about is the stride per 4 rows,
        // since 4x4 blocks are the atomic unit for YCOCG.
        granny_int32 const RowStride   = Image->MIPLevels[Mip].Stride;
        granny_int32 const BlockStride = RowStride * 4;
        granny_uint8 const* Pixels     = (granny_uint8 const*)Image->MIPLevels[Mip].PixelBytes;
        assert(BlockStride <= LockedRect.Pitch);

        // We'll be fastidious about the Pitch/BlockStride here.  If Pitch == BlockStride,
        // you could obviously simply blast the bits with a single memcpy.
        {for(granny_int32x y = 0; y < MipHeight/4; ++y)
        {
            granny_uint8 const* SourceRow = Pixels + y*BlockStride;
            granny_uint8* DestRow = ((granny_uint8*)LockedRect.pBits) + y*LockedRect.Pitch;

            memcpy(DestRow, SourceRow, BlockStride);
        }}

        NewTex->UnlockRect(0);
    }}

    /* We're storing the texture in a global pointer, so just stuff it in there... */
    g_pTexture = NewTex;
    return true;
}

/* DEFTUTORIAL EXPGROUP(TutorialYCOCG_DX9) (TutorialYCOCG_DX9_Render, Render) */
/* This routine just sets draws a full screen quad using our global texture with the fixed
   function pipeline. */
struct my_rhw_vertex
{
    float x,y,z,w;
    DWORD diffuse;
    float u,v;
};
#define MY_RHW_VERTEX_FVF ( D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE2(0) )

void Render()
{
    if (!GlobalRunning)
        return;
    assert(g_pD3DDevice);
    assert(g_pTexture);

    LPDIRECT3DSURFACE9 pBackBuffer;
    D3DSURFACE_DESC SurfaceDesc;
    g_pD3DDevice->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );
    pBackBuffer->GetDesc(&SurfaceDesc);
    pBackBuffer->Release();

    /* Clear the backbuffer to a blue color, and start rendering our scene */
    g_pD3DDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                         D3DCOLOR_XRGB(0,0,255), 1.0f, 0L );

    if(SUCCEEDED(g_pD3DDevice->BeginScene()))
    {
        // This is just a huge bolus of uninteresting code that simply sets up the fixed
        // function pipe and renders the texture to the entire framebuffer
        {
            float left   = 0.0f;
            float right  = (float)SurfaceDesc.Width;
            float top    = 0.0f;
            float bottom = (float)SurfaceDesc.Height;
            my_rhw_vertex Verts[4] = {
                // x      y       z       w     diffuse     u     v
                {  left,  top,    0.999f, 0.5f, 0xffffffff, 0.0f, 0.0f },
                {  right, top,    0.999f, 0.5f, 0xffffffff, 1.0f, 0.0f },
                {  left,  bottom, 0.999f, 0.5f, 0xffffffff, 0.0f, 1.0f },
                {  right, bottom, 0.999f, 0.5f, 0xffffffff, 1.0f, 1.0f },
            };

			// normally you'd look this up in the constant table, but I happen to know what's up here.  :)
			g_pD3DDevice->SetTexture( 0, g_pTexture );
            g_pD3DDevice->SetFVF ( MY_RHW_VERTEX_FVF );
            g_pD3DDevice->SetPixelShader(g_pPixelShader);

            const WORD Indices[6] = { 0, 1, 2, 1, 3, 2 };
            g_pD3DDevice->DrawIndexedPrimitiveUP ( D3DPT_TRIANGLELIST,
                                                   0, 4,
                                                   2,
                                                   Indices, D3DFMT_INDEX16,
                                                   Verts, sizeof ( Verts[0] ) );
        }

        g_pD3DDevice->EndScene();
    }

    // Present the backbuffer contents to the display
    g_pD3DDevice->Present( NULL, NULL, NULL, NULL );
}


/* DEFTUTORIALEND */

