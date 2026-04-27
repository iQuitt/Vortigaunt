// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "opengl_rendering.h"

#define CHECKED_RELEASE(x) { if (x) { (x)->Release(); } (x) = NULL; } 0

DemoTexture::DemoTexture()
  : Name(NULL), TextureName(0)
{

}

DemoTexture::~DemoTexture()
{
    if (TextureName != 0)
    {
        glDeleteTextures(1, &TextureName);
        TextureName = 0;
    }

    delete [] Name;
    Name = NULL;
}

DemoMesh::DemoMesh()
  : Mesh(NULL),
    MeshBinding(NULL),
    VertexBufferObject(0),
    IndexBufferObject(0)
{
    //
}

DemoMesh::~DemoMesh()
{
    glDeleteBuffers(1, &VertexBufferObject);
    glDeleteBuffers(1, &IndexBufferObject);
    VertexBufferObject = 0;
    IndexBufferObject = 0;

    // We don't own these
    Mesh = NULL;
    MaterialBindings.clear();

    // We do own these
    if (MeshBinding)
    {
        GrannyFreeMeshBinding(MeshBinding);
        MeshBinding = NULL;
    }

}

DemoModel::DemoModel()
  : ModelInstance(NULL)
{

}

DemoModel::~DemoModel()
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

DemoScene::DemoScene()
  : SceneFile(NULL),
    SceneFileInfo(NULL),
    MaxBoneCount(-1),
    SharedLocalPose(NULL),
    SharedWorldPose(NULL)
{
    // Nada
}

DemoScene::~DemoScene()
{
    {for(granny_uint32x i = 0; i < Textures.size(); ++i)
    {
        delete Textures[i];
        Textures[i] = NULL;
    }}

    {for(granny_uint32x i = 0; i < Models.size(); ++i)
    {
        delete Models[i];
        Models[i] = NULL;
    }}

    GrannyFreeLocalPose(SharedLocalPose);
    GrannyFreeWorldPose(SharedWorldPose);
    SharedLocalPose = NULL;
    SharedWorldPose = NULL;

    GrannyFreeFile(SceneFile);
    SceneFile = NULL;
    SceneFileInfo = NULL;
}
