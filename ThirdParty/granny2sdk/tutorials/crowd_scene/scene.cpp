/* ========================================================================
   $RCSfile: $
   $Date$
   $Revision$
   $Creator: Dave Moore $
   $Notice: $
   ======================================================================== */
#include "scene.h"
#include "crowd_scene.h"
#include "shader_assemble.h"

#include <algorithm>

#include "d3dapp/dxutil.h"


// Loads and transforms a file into the proper space...
static bool LoadGrannyFile(const char*        Filename,
                           granny_file**      TheLoadedFile,
                           granny_file_info** TheFileInfo);

static bool CreateSkinnedShader(LPDIRECT3DDEVICE9 d3ddev, LPDIRECT3DVERTEXSHADER9* ppShader);


// Weights and normals and positions, oh, my!
typedef granny_pwnt3432_vertex MyVertex;
#define MyVertexType           GrannyPWNT3432VertexType

const D3DVERTEXELEMENT9 MyVertexElements[] =
{
    { 0,  0, D3DDECLTYPE_FLOAT3,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,       0 },
    { 0, 12, D3DDECLTYPE_D3DCOLOR,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT,    0 },
    { 0, 16, D3DDECLTYPE_D3DCOLOR,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES,   0 },
    { 0, 20, D3DDECLTYPE_FLOAT3,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,         0 },
    { 0, 32, D3DDECLTYPE_FLOAT2,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,       0 },
    D3DDECL_END()
};

// Our vertex shader constant defines...
#define VSCONSTANT_NUM765_NUM1OVER4096_NUM1OVER2_NUM1 0
#define VSCONSTANT_NUM0_NUM2_NUM1_x         1
#define VSCONSTANT_LIGHT_DIRECTION_ALPHA         2
#define VSCONSTANT_LIGHT_COLOUR_AMBIENT          3
#define VSCONSTANT_FIRST_VIEWPROJ_MATRIX4        4
#define VSCONSTANT_FIRST_BONE_MATRIX3            8

// We use vs1.1 shaders, so it will always have at least 96 constants
// available.  Each bone is a 4x3 matrix, so takes 3 constants.

#define VSCONSTANT_MAX_CONSTANTS  (256)
#define VSCONSTANT_MAX_BONE_COUNT (( VSCONSTANT_MAX_CONSTANTS - VSCONSTANT_FIRST_BONE_MATRIX3 ) / 3)



// ---------------------------------------------------------------------
// Internal structures
// ---------------------------------------------------------------------
struct Scene::WrappedFile
{
    char*             Filename;
    granny_file*      GrannyFile;
    granny_file_info* GrannyFileInfo;

    WrappedFile(const char*       name,
                granny_file*      file,
                granny_file_info* fileinfo)
    {
        Filename = new char[strlen(name) + 1];
        strcpy(Filename, name);

        GrannyFile     = file;
        GrannyFileInfo = fileinfo;
    }

    ~WrappedFile()
    {
        // Kill the submembers
        delete [] Filename;
        GrannyFreeFile(GrannyFile);

        Filename       = NULL;
        GrannyFile     = NULL;
        GrannyFileInfo = NULL;
    }
};

struct Texture
{
    Texture(granny_texture* TheTexture);
    ~Texture();

    void CreateD3DObjects(LPDIRECT3DDEVICE9 d3ddev);
    void DestroyD3DObjects(LPDIRECT3DDEVICE9 d3ddev);

    // Just leave these public for convenience
    granny_texture* GrannyTexture;
    LPDIRECT3DTEXTURE9 D3DTexture;
};


// ---------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------
template <class mem_t>
void DeleteVecMembers(std::vector<mem_t>& vec)
{
    for (std::vector<mem_t>::iterator itr = vec.begin();
         itr != vec.end();
         ++itr)
    {
        ASSERT(*itr);
        delete *itr;
        *itr = NULL;
    }
    vec.clear();
}


// ---------------------------------------------------------------------
// Scene members
// ---------------------------------------------------------------------
Scene::Scene()
  : D3DVertexShaderAnimated(NULL),
    D3DVertexDeclarationAnimated(NULL)
{

}

Scene::~Scene()
{
    // Kill everything.  Note that grannyFiles should
    //  go last...
    DeleteVecMembers(m_textures);
    DeleteVecMembers(m_animations);
    DeleteVecMembers(m_models);
    DeleteVecMembers(m_grannyFiles);
}

bool Scene::FindOrLoadGrannyFile(const char*        Filename,
                                 granny_file**      TheLoadedFile,
                                 granny_file_info** TheFileInfo)
{
    ASSERT(Filename);
    ASSERT(TheLoadedFile);
    ASSERT(TheFileInfo);

    // Check if we already have it
    for (size_t i = 0; i < m_grannyFiles.size(); i++)
    {
        ASSERT(m_grannyFiles[i]);
        ASSERT(m_grannyFiles[i]->Filename);

        if (_stricmp(Filename, m_grannyFiles[i]->Filename) == 0)
        {
            ASSERT(m_grannyFiles[i]->GrannyFile && m_grannyFiles[i]->GrannyFileInfo);
            *TheLoadedFile = m_grannyFiles[i]->GrannyFile;
            *TheFileInfo   = m_grannyFiles[i]->GrannyFileInfo;
            return true;
        }
    }

    // Try to load and process the new file...
    if (LoadGrannyFile(Filename, TheLoadedFile, TheFileInfo))
    {
        // Got it!
        m_grannyFiles.push_back(new WrappedFile(Filename, *TheLoadedFile, *TheFileInfo));
        return true;
    }
    else
    {
        // Failure
        return false;
    }
}

Texture* Scene::FindTexture(granny_texture* TheTexture )
{
    ASSERT(TheTexture);

    for (size_t i = 0; i < m_textures.size(); i++)
    {
        if (m_textures[i]->GrannyTexture == TheTexture)
            return m_textures[i];
    }

    // Didn't find the texture. So create it.
    m_textures.push_back(new Texture(TheTexture));
    return m_textures.back();
}


AnimatedModel* Scene::FindOrLoadModel(const char* Filename,
                                      const char* Modelname)
{
    ASSERT(Filename);
    ASSERT(Modelname);

    // First, look for the model in our cache...
    for (size_t i = 0; i < m_models.size(); i++)
    {
        if (_stricmp(Modelname, m_models[i]->GetModelName()) == 0)
            return m_models[i];
    }

    granny_file*      GrannyFile     = NULL;
    granny_file_info* GrannyFileInfo = NULL;
    if (FindOrLoadGrannyFile(Filename, &GrannyFile, &GrannyFileInfo))
    {
        for ( int ModelNum = 0; ModelNum < GrannyFileInfo->ModelCount; ModelNum++ )
        {
            if (_stricmp(Modelname, GrannyFileInfo->Models[ModelNum]->Name) == 0)
            {
                // Found it!
                m_models.push_back(new AnimatedModel(this, GrannyFileInfo->Models[ModelNum]));
                return m_models.back();
            }
        }
    }

    // Doesn't exist...
    return NULL;
}


Animation* Scene::FindOrLoadAnimation(const char* Filename,
                                      const char* AnimationNameKey,
                                      const char* TrackGroupName)
{
    ASSERT(Filename);
    ASSERT(AnimationNameKey);
    ASSERT(TrackGroupName);

    // First, look for the animation in our cache...
    for (size_t i = 0; i < m_animations.size(); i++)
    {
        Animation* pAnimation = m_animations[i];

        // TODO: stristr here instead of strstr
        if (_stricmp(pAnimation->GetFilename(), Filename) == 0 &&
            strstr(pAnimation->GetAnimationName(), AnimationNameKey) != NULL &&
            _stricmp(pAnimation->GetTrackGroupName(), TrackGroupName) == 0)
        {
            // Got it!
            return pAnimation;
        }
    }

    granny_file*      GrannyFile     = NULL;
    granny_file_info* GrannyFileInfo = NULL;
    if (FindOrLoadGrannyFile(Filename, &GrannyFile, &GrannyFileInfo) == false)
        return NULL;

    granny_animation* TheAnimation = NULL;
    for (int i = 0; i < GrannyFileInfo->AnimationCount; i++)
    {
        // TODO: stristr here instead of strstr
        granny_animation* ThisAnimation = GrannyFileInfo->Animations[i];
        if (strstr(ThisAnimation->Name, AnimationNameKey) != NULL)
        {
            TheAnimation = ThisAnimation;
            break;
        }
    }
    if (TheAnimation == NULL)
        return NULL;

    granny_track_group* TheTrackGroup = NULL;
    for (int i = 0; i < TheAnimation->TrackGroupCount; i++)
    {
        granny_track_group* ThisTrackGroup = TheAnimation->TrackGroups[i];
        if (_stricmp(ThisTrackGroup->Name, TrackGroupName) == 0)
        {
            TheTrackGroup = ThisTrackGroup;
            break;
        }
    }
    if (TheTrackGroup == NULL)
        return NULL;

    // Hah!  Found it.  Add the animation
    m_animations.push_back(new Animation(this, Filename, TheAnimation, TheTrackGroup));
    return m_animations.back();
}


void Scene::CreateD3DObjects ( LPDIRECT3DDEVICE9 d3ddev )
{
    ASSERT(D3DVertexShaderAnimated == NULL);
    ASSERT(D3DVertexDeclarationAnimated == NULL);

    for (size_t i = 0; i < m_textures.size(); i++)
        m_textures[i]->CreateD3DObjects(d3ddev);

    for (size_t i = 0; i < m_models.size(); i++)
        m_models[i]->CreateD3DObjects(d3ddev);

    // Create the shader and the vertex declaration for the animated path
    {
        CreateSkinnedShader(d3ddev, &D3DVertexShaderAnimated);
        ASSERT(D3DVertexShaderAnimated != NULL);

        // Create the vertex declaration
        HRESULT hres = d3ddev->CreateVertexDeclaration ( MyVertexElements, &D3DVertexDeclarationAnimated );
        hres = hres;
        ASSERT ( SUCCEEDED ( hres ) );
    }
}


void Scene::DestroyD3DObjects ( LPDIRECT3DDEVICE9 d3ddev )
{
    for (size_t i = 0; i < m_textures.size(); i++)
        m_textures[i]->DestroyD3DObjects(d3ddev);

    for (size_t i = 0; i < m_models.size(); i++)
        m_models[i]->DestroyD3DObjects(d3ddev);

    SAFE_RELEASE(D3DVertexShaderAnimated);
    SAFE_RELEASE(D3DVertexDeclarationAnimated);
}


// ---------------------------------------------------------------------
//
// ---------------------------------------------------------------------
AnimatedModel::AnimatedModel(Scene*        TheScene,
                             granny_model* TheGrannyModel)
  : OwningScene(TheScene),
    GrannyModel(TheGrannyModel),
    GrannyWorldPose(GrannyNewWorldPose(TheGrannyModel->Skeleton->BoneCount))
{
    ASSERT(TheGrannyModel);

    // Create our meshes...
    for (int i = 0; i < GrannyModel->MeshBindingCount; i++)
    {
        m_meshes.push_back(new Mesh(this, GrannyModel->MeshBindings[i].Mesh, GrannyModel->Skeleton));
    }
}


AnimatedModel::~AnimatedModel()
{
    GrannyFreeWorldPose(GrannyWorldPose);
    GrannyWorldPose = NULL;

    for (size_t i = 0; i < m_meshes.size(); i++)
    {
        delete m_meshes[i];
    }
    m_meshes.clear();
}


void AnimatedModel::CreateD3DObjects ( LPDIRECT3DDEVICE9 d3ddev )
{
    for (size_t i = 0; i < m_meshes.size(); i++)
        m_meshes[i]->CreateD3DObjects(d3ddev);
}


void AnimatedModel::DestroyD3DObjects( LPDIRECT3DDEVICE9 d3ddev )
{
    for (size_t i = 0; i < m_meshes.size(); i++)
        m_meshes[i]->DestroyD3DObjects(d3ddev);
}


void AnimatedModel::Render(LPDIRECT3DDEVICE9 d3ddev, granny_world_pose* ThePose)
{
    // Grab the raw 4x4 matrix data from the world pose.
    granny_matrix_4x4 *CompositeBuffer = GrannyGetWorldPoseComposite4x4Array (ThePose);

    for (size_t i = 0; i < m_meshes.size(); i++)
    {
        m_meshes[i]->Render(d3ddev, CompositeBuffer);
    }
}


// ---------------------------------------------------------------------
//
// ---------------------------------------------------------------------
Mesh::Mesh(AnimatedModel*      TheModel,
           granny_mesh*     TheGrannyMesh,
           granny_skeleton* SkeletonToBind)
  : OwningModel(TheModel),
    GrannyMesh(TheGrannyMesh),
    GrannyMeshBinding(GrannyNewMeshBinding(GrannyMesh, SkeletonToBind, SkeletonToBind))
{
    // Extract mesh groups
    granny_tri_material_group* GrannyGroups = GrannyGetMeshTriangleGroups (GrannyMesh);
    for ( int GroupNum = 0; GroupNum < GrannyGetMeshTriangleGroupCount ( GrannyMesh ); GroupNum++ )
    {
        TriGroup newGroup;
        granny_material* GrannyMaterial = GrannyMesh->MaterialBindings[GrannyGroups[GroupNum].MaterialIndex].Material;
        granny_texture*  GrannyTexture  = GrannyGetMaterialTextureByType ( GrannyMaterial, GrannyDiffuseColorTexture );
        if ( GrannyTexture != NULL )
        {
            newGroup.ColorTexture = OwningModel->GetOwningScene()->FindTexture(GrannyTexture);
        }

        GrannyTexture = GrannyGetMaterialTextureByType ( GrannyMaterial, GrannyBumpHeightTexture );
        if ( GrannyTexture != NULL )
        {
            // We have a normal map!
            newGroup.NormalTexture = OwningModel->GetOwningScene()->FindTexture(GrannyTexture);
        }

        newGroup.FirstTri = GrannyGroups[GroupNum].TriFirst;
        newGroup.NumTris  = GrannyGroups[GroupNum].TriCount;
        TriGroupings.push_back(newGroup);
    }
}

Mesh::~Mesh()
{
    GrannyFreeMeshBinding(GrannyMeshBinding);
    GrannyMeshBinding = NULL;
}


void Mesh::CreateD3DObjects(LPDIRECT3DDEVICE9 d3ddev)
{
    for (size_t GroupNum = 0; GroupNum < TriGroupings.size(); GroupNum++)
    {
        TriGroup& Group = TriGroupings[GroupNum];
        ASSERT(Group.D3DVertexBuffer == NULL);
        ASSERT(Group.D3DIndexBuffer == NULL);

        // Make the vb and ib for this group.  Gnarly nasty remapping in
        //  here, so shove it off to the side.  On exit, the vb and ib
        //  should be created, as well as the bone remap buffer...
        Group.CreateD3DObjects(d3ddev, this);
    }
}

void Mesh::DestroyD3DObjects(LPDIRECT3DDEVICE9 /*d3ddev*/)
{
    for (size_t GroupNum = 0; GroupNum < TriGroupings.size(); GroupNum++)
    {
        TriGroupings[GroupNum].NumVertices = -1;
        SAFE_RELEASE(TriGroupings[GroupNum].D3DIndexBuffer);
        SAFE_RELEASE(TriGroupings[GroupNum].D3DVertexBuffer);
        SAFE_DELETE(TriGroupings[GroupNum].BoneRemapBuffer);
    }
}

void Mesh::Render(LPDIRECT3DDEVICE9 d3ddev, granny_matrix_4x4* CompositeBuffer)
{
    const int* ToBoneIndices = GrannyGetMeshBindingToBoneIndices(GrannyMeshBinding);

    // Create our vertex shader constants
    float VertexShaderConstants[VSCONSTANT_MAX_CONSTANTS][4];
    VertexShaderConstants[VSCONSTANT_NUM765_NUM1OVER4096_NUM1OVER2_NUM1][0] = 255.0f * 3.0f;
    VertexShaderConstants[VSCONSTANT_NUM765_NUM1OVER4096_NUM1OVER2_NUM1][1] = 1.0f / 4096.0f;
    VertexShaderConstants[VSCONSTANT_NUM765_NUM1OVER4096_NUM1OVER2_NUM1][2] = 1.0f / 2.0f;
    VertexShaderConstants[VSCONSTANT_NUM765_NUM1OVER4096_NUM1OVER2_NUM1][3] = 1.0f;
    VertexShaderConstants[VSCONSTANT_NUM0_NUM2_NUM1_x][0] = 0.0f;
    VertexShaderConstants[VSCONSTANT_NUM0_NUM2_NUM1_x][1] = 2.0f;
    VertexShaderConstants[VSCONSTANT_NUM0_NUM2_NUM1_x][2] = 1.0f;
    VertexShaderConstants[VSCONSTANT_NUM0_NUM2_NUM1_x][3] = 0.0f;

    // This is a hack - I happen to know where this light needs to be!
    VertexShaderConstants[VSCONSTANT_LIGHT_DIRECTION_ALPHA][0] = -1.0f;
    VertexShaderConstants[VSCONSTANT_LIGHT_DIRECTION_ALPHA][1] = 2.0f;
    VertexShaderConstants[VSCONSTANT_LIGHT_DIRECTION_ALPHA][2] = 1.0f;
    D3DXVECTOR3 *Hacky = (D3DXVECTOR3*)&(VertexShaderConstants[VSCONSTANT_LIGHT_DIRECTION_ALPHA][0]);
    D3DXVec3Normalize ( Hacky, Hacky );
    VertexShaderConstants[VSCONSTANT_LIGHT_DIRECTION_ALPHA][3] = 1.0f;
    VertexShaderConstants[VSCONSTANT_LIGHT_COLOUR_AMBIENT][0] = 0.8f;
    VertexShaderConstants[VSCONSTANT_LIGHT_COLOUR_AMBIENT][1] = 0.8f;
    VertexShaderConstants[VSCONSTANT_LIGHT_COLOUR_AMBIENT][2] = 0.8f;
    VertexShaderConstants[VSCONSTANT_LIGHT_COLOUR_AMBIENT][3] = 0.2f;

    // Setup the projection matrix from the current scene
    {
        Scene* TheScene = OwningModel->OwningScene;

        D3DXMATRIX matViewProj;
        D3DXMatrixMultiply(&matViewProj, &(TheScene->MatView), &(TheScene->MatProjection));
        VertexShaderConstants[VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+0][0] = matViewProj._11;
        VertexShaderConstants[VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+0][1] = matViewProj._21;
        VertexShaderConstants[VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+0][2] = matViewProj._31;
        VertexShaderConstants[VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+0][3] = matViewProj._41;
        VertexShaderConstants[VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+1][0] = matViewProj._12;
        VertexShaderConstants[VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+1][1] = matViewProj._22;
        VertexShaderConstants[VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+1][2] = matViewProj._32;
        VertexShaderConstants[VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+1][3] = matViewProj._42;
        VertexShaderConstants[VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+2][0] = matViewProj._13;
        VertexShaderConstants[VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+2][1] = matViewProj._23;
        VertexShaderConstants[VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+2][2] = matViewProj._33;
        VertexShaderConstants[VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+2][3] = matViewProj._43;
        VertexShaderConstants[VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+3][0] = matViewProj._14;
        VertexShaderConstants[VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+3][1] = matViewProj._24;
        VertexShaderConstants[VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+3][2] = matViewProj._34;
        VertexShaderConstants[VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+3][3] = matViewProj._44;
    }

    // Set all constants up to this point
    d3ddev->SetVertexShaderConstantF ( 0, &(VertexShaderConstants[0][0]), VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+4 );


    // texture state
    {
        d3ddev->SetRenderState ( D3DRS_ALPHAREF, 0x80 );
        d3ddev->SetRenderState ( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
        d3ddev->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
        d3ddev->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
        d3ddev->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
        d3ddev->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
        d3ddev->SetSamplerState( 2, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
        d3ddev->SetSamplerState( 2, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    }

    // Shader state
    d3ddev->SetVertexShader(OwningModel->OwningScene->GetAnimatedShader() );
    d3ddev->SetVertexDeclaration(OwningModel->OwningScene->GetAnimatedVertexDeclaration() );

    // Render each grouping...
    for (size_t GroupNum = 0; GroupNum < TriGroupings.size(); GroupNum++)
    {
        const TriGroup& Group = TriGroupings[GroupNum];
        int NumRemappedBones  = (int)Group.BoneRemapBuffer->size();

        for (int BoneNum = 0; BoneNum < NumRemappedBones; BoneNum++)
        {
            const int BoneConstantStart = VSCONSTANT_FIRST_BONE_MATRIX3 + BoneNum * 3;

            granny_matrix_4x4 *OriginalMatrix;
            if ( GrannyMeshIsRigid ( GrannyMesh ) )
            {
                OriginalMatrix = &(CompositeBuffer[ToBoneIndices[0]]);
            }
            else
            {
                int OriginalBoneNum = Group.BoneRemapBuffer->at(BoneNum);
                OriginalMatrix = &(CompositeBuffer[OriginalBoneNum]);
            }

            // Note the transposition, so we can throw away the last row, rather than the last column.
            VertexShaderConstants[BoneConstantStart + 0][0] = (*OriginalMatrix)[0][0];
            VertexShaderConstants[BoneConstantStart + 0][1] = (*OriginalMatrix)[1][0];
            VertexShaderConstants[BoneConstantStart + 0][2] = (*OriginalMatrix)[2][0];
            VertexShaderConstants[BoneConstantStart + 0][3] = (*OriginalMatrix)[3][0];
            VertexShaderConstants[BoneConstantStart + 1][0] = (*OriginalMatrix)[0][1];
            VertexShaderConstants[BoneConstantStart + 1][1] = (*OriginalMatrix)[1][1];
            VertexShaderConstants[BoneConstantStart + 1][2] = (*OriginalMatrix)[2][1];
            VertexShaderConstants[BoneConstantStart + 1][3] = (*OriginalMatrix)[3][1];
            VertexShaderConstants[BoneConstantStart + 2][0] = (*OriginalMatrix)[0][2];
            VertexShaderConstants[BoneConstantStart + 2][1] = (*OriginalMatrix)[1][2];
            VertexShaderConstants[BoneConstantStart + 2][2] = (*OriginalMatrix)[2][2];
            VertexShaderConstants[BoneConstantStart + 2][3] = (*OriginalMatrix)[3][2];
        }

        // Set the bone constants
        d3ddev->SetVertexShaderConstantF(VSCONSTANT_FIRST_BONE_MATRIX3,
                                         &(VertexShaderConstants[VSCONSTANT_FIRST_BONE_MATRIX3][0]),
                                         NumRemappedBones * 3 );

        // Set up the data sources...
        {
            d3ddev->SetIndices(Group.D3DIndexBuffer);
            d3ddev->SetStreamSource(0, Group.D3DVertexBuffer, 0, sizeof(MyVertex));
        }


        // No texture.
        d3ddev->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG2 );
        d3ddev->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        d3ddev->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
        d3ddev->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2 );
        d3ddev->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
        d3ddev->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
        d3ddev->SetRenderState ( D3DRS_ALPHATESTENABLE, FALSE );
        d3ddev->SetTexture ( 0, NULL );

        d3ddev->DrawIndexedPrimitive ( D3DPT_TRIANGLELIST,
                                       0, 0,
                                       Group.NumVertices,
                                       0,
                                       Group.NumTris );
    }
}


// ----------------------------------------------------------------------
//  Texture
// ----------------------------------------------------------------------
Texture::Texture(granny_texture* TheTexture)
  : GrannyTexture(TheTexture),
    D3DTexture(NULL)
{
    ASSERT(GrannyTexture);
}


Texture::~Texture()
{
    //
}


void Texture::CreateD3DObjects(LPDIRECT3DDEVICE9 d3ddev)
{
    // Cubemaps and other curious formats will have more than one image.
    ASSERT ( GrannyTexture->ImageCount == 1 );

    granny_texture_image *GrannyImage = &GrannyTexture->Images[0];
    HRESULT hres = d3ddev->CreateTexture ( GrannyTexture->Width, GrannyTexture->Height,
                                           GrannyImage->MIPLevelCount,
                                           0, D3DFMT_A8R8G8B8,
                                           D3DPOOL_MANAGED, &D3DTexture, NULL );
    hres = hres;
    ASSERT ( SUCCEEDED ( hres ) );

    // Now loop over all the MIP levels and fill them.
    int Width  = GrannyTexture->Width;
    int Height = GrannyTexture->Height;
    for ( int MIPIndex = 0; MIPIndex < GrannyImage->MIPLevelCount; MIPIndex++ )
    {
        D3DLOCKED_RECT LockedRect;
        HRESULT recthres = D3DTexture->LockRect (MIPIndex, &LockedRect, 0, 0 );
        ASSERT ( SUCCEEDED (recthres) );
        recthres = recthres;

        // This call converts to the desired format as well as doing the copy.
        // Note that D3D's notation is the reverse of Granny's - BGRA vs ARGB
        GrannyCopyTextureImage ( GrannyTexture, 0, MIPIndex,
                                 GrannyBGRA8888PixelFormat,
                                 Width, Height,
                                 LockedRect.Pitch,
                                 LockedRect.pBits );

        recthres = D3DTexture->UnlockRect ( MIPIndex );
        ASSERT ( SUCCEEDED (recthres) );

        // Each cycle, the MIP'ing makes the texture half as tall and wide.
        Width  >>= 1;
        Height >>= 1;

        // Unless the size shrinks to 0, in which case it gets clamped (this happens on non-square textures).
        if ( Width == 0 )
            Width = 1;
        if ( Height == 0 )
            Height = 1;
    }
}


void Texture::DestroyD3DObjects(LPDIRECT3DDEVICE9 d3ddev)
{
    // Make sure the device is not still using them.
    d3ddev->SetTexture ( 0, NULL );
    d3ddev->SetTexture ( 1, NULL );
    d3ddev->SetTexture ( 2, NULL );
    SAFE_RELEASE ( D3DTexture );
}


// ---------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------
Animation::Animation(Scene* TheScene,
                     const char* SourceFilename,
                     granny_animation* TheAnimation,
                     granny_track_group* TheTrackGroup)
  : OwningScene(TheScene),
    GrannyAnimation(TheAnimation),
    GrannyTrackGroup(TheTrackGroup)
{
    ASSERT(TheScene);
    ASSERT(TheAnimation);
    ASSERT(TheTrackGroup);
    ASSERT(SourceFilename);

    Filename = new char[strlen(SourceFilename) + 1];
    strcpy(Filename, SourceFilename);
}


Animation::~Animation()
{
    SAFE_DELETE_ARRAY(Filename);
}


// ---------------------------------------------------------------------
// Load and transform a file into the correct space
// ---------------------------------------------------------------------
static bool LoadGrannyFile(const char*        Filename,
                           granny_file**      TheLoadedFile,
                           granny_file_info** TheFileInfo)
{
    ASSERT(Filename);
    ASSERT(TheLoadedFile);
    ASSERT(TheFileInfo);

    // Now open the file.
    *TheLoadedFile = GrannyReadEntireFile ( Filename );
    if ( *TheLoadedFile != NULL )
    {
        // It's a file Granny can load (but might be just a raw bunch of bits).
        *TheFileInfo = GrannyGetFileInfo ( *TheLoadedFile );

        if ( *TheFileInfo != NULL )
        {
            // And it has an actual Granny structure to it
            // and will have been updated it to the latest version/format.

            // Let's change to the coordinate system we expect.  Note that we can
            //  only do this if the arttool info is present.  Otherwise, we must
            //  assume that the file has been pretransformed into our coordinate
            //  system...
            if ((*TheFileInfo)->ArtToolInfo != NULL)
            {
                granny_triple Origin =      {0, 0, 0};
                granny_triple RightVector = {1, 0, 0};
                granny_triple UpVector =    {0, 1, 0};
                granny_triple BackVector =  {0, 0, 1};

                granny_real32 UnitsPerMeter = 1.0f;

                // Tell Granny to construct the transform from the file's coordinate
                // system to our coordinate system
                granny_triple Affine3;
                granny_matrix_3x3 Linear3x3;
                granny_matrix_3x3 InverseLinear3x3;
                GrannyComputeBasisConversion (
                    *TheFileInfo, UnitsPerMeter,
                    Origin, RightVector, UpVector, BackVector,
                    Affine3, (granny_real32*)Linear3x3, (granny_real32*)InverseLinear3x3);

                // Tell Granny to transform the file into our coordinate system
                GrannyTransformFile ( *TheFileInfo, Affine3, (granny_real32*)Linear3x3, (granny_real32*)InverseLinear3x3,
                                      0.0001f, 0.0001f, GrannyRenormalizeNormals | GrannyReorderTriangleIndices );
            }

            return true;
        }

        // Failed.
        GrannyFreeFile ( *TheLoadedFile );
        *TheLoadedFile = NULL;
    }

    return false;
}


static bool CreateSkinnedShader(LPDIRECT3DDEVICE9 d3ddev,
                                LPDIRECT3DVERTEXSHADER9* ppShader)
{
    ASSERT(ppShader  != NULL);
    ASSERT(*ppShader == NULL);


    // Compile the vertex shader(s).
    ShaderAssembler sa;

    // Mapping from world matrix row to the accumulated register
    const char* WorldMatrixRegister[3] = { "r1", "r2", "r3" };


    // 1.1 for max compatibility
    sa.AddLine("vs_1_1");

    // Vertex components
    sa.AddLine("dcl_position     v0");
    sa.AddLine("dcl_blendweight  v1");
    sa.AddLine("dcl_blendindices v2");
    sa.AddLine("dcl_normal       v3");
    sa.AddLine("dcl_texcoord0    v4");

    // Handle the position.
    // Inputs:
    //  v0: position
    //  v1/v2: weights/indices (d3dcolors)
    // Outputs:
    //  r1-3: Weighted world matrix
    //  oPos: final position
    {
        // Scale up the bone indices, which we've downloaded as a d3d
        // color for compatibility.  These are 0-1, we need to scale
        // them to 0-255, and then by 3 to select the proper matrix,
        // which is stored 4x3 in 3 constant registers.  Note that we
        // add 0.5 to that to ensure proper rounding...
        sa.AddLine("mad r0.xyzw, c%i.x, v2, c%i.z", VSCONSTANT_NUM765_NUM1OVER4096_NUM1OVER2_NUM1, VSCONSTANT_NUM765_NUM1OVER4096_NUM1OVER2_NUM1);

        // Do the matrix accumulation.
        for (int BoneNum = 0; BoneNum < 4; BoneNum++)
        {
            // Note that this swizzle accounts for the fact that when
            // downloaded as colors, the components get changed from
            // "rgba" to "argb".
            char BoneComponent = "wxyz"[BoneNum];

            sa.AddLine("mov a0.x, r0.%c", BoneComponent);
            if ( BoneNum == 0 )
            {
                // Only the first bone is mul, everything else accumulates
                sa.AddLine("mul r1, v1.%c, c%i[a0.x]    ", BoneComponent, VSCONSTANT_FIRST_BONE_MATRIX3+0);
                sa.AddLine("mul r2, v1.%c, c%i[a0.x]    ", BoneComponent, VSCONSTANT_FIRST_BONE_MATRIX3+1);
                sa.AddLine("mul r3, v1.%c, c%i[a0.x]    ", BoneComponent, VSCONSTANT_FIRST_BONE_MATRIX3+2);
            }
            else
            {
                sa.AddLine("mad r1, v1.%c, c%i[a0.x], r1", BoneComponent, VSCONSTANT_FIRST_BONE_MATRIX3+0);
                sa.AddLine("mad r2, v1.%c, c%i[a0.x], r2", BoneComponent, VSCONSTANT_FIRST_BONE_MATRIX3+1);
                sa.AddLine("mad r3, v1.%c, c%i[a0.x], r3", BoneComponent, VSCONSTANT_FIRST_BONE_MATRIX3+2);
            }
        }

        // Transform the position by the blended matrix to get to world-space.
        sa.AddLine("dp4 r4.x, v0, %s", WorldMatrixRegister[0]);
        sa.AddLine("dp4 r4.y, v0, %s", WorldMatrixRegister[1]);
        sa.AddLine("dp4 r4.z, v0, %s", WorldMatrixRegister[2]);
        sa.AddLine("mov r4.w, c%i.w", VSCONSTANT_NUM765_NUM1OVER4096_NUM1OVER2_NUM1);

        // Then transform the world-space position into projection space.
        sa.AddLine("dp4 oPos.x, r4, c%i", VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+0);
        sa.AddLine("dp4 oPos.y, r4, c%i", VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+1);
        sa.AddLine("dp4 oPos.z, r4, c%i", VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+2);
        sa.AddLine("dp4 oPos.w, r4, c%i", VSCONSTANT_FIRST_VIEWPROJ_MATRIX4+3);
    }

    // Handle the normal
    //  Inputs:
    //   v1: Normal
    //   r1-3: Blended World matrix
    //  Outputs:
    //   r0: scaled normal
    //   r5: transformed normal
    //   oD0: lighting
    {
        // Transform the normal by the blended matrix
        // (technically we should use the inverse transpose, so this is incorrect for shears or non-uniform scales).
        sa.AddLine("dp3 r5.x, v3, %s", WorldMatrixRegister[0]);
        sa.AddLine("dp3 r5.y, v3, %s", WorldMatrixRegister[1]);
        sa.AddLine("dp3 r5.z, v3, %s", WorldMatrixRegister[2]);
        // Can't use nrm in VS1.1, so we do it manually.
        sa.AddLine("dp3 r5.w, r5, r5");
        sa.AddLine("rsq r5.w, r5.w");
        sa.AddLine("mul r5.xyz, r5, r5.w");

        // Do some simple lighting.
        // clamp(N.L)
        sa.AddLine ( "dp3 r5.w, r5, c%i", VSCONSTANT_LIGHT_DIRECTION_ALPHA );
        sa.AddLine ( "max r5.w, r5.w, c%i.x", VSCONSTANT_NUM0_NUM2_NUM1_x );

        // Add in the light's ambient.
        sa.AddLine ( "add r5.w, r5.w, c%i.w", VSCONSTANT_LIGHT_COLOUR_AMBIENT );

        // And finally multiply by the actual colour of the light.
        sa.AddLine ( "mul oD0.xyz, r5.w, c%i", VSCONSTANT_LIGHT_COLOUR_AMBIENT );
        sa.AddLine ( "mov oD0.w, c%i.w", VSCONSTANT_LIGHT_DIRECTION_ALPHA );
    }



    ID3DXBuffer *d3dxbufferShader = NULL;
    ID3DXBuffer *d3dxbufferErrors = NULL;

    const char *ShaderText = sa.GetShaderText();
    HRESULT hres = D3DXAssembleShader(ShaderText, (UINT)strlen(ShaderText), NULL, NULL, 0, &d3dxbufferShader, &d3dxbufferErrors);

    if( FAILED ( hres ) )
    {
        OutputDebugString("// Shader text\n");
        OutputDebugString(ShaderText);
        if( d3dxbufferErrors != NULL )
            OutputDebugString ( (char*)d3dxbufferErrors->GetBufferPointer() );

        FAIL("Couldn't assemble shader");
        return false;
    }

    hres = d3ddev->CreateVertexShader((DWORD*)d3dxbufferShader->GetBufferPointer(), ppShader);
    ASSERT ( SUCCEEDED ( hres ) );

    SAFE_RELEASE(d3dxbufferErrors);
    SAFE_RELEASE(d3dxbufferShader);

    return SUCCEEDED(hres);
}


void Mesh::TriGroup::CreateD3DObjects(LPDIRECT3DDEVICE9 d3ddev,
                                      Mesh* TheMesh)
{
    granny_mesh* TheGrannyMesh = TheMesh->GrannyMesh;

    // Let's get our indices in a standard 4-byte format
    std::vector<DWORD> OriginalIndices(NumTris * 3, (DWORD)-1);
    {
        void* MeshIndices   = GrannyGetMeshIndices(TheGrannyMesh);
        const int BytesPer  = GrannyGetMeshBytesPerIndex(TheGrannyMesh);
        void* StartIndexPtr = ((granny_uint8*)MeshIndices) + (BytesPer * FirstTri * 3);

        GrannyConvertIndices((int)OriginalIndices.size(),
                             BytesPer, StartIndexPtr,
                             (int)sizeof(DWORD), &OriginalIndices[0]);
    }

    // Extract the vertices. Granny will convert on the way.
    const int OriginalNumVertices = GrannyGetMeshVertexCount(TheGrannyMesh);
    std::vector<MyVertex> OriginalVertices(OriginalNumVertices);
    {
        // Use the standard granny remap call to move to our vert format
        GrannyConvertVertexLayouts(OriginalNumVertices,
                                   TheGrannyMesh->PrimaryVertexData->VertexType, TheGrannyMesh->PrimaryVertexData->Vertices,
                                   MyVertexType, &OriginalVertices[0]);
    }

    for (int i = 0; i < OriginalNumVertices; ++i)
    {
        MyVertex& Vert = OriginalVertices[i];

        // Reset the verts, they're binormals...
        Vert.Normal[0] = 2.0f * (Vert.Normal[0] - 0.5f);
        Vert.Normal[1] = 2.0f * (Vert.Normal[1] - 0.5f);
        Vert.Normal[2] = 2.0f * (Vert.Normal[2] - 0.5f);
    }


    // That converted ALL the vertices in the mesh, but this group might not use them all.
    // So scan the tris and find which verts we do use. This also happens to reorder
    // vertices in the order in which they are used, which helps things like AGP
    // transfer efficiency and stuff like that.
    NumVertices = 0;
    std::vector<int> OriginalToRealVertexMap(OriginalNumVertices, -1);
    std::vector<int> RealToOriginalVertexMap;
    RealToOriginalVertexMap.reserve(OriginalNumVertices);

    for ( int IndexNum = 0; IndexNum < NumTris * 3; IndexNum++ )
    {
        DWORD Index = OriginalIndices[IndexNum];
        ASSERT ( Index < DWORD(OriginalNumVertices) );
        if ( OriginalToRealVertexMap[Index] == -1 )
        {
            // Add this vertex.
            OriginalToRealVertexMap[Index] = NumVertices;
            RealToOriginalVertexMap.push_back(Index);
            NumVertices++;
        }
    }

    // Now find a mapping from "global" mesh bone indices to the indices of
    // data we'll be putting into the VS constants.
    // Remap the indices in OptimisedVertices on the way.
    // Also, find the mapping from the parts of a granny_world_pose to
    // VS bone indices (this is not the same thing!)
    ASSERT(BoneRemapBuffer == NULL);
    BoneRemapBuffer = new std::vector<int>;
    std::vector<int> RemapVertexToMeshBone;

    const int *ToBoneIndices = GrannyGetMeshBindingToBoneIndices( TheMesh->GrannyMeshBinding );
    for (int VertNum = 0; VertNum < NumVertices; VertNum++)
    {
        const int OriginalIndex = RealToOriginalVertexMap[VertNum];
        ASSERT(OriginalIndex >= 0 && OriginalIndex < OriginalNumVertices);

        MyVertex& CurVertex = OriginalVertices[OriginalIndex];

        int TotalWeight = 0;
        for ( int i = 0; i < 4; i++ )
        {
            TotalWeight += CurVertex.BoneWeights[i];
            if ( CurVertex.BoneWeights[i] == 0 )
            {
                // Zero weight - make sure the index doesn't blow up by being out of range.
                CurVertex.BoneIndices[i] = 0;
            }
            else
            {
                int OriginalBoneIndex = CurVertex.BoneIndices[i];

                std::vector<int>::const_iterator itr = std::find(RemapVertexToMeshBone.begin(),
                                                                 RemapVertexToMeshBone.end(),
                                                                 (int)OriginalBoneIndex);
                int NewBoneIndex = int(itr - RemapVertexToMeshBone.begin());
                if ( NewBoneIndex == int(RemapVertexToMeshBone.size()) )
                {
                    // Not found, so add it.
                    NewBoneIndex = (int)BoneRemapBuffer->size();
                    RemapVertexToMeshBone.push_back(OriginalBoneIndex);
                    BoneRemapBuffer->push_back(ToBoneIndices[OriginalBoneIndex]);
                    //ASSERT ( BoneRemapBuffer->size() <= VSCONSTANT_MAX_BONE_COUNT );
                }

                ASSERT(NewBoneIndex < 255);
                CurVertex.BoneIndices[i] = BYTE(NewBoneIndex);
            }
        }
        ASSERT ( TotalWeight == 255 );
    }


    // Copy our verts into a VB.  Note that we're going to make sure
    //  that our vertex shader does all the xforming, so we'll only
    //  be copying into this once...
    {
        HRESULT hres =
            d3ddev->CreateVertexBuffer(NumVertices * sizeof(MyVertex),
                                       D3DUSAGE_WRITEONLY,
                                       0,
                                       D3DPOOL_DEFAULT,
                                       &D3DVertexBuffer,
                                       NULL );
        ASSERT ( SUCCEEDED ( hres ) );

        MyVertex *TempVertex;
        hres = D3DVertexBuffer->Lock ( 0, 0, (void**)&TempVertex, 0 );
        ASSERT ( SUCCEEDED ( hres ) );

        for (size_t VertNum = 0; VertNum < RealToOriginalVertexMap.size(); VertNum++)
        {
            const int RealIndex = RealToOriginalVertexMap[VertNum];
            ASSERT(RealIndex >= 0 && RealIndex < OriginalNumVertices);

            memcpy(TempVertex + VertNum, &OriginalVertices[RealIndex], sizeof(MyVertex));
        }

        hres = D3DVertexBuffer->Unlock();
        ASSERT ( SUCCEEDED ( hres ) );
    }

    // Dump the indices into the index buffer
    {
        HRESULT hres =
            d3ddev->CreateIndexBuffer (NumTris * 3 * sizeof(DWORD),
                                       D3DUSAGE_WRITEONLY,
                                       D3DFMT_INDEX32,
                                       D3DPOOL_DEFAULT,
                                       &D3DIndexBuffer,
                                       NULL );
        ASSERT ( SUCCEEDED ( hres ) );

        DWORD* IBIndices;
        hres = D3DIndexBuffer->Lock ( 0, 0, (void**)&IBIndices, 0 );
        ASSERT ( SUCCEEDED ( hres ) );

        // And copy the indices for the group in, remapping from original indices to real ones.
        for ( int IndexNum = 0; IndexNum < ( NumTris * 3 ); IndexNum++ )
        {
            DWORD OriginalIndex = OriginalIndices[IndexNum];
            int   RealIndex     = OriginalToRealVertexMap[OriginalIndex];
            ASSERT ( ( RealIndex >= 0 ) && ( RealIndex < NumVertices ) );
            *IBIndices++ = (DWORD)RealIndex;
        }

        hres = D3DIndexBuffer->Unlock();
        ASSERT ( SUCCEEDED ( hres ) );
    }

}
