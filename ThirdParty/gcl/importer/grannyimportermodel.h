#pragma once

#include "gcl/bindings/bone.h"
#include "gcl/bindings/mesh.h"
#include "gcl/bindings/scene.h"
#include "gcl/importer/grannyformat.h"

#include <vector>

namespace GCL::Importer {

using namespace std;
using namespace GCL::Bindings;

///
/// \brief The GrannyImporterModel class.
///
class GrannyImporterModel {
public:
    ///
    /// \brief Constructor
    /// \param scene Scene which needs to be exported.
    ///
    GrannyImporterModel(Scene::SharedPtr scene);

    ///
    /// \brief Destructor
    ///
    ~GrannyImporterModel();

    ///
    /// \brief Imports a model and all its meshes from a granny file to the scene.
    /// \param granny_file_info Granny file info
    ///
    void importModels(granny_file_info* granny_file_info) const;

protected:
    ///
    /// \brief Imports a granny model as scene model.
    /// \param granny_model Granny model.
    /// \return Model of the granny model.
    ///
    Model::SharedPtr importModel(granny_model* granny_model) const;

    ///
    /// \brief Import all meshes from a granny model as scene meshes.
    /// \param granny_model Granny model
    /// \return All meshes of the granny model.
    ///
    vector<Mesh::SharedPtr> importMeshes(granny_model* granny_model) const;

    ///
    /// \brief Imports a granny mesh as scene mesh.
    /// \param granny_mesh Granny mesh
    /// \return granny_mesh Mesh of the granny mesh.
    ///
    Mesh::SharedPtr importMesh(granny_mesh* granny_mesh) const;

protected:
    ///
    /// \brief Scene of the importing granny file.
    ///
    Scene::SharedPtr m_scene;
};

} // namespace GCL::Importer
