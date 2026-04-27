#include "gcl/bindings/model.h"

namespace GCL::Bindings {

Model::Model(granny_model* data)
    : m_data(data)
{
}

granny_model* Model::getData()
{
    return m_data;
}

vector<Mesh::SharedPtr> Model::getMeshes()
{
    return m_meshes;
}

vector<Bone::SharedPtr> Model::getBones()
{
    return m_bones;
}

void Model::setMeshes(vector<Mesh::SharedPtr> meshes)
{
    m_meshes.swap(meshes);
}

void Model::setBones(vector<Bone::SharedPtr> bones)
{
    m_bones = bones;
}

bool Model::hasRigidMeshes()
{
    for (auto mesh : m_meshes) {
        if (mesh->isRigid()) {
            return true;
        }
    }

    return false;
}

void Model::setTransform(FbxMatrix transform)
{
    m_transform = transform;
}

} // namespace GCL::Bindings
