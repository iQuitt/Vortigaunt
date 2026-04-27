// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "simple_model_helper.h"
#include <string.h>
#include <assert.h>

IDirect3DVertexDeclaration9*  g_pVertexDecl_Rigid = NULL;
IDirect3DVertexDeclaration9*  g_pVertexDecl_Skinned = NULL;

IDirect3DTexture9* g_DefaultWhiteTexture = NULL;
ID3DXEffect* g_pEffect = NULL;
D3DXHANDLE   g_RigidTechnique = NULL;
D3DXHANDLE   g_SkinnedTechnique = NULL;

D3DXHANDLE g_hMatObj2World;
D3DXHANDLE g_hMatWorld2View;
D3DXHANDLE g_hMatView2Clip;
D3DXHANDLE g_hDirFromLight;
D3DXHANDLE g_hLightColor;
D3DXHANDLE g_hAmbientColor;
D3DXHANDLE g_hSkinningMatrices;
D3DXHANDLE g_hDiffuseTexture;

using namespace std;

#define CHECKED_RELEASE(x) { if (x) { (x)->Release(); } (x) = NULL; } 0

char const* const ShaderFile = "media\\Shaders\\SimpleModelShader.hlsl";

bool InitSimpleModelHelper(IDirect3DDevice9* pD3DDevice)
{
    ID3DXBuffer* ErrorBuffer = NULL;
    if (D3DXCreateEffectFromFile(pD3DDevice,
                                 ShaderFile,
                                 NULL,
                                 NULL,
#if !defined(DEBUG_VS)
                                 0,
#else
                                 D3DXSHADER_DEBUG | D3DXSHADER_SKIPOPTIMIZATION,
#endif
                                 NULL,
                                 &g_pEffect, &ErrorBuffer) != S_OK)
    {
        OutputDebugString((char*)ErrorBuffer->GetBufferPointer());
        CHECKED_RELEASE(ErrorBuffer);
        return false;
    }

    g_hMatObj2World     = g_pEffect->GetParameterByName ( NULL, "ObjToWorld" );
    g_hMatWorld2View    = g_pEffect->GetParameterByName ( NULL, "WorldToView" );
    g_hMatView2Clip     = g_pEffect->GetParameterByName ( NULL, "ViewToClip" );
    g_hDirFromLight     = g_pEffect->GetParameterByName ( NULL, "DirFromLight" );
    g_hLightColor       = g_pEffect->GetParameterByName ( NULL, "LightColour" );
    g_hAmbientColor     = g_pEffect->GetParameterByName ( NULL, "AmbientColour" );
    g_hSkinningMatrices = g_pEffect->GetParameterByName ( NULL, "BoneMatrices" );
    g_hDiffuseTexture   = g_pEffect->GetParameterByName ( NULL, "diffuse_texture" );

    g_SkinnedTechnique = g_pEffect->GetTechniqueByName ( "Skinned" );
    if (FAILED(g_pEffect->ValidateTechnique(g_SkinnedTechnique)))
        return false;

    g_RigidTechnique = g_pEffect->GetTechniqueByName ( "Rigid" );
    if (FAILED(g_pEffect->ValidateTechnique(g_RigidTechnique)))
        return false;


    // Create the rigid and skinned vertex declarations
    {
        D3DVERTEXELEMENT9 VertexElementsRigid[] = {
            { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
            { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };

        // Note the position[1-3] elements are pulled from vertex
        // stream 1, rather than 0.  We'll be binding stream 1 to our
        // matrix palette.  Essentially we're hijacking those vertex
        // semantics to represent a set of 4x3 matrices.
        const D3DVERTEXELEMENT9 VertexElementsSkinned[] = {
            { 0,  0, D3DDECLTYPE_FLOAT3,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,     0 },
            { 0, 12, D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT,  0 },
            { 0, 16, D3DDECLTYPE_UBYTE4,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },
            { 0, 20, D3DDECLTYPE_FLOAT3,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,       0 },
            { 0, 32, D3DDECLTYPE_FLOAT2,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     0 },
            D3DDECL_END()
        };

        pD3DDevice->CreateVertexDeclaration( VertexElementsRigid,   &g_pVertexDecl_Rigid   );
        pD3DDevice->CreateVertexDeclaration( VertexElementsSkinned, &g_pVertexDecl_Skinned );
    }

    // Create the default white texture...
    {
        pD3DDevice->CreateTexture(4, 4,
                                  1,
                                  0, D3DFMT_A8R8G8B8,
                                  D3DPOOL_MANAGED,
                                  &g_DefaultWhiteTexture, NULL);
        if (!g_DefaultWhiteTexture)
            return false;

        D3DLOCKED_RECT LockedRect;
        g_DefaultWhiteTexture->LockRect(0, &LockedRect, 0, 0);
        memset(LockedRect.pBits, 0xff, LockedRect.Pitch * 4);
        g_DefaultWhiteTexture->UnlockRect(0);
    }


    return true;
}


void CleanupSimpleModelHelper()
{
    CHECKED_RELEASE(g_DefaultWhiteTexture);
    CHECKED_RELEASE(g_pEffect);
    CHECKED_RELEASE(g_pVertexDecl_Skinned);
    CHECKED_RELEASE(g_pVertexDecl_Rigid);
}

void TransposedMatrix4x4(granny_real32 *Matrix)
{
    granny_real32 TMatrix[16];
    static const int remaps[16] = {
        0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15
    };

    for (int i = 0; i < 16; i++)
        TMatrix[i] = Matrix[remaps[i]];
    memcpy(Matrix, TMatrix, sizeof(TMatrix));
}

void SetTransposedMatrix4x4(D3DXHANDLE ShaderConstant,
                            float const *Matrix )
{
    float TMatrix[16];
    memcpy(TMatrix, Matrix, sizeof(TMatrix));
    TransposedMatrix4x4(TMatrix);

    g_pEffect->SetMatrix(ShaderConstant, (D3DXMATRIX const*)Matrix);
}

SimpleTexture::SimpleTexture()
  : Name(NULL),
    TextureBuffer(NULL)
{

}

SimpleTexture::~SimpleTexture()
{
    delete [] Name;
    Name = NULL;

    CHECKED_RELEASE(TextureBuffer);
}

SimpleMesh::SimpleMesh()
  : Mesh(NULL),
    MeshBinding(NULL),
    IndexBuffer(NULL),
    VertexBuffer(NULL)
{

}

SimpleMesh::~SimpleMesh()
{
    // We don't own these
    Mesh = NULL;
    MaterialBindings.clear();

    // We do own these
    if (MeshBinding)
    {
        GrannyFreeMeshBinding(MeshBinding);
        MeshBinding = NULL;
    }

    CHECKED_RELEASE(IndexBuffer);
    CHECKED_RELEASE(VertexBuffer);
}

SimpleModel::SimpleModel()
  : ModelInstance(NULL)
{

}

SimpleModel::~SimpleModel()
{
    if (ModelInstance)
    {
        GrannyFreeModelInstance(ModelInstance);
        ModelInstance = NULL;
    }

    {for(granny_uint32x i = 0; i < BoundMeshes.size(); ++i)
    {
        delete BoundMeshes[i];
        BoundMeshes[i] = NULL;
    }}

    BoundMeshes.clear();
}



bool CreateSimpleTextures(IDirect3DDevice9*       pD3DDevice,
                          granny_file_info*       Info,
                          vector<SimpleTexture*>& Textures)
{
    assert(Info);
    assert(pD3DDevice);
    assert(Textures.empty());

    /* Iterate across all the textures */
    {for(granny_int32x TexIdx = 0; TexIdx < Info->TextureCount; ++TexIdx)
    {
        granny_texture* GrannyTexture = Info->Textures[TexIdx];
        assert(GrannyTexture);

        if (GrannyTexture->TextureType == GrannyColorMapTextureType &&
            GrannyTexture->ImageCount == 1)
        {
            /* We select the appropriate 8-bit/channel format based on whether or not the
               image has an alpha channel. */
            D3DFORMAT D3DTexFormat;
            granny_pixel_layout const* GrannyTexFormat;
            {
                D3DTexFormat = D3DFMT_A8R8G8B8;
                GrannyTexFormat = GrannyBGRA8888PixelFormat;
            }

            /* Create and lock the texture surface, and use $CopyTextureImage to move the
               pixels.  This function will handle format conversion, if necessary,
               including adding or removing alpha components, decompressing from Bink or
               S3TC formats, etc. */
            SimpleTexture* NewTex = new SimpleTexture;
            pD3DDevice->CreateTexture(GrannyTexture->Width, GrannyTexture->Height,
                                      1,
                                      0, D3DTexFormat,
                                      D3DPOOL_MANAGED,
                                      &NewTex->TextureBuffer, NULL);
            if (!NewTex->TextureBuffer)
                return false;

            D3DLOCKED_RECT LockedRect;
            NewTex->TextureBuffer->LockRect(0, &LockedRect, 0, 0);

            GrannyCopyTextureImage(GrannyTexture, 0, 0,
                                   GrannyTexFormat,
                                   GrannyTexture->Width,
                                   GrannyTexture->Height,
                                   LockedRect.Pitch,
                                   LockedRect.pBits);

            NewTex->TextureBuffer->UnlockRect(0);


            /* Add the texture to our demo list, after copying the file name. */
            NewTex->Name = new char[strlen(GrannyTexture->FromFileName) + 1];
            strcpy(NewTex->Name, GrannyTexture->FromFileName);

            Textures.push_back(NewTex);
        }
    }}

    return true;
}

SimpleMesh* CreateBoundMesh(granny_mesh*            GrannyMesh,
                            granny_model_instance*  ModelInstance,
                            vector<SimpleTexture*>& Textures,
                            IDirect3DDevice9*       pD3DDevice)
{
    assert(GrannyMesh);
    assert(ModelInstance);
    assert(pD3DDevice);

    granny_model* SourceModel = GrannyGetSourceModel(ModelInstance);

    /* Create the binding.  $NewMeshBinding takes two $skeleton parameters to allow you to
       attach the mesh to a $skeleton other than the one it was modeled on.  Check
       $BindingAnimationsToMeshes_BindingMeshes for more details.  In this case, we're
       binding the mesh to the original $skeleton, so we just pass it twice. */
    SimpleMesh* NewMesh = new SimpleMesh;
    NewMesh->Mesh = GrannyMesh;
    NewMesh->MeshBinding = GrannyNewMeshBinding(GrannyMesh,
                                                SourceModel->Skeleton,
                                                SourceModel->Skeleton);

    /* Lookup the texture bindings in the list we created in CreateSimpleTextures.  Note
       that this process can insert NULL pointers in to the MaterialBindings array, which
       we will handle in Render(). */
    {for(granny_int32x MatIdx = 0; MatIdx < GrannyMesh->MaterialBindingCount; ++MatIdx)
    {
        granny_material* Material = GrannyMesh->MaterialBindings[MatIdx].Material;
        SimpleTexture* Found = NULL;
        if (Material && Material->MapCount >= 1)
        {
            {for(granny_int32x MapIdx = 0; MapIdx < Material->MapCount; ++MapIdx)
            {
                granny_material_map& Map = Material->Maps[MapIdx];

                if (_stricmp(Map.Usage, "color") == 0 && Map.Material->Texture)
                {
                    {for(granny_uint32x i = 0; i < Textures.size(); ++i)
                    {
                        if (_stricmp(Map.Material->Texture->FromFileName, Textures[i]->Name) == 0)
                        {
                            Found = Textures[i];
                            break;
                        }
                    }}
                }
            }}
        }

        NewMesh->MaterialBindings.push_back(Found);
    }}

    /* Now create the d3d index buffer */
    D3DFORMAT IndexFormat;
    int BytesPerIndex;
    int IndexCount = GrannyGetMeshIndexCount(GrannyMesh);
    if (GrannyGetMeshBytesPerIndex(GrannyMesh) == 2)
    {
        IndexFormat = D3DFMT_INDEX16;
        BytesPerIndex = 2;
    }
    else
    {
        assert(GrannyGetMeshBytesPerIndex(GrannyMesh) == 4);
        IndexFormat = D3DFMT_INDEX32;
        BytesPerIndex = 4;
    }

    int const IndexBufferSize = BytesPerIndex * IndexCount;
    pD3DDevice->CreateIndexBuffer(IndexBufferSize,
                                  D3DUSAGE_WRITEONLY,
                                  IndexFormat,
                                  D3DPOOL_MANAGED,
                                  &NewMesh->IndexBuffer, NULL);
    if (NewMesh->IndexBuffer == 0)
    {
        delete NewMesh;
        return NULL;
    }

    BYTE *Indices;
    NewMesh->IndexBuffer->Lock(0, IndexBufferSize, (void**)&Indices, 0);
    memcpy(Indices, GrannyGetMeshIndices(GrannyMesh), IndexBufferSize);
    NewMesh->IndexBuffer->Unlock();

    /* And the vertex buffer.  You can examine the D3D format specifications that we
       selected for g_pVertexDecl_Rigid and g_pVertexDecl_Skinned in
       dx9_rendering_helpers.cpp.  We chose the formats to correspond to the Granny vertex
       formats $pnt332_vertex (rigid) and $pwnt3432_vertex (skinned).  You are not
       restricted to built-in formats, check the main Granny docs for details on how to
       create your own format.

       For skinned meshes, we also need to check at this point that the number of bone
       bindings in the $mesh doesn't exceed the number of constant register slots we have
       available in our vertex shader.  If a source mesh exceeds the number of constant
       registers available to you on your minimum spec hardware, you can use the Granny
       Preprocessor to break the mesh up into renderable chunks without imposing
       restrictions on your artists. */
    granny_data_type_definition* VertexFormat;
    int VertexBufferSize;
    if (GrannyMeshIsRigid(GrannyMesh))
    {
        VertexFormat = GrannyPNT332VertexType;
        VertexBufferSize = (sizeof(granny_pnt332_vertex) *
                            GrannyGetMeshVertexCount(GrannyMesh));
    }
    else
    {
        VertexFormat = GrannyPWNT3432VertexType;
        VertexBufferSize = (sizeof(granny_pwnt3432_vertex) *
                            GrannyGetMeshVertexCount(GrannyMesh));
    }

    pD3DDevice->CreateVertexBuffer(VertexBufferSize,
                                   D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED,
                                   &NewMesh->VertexBuffer, NULL);
    if (NewMesh->VertexBuffer == 0)
    {
        delete NewMesh;
        return NULL;
    }

    BYTE *Vertices;
    NewMesh->VertexBuffer->Lock(0, VertexBufferSize, (void**)&Vertices, 0);
    GrannyCopyMeshVertices(GrannyMesh, VertexFormat, Vertices);
    NewMesh->VertexBuffer->Unlock();

    return NewMesh;
}

bool CreateSimpleModels(IDirect3DDevice9*       pD3DDevice,
                        granny_file_info*       Info,
                        vector<SimpleTexture*>& Textures,
                        vector<SimpleModel*>&   Models,
                        bool CreateAnimations /* = true */)
{
    assert(pD3DDevice);
    assert(Info);

    /* Iterate across all the models */
    {for(granny_int32x ModelIdx = 0; ModelIdx < Info->ModelCount; ++ModelIdx)
    {
        granny_model* GrannyModel = Info->Models[ModelIdx];
        assert(GrannyModel);

        if (GrannyModel->MeshBindingCount > 0)
        {
            SimpleModel* NewModel = new SimpleModel;
            Models.push_back(NewModel);

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

                SimpleMesh* NewMesh = CreateBoundMesh(GrannyMesh, NewModel->ModelInstance, Textures, pD3DDevice);
                assert(NewMesh);
                NewModel->BoundMeshes.push_back(NewMesh);
            }}

            /* We're going to use a "fire-and-forget" animation technique here, since
               we're simply going to loop a single animation until the application
               finishes.  Once we create the control, we tell Granny that it's allowed to
               free it when we free the $model_instance, and set the loop count to 0 to
               indicate that we want it to loop forever. */
            if (CreateAnimations)
            {
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
        }
    }}

    return true;
}


void SimpleModelSetup(granny_camera const& DemoCamera,
                      granny_real32 const* DirFromLight,
                      granny_real32 const* LightColour,
                      granny_real32 const* AmbientColour)
{
    /* Setup the camera and lighting parameters, which don't change over the course of
       the scene.  Note that Granny matrices are column-major, by default, while D3D
       matrices are row-major, which is why we call a utility function for the set
       here. */
    SetTransposedMatrix4x4(g_hMatWorld2View,  (FLOAT*)DemoCamera.View4x4);
    SetTransposedMatrix4x4(g_hMatView2Clip,   (FLOAT*)DemoCamera.Projection4x4);
    g_pEffect->SetFloatArray(g_hDirFromLight, (FLOAT*)DirFromLight, 3);
    g_pEffect->SetFloatArray(g_hLightColor,   (FLOAT*)LightColour, 3);
    g_pEffect->SetFloatArray(g_hAmbientColor, (FLOAT*)AmbientColour, 3);
}



void RenderSimpleModel(SimpleModel* Model,
                       granny_world_pose* WorldPose,
                       IDirect3DDevice9* pD3DDevice)
{
    pD3DDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    /* Iterate across all meshes */
    {for (size_t MeshIndex = 0; MeshIndex < Model->BoundMeshes.size(); ++MeshIndex)
    {
        SimpleMesh* Mesh = Model->BoundMeshes[MeshIndex];

        // Load the mesh's index buffer.
        pD3DDevice->SetIndices(Mesh->IndexBuffer);

        /* The ToBoneIndices holds the mapping from the contiguously index mesh bones to
           original source indices in the bound skeleton.  This is necessary for both
           rigid and skinned meshes.  There is an analogous call to get the mapping to the
           source skeleton, $GetMeshBindingFromBoneIndices, but you'll almost always want
           to use $GetMeshBindingToBoneIndices.  In our case, the two mappings are
           identical, but this is not always true. */
        int const *ToBoneIndices =
            GrannyGetMeshBindingToBoneIndices(Mesh->MeshBinding);

        if (GrannyMeshIsRigid(Mesh->Mesh))
        {
            /* It's a rigid mesh, so load the appropriate technique.  Note that this is
               pretty slow, normally you'd order your meshes to minimize the number of
               state switches. */
            g_pEffect->SetTechnique(g_RigidTechnique);
            pD3DDevice->SetVertexDeclaration( g_pVertexDecl_Rigid );
            pD3DDevice->SetStreamSource(0, Mesh->VertexBuffer, 0,
                                        sizeof(granny_pnt332_vertex));

            /* Now I look up the transform for this mesh, and load it.  (Note that we're
               making the assumption that Bone 0 represents the rigid transform for the
               model.  In most cases, this is a safe assumption.) */
            __declspec(align(16)) granny_matrix_4x4 CompositeMatrix;
            GrannyBuildIndexedCompositeBuffer(
                GrannyGetMeshBindingToSkeleton(Mesh->MeshBinding),
                WorldPose,
                ToBoneIndices, 1,
                &CompositeMatrix);

            g_pEffect->SetMatrix(g_hMatObj2World, (D3DXMATRIX const*)CompositeMatrix);
        }
        else
        {
            /* It's a skinned mesh, activate the appropriate technique. */
            int const NumMeshBones = GrannyGetMeshBindingBoneCount(Mesh->MeshBinding);
            assert(NumMeshBones <= 80);

            g_pEffect->SetTechnique(g_SkinnedTechnique);
            pD3DDevice->SetVertexDeclaration( g_pVertexDecl_Skinned );

            /* Our source vertices are set as stream source 0 */
            pD3DDevice->SetStreamSource(0, Mesh->VertexBuffer, 0,
                                        sizeof(granny_pwnt3432_vertex));

            /* Load the matrices into the constant registers.  This is about the simplest
               way to go about this.  Remember that I said we'd be talking about the call
               to $NewWorldPoseNoComposite?  Here's the payoff.  When you call that
               routine, it creates a $world_pose without a Composite array attached, which
               allows Granny to skip those matrices when $BuildWorldPose is called.  (You
               can get the same effect conditionally by calling $BuildWorldPoseNoComposite
               for $(world_pose)s that do have the composite array.)  Since we're just
               going to copy a few of the matrices into the constant buffer, we delay
               until this point, at which we know exactly which matrices we need, and
               exactly where they need to go.  $BuildIndexedCompositeBuffer is designed to
               do this for you.  (If you need the matrices transposed, use
               $BuildIndexedCompositeBufferTransposed.)

               The static buffer is necessary, since you cannot lock a constant buffer in
               DirectX 9.  (Wait for DX10, the most efficient way to do this will change
               again.)  We know that no mesh in our scene will bind to more than 80 bones,
               since we've preprocessed the data to ensure that all meshes are broken up
               into pieces of at most that size. */
            __declspec(align(16)) granny_matrix_4x4 CompositeBuffer[80];

            GrannyBuildIndexedCompositeBuffer(
                GrannyGetMeshBindingToSkeleton(Mesh->MeshBinding),
                WorldPose,
                ToBoneIndices, NumMeshBones,
                CompositeBuffer);

            g_pEffect->SetMatrixArray(g_hSkinningMatrices,
                                      (D3DXMATRIX const*)CompositeBuffer,
                                      NumMeshBones);
        }

        /* Now both the indices and vertices are loaded, so I can render.  Grab the
           material groups and spin over them, changing to the appropriate texture and
           rendering each batch.  A more savvy rendering loop might have instead built a
           sorted list of material groups to minimize texture changes, etc., but this is
           the most basic way to render. */
        int const VertexCount = GrannyGetMeshVertexCount(Mesh->Mesh);
        int GroupCount = GrannyGetMeshTriangleGroupCount(Mesh->Mesh);
        granny_tri_material_group *Group =
            GrannyGetMeshTriangleGroups(Mesh->Mesh);
        while(GroupCount--)
        {
            if(Group->MaterialIndex < int(Mesh->MaterialBindings.size()) &&
               Mesh->MaterialBindings[Group->MaterialIndex] != NULL)
            {
                SimpleTexture* Texture = Mesh->MaterialBindings[Group->MaterialIndex];
                g_pEffect->SetTexture(g_hDiffuseTexture, Texture->TextureBuffer);
            }
            else
            {
                g_pEffect->SetTexture(g_hDiffuseTexture, g_DefaultWhiteTexture);
            }

            // We know that there's only one pass, but let's be general about this
            UINT numPasses;
            g_pEffect->Begin(&numPasses, 0);
            for (UINT i = 0; i < numPasses; i++)
            {
                g_pEffect->BeginPass(i);

                pD3DDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,
                                                 0,
                                                 0, VertexCount,
                                                 3*Group->TriFirst,
                                                 Group->TriCount);

                g_pEffect->EndPass();
            }
            g_pEffect->End();

            ++Group;
        }
    }}
}
