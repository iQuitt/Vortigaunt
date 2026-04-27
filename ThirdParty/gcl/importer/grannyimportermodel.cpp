#include "gcl/importer/grannyimportermodel.h"



namespace GCL::Importer {

GrannyImporterModel::GrannyImporterModel(Scene::SharedPtr scene)
    : m_scene(scene)
{
}

GrannyImporterModel::~GrannyImporterModel()
{
}

void GrannyImporterModel::importModels(granny_file_info* granny_file_info) const
{
    // Import each model of the granny model as scene model.
    for (unsigned i = 0; i < static_cast<unsigned>(granny_file_info->ModelCount); i++) {
        m_scene->addModel(importModel(granny_file_info->Models[i]));
    }
}

Model::SharedPtr GrannyImporterModel::importModel(granny_model* granny_model) const
{
    printf("Import granny model (name: \"%s\") as scene model.\n", granny_model->Name);

    const Model::SharedPtr model = make_shared<Model>(granny_model);

    // Use granny method to translate initial placement in scene correctly.
    FbxMatrix transform;
    GrannyBuildCompositeTransform4x4(&granny_model->InitialPlacement, reinterpret_cast<float*>(&transform));

    model->setTransform(transform);
    model->setMeshes(importMeshes(granny_model));

    return model;
}

vector<Mesh::SharedPtr> GrannyImporterModel::importMeshes(granny_model* granny_model) const
{
    unsigned meshBindingCount = static_cast<unsigned>(granny_model->MeshBindingCount);

    // Create scene meshes of each mesh of the granny model.
    vector<Mesh::SharedPtr> meshes;
    meshes.reserve(meshBindingCount);

    // Import each mesh of the granny model as scene mesh.
    for (unsigned i = 0; i < meshBindingCount; i++) {
        meshes.push_back(make_shared<Mesh>(granny_model->MeshBindings[i].Mesh));
    }

    return meshes;
}

Mesh::SharedPtr GrannyImporterModel::importMesh(granny_mesh* granny_mesh) const
{
    return make_shared<Mesh>(granny_mesh);
}

} // namespace GCL::Importer
