#pragma once

#include "gcl/bindings/binding.h"
#include "gcl/bindings/bone.h"
#include "gcl/bindings/mesh.h"
#include "gcl/importer/grannyformat.h"

#include <fbxsdk.h>

#include <vector>

namespace GCL::Bindings {

using namespace std;

///
/// \brief Binding of granny model data and the counterparts data like meshes and bones.
///
class Model : public Binding<Model> {
public:
    ///
    /// \brief Constructer
    /// \param data Granny data of the model.
    ///
    Model(granny_model* data);

    ///
    /// \brief Returns the granny model data.
    /// \return Granny model data
    ///
    granny_model* getData();

    ///
    /// \brief Returns all meshes of the model.
    /// \return Meshes
    ///
    vector<Mesh::SharedPtr> getMeshes();

    ///
    /// \brief Returns all bones of the model.
    /// \return Model bones
    ///
    vector<Bone::SharedPtr> getBones();

    ///
    /// \brief Append meshes to the scene.
    /// \param Meshes
    ///
    void setMeshes(vector<Mesh::SharedPtr> meshes);

    ///
    /// \brief Append bones to the scene.
    /// \param Model bones
    ///
    void setBones(vector<Bone::SharedPtr> bones);

    ///
    /// \brief Returns if model has rigid body meshes.
    /// \return Returns whether model has at least one rigid meshes or not.
    ///
    bool hasRigidMeshes();

    ///
    /// \brief Set transform of the model.
    /// \param Model transform
    ///
    void setTransform(FbxMatrix transform);

protected:
    ///
    /// \brief Granny data of the model.
    ///
    granny_model* m_data = nullptr;

    ///
    /// \brief Meshes of the model.
    ///
    vector<Mesh::SharedPtr> m_meshes;

    ///
    /// \brief Bones of the model.
    ///
    vector<Bone::SharedPtr> m_bones;

    ///
    /// \brief Transform of the model.
    ///
    FbxMatrix m_transform;
};

} // namespace GCL::Bindings
