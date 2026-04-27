#if !defined(OPENGL_RENDERING_H)
// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "granny.h"
#include <stdio.h>
#include <assert.h>
#include <vector>

#if defined(_MACOSX) || defined(linux)
  #include <GL/glew.h>
  #include <GL/glut.h>
#else
  #include "glew.h"
  #include "glut.h"

  #pragma warning(disable : 4505)
#endif


struct DemoTexture
{
    char* Name;
    GLuint TextureName;

    DemoTexture();
    ~DemoTexture();
};

struct DemoMesh
{
    granny_mesh* Mesh;
    granny_mesh_binding* MeshBinding;
    std::vector<DemoTexture*> MaterialBindings;

    GLuint VertexBufferObject;
    GLuint IndexBufferObject;

    DemoMesh();
    ~DemoMesh();
};

struct DemoModel
{
    granny_model_instance* ModelInstance;
    std::vector<DemoMesh*>      BoundMeshes;

    granny_real32 InitialMatrix[16];

    DemoModel();
    ~DemoModel();
};

struct DemoScene
{
    granny_camera DemoCamera;

    granny_file      *SceneFile;
    granny_file_info *SceneFileInfo;

    std::vector<DemoTexture*> Textures;
    std::vector<DemoModel*>   Models;

    // For the purposes of this sample, we'll be sampling the model
    // animation immediately before rendering, which allows us to share
    // the local pose across all models.
    granny_int32x      MaxBoneCount;
    granny_local_pose* SharedLocalPose;
    granny_world_pose* SharedWorldPose;

    // Simple directional light
    float DirFromLight[4];
    float LightColor[4];
    float AmbientColor[4];

    DemoScene();
    ~DemoScene();
};

// ---------------------------------------------------------
// Handy functions.
bool InitializeOpenGL();
void CleanupOpenGL();

void Render();
void ProcessNormalKeys(unsigned char key, int /*x*/, int /*y*/);

#define OPENGL_RENDERING_H
#endif
