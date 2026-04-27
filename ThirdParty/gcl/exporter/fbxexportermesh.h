#pragma once

#include "gcl/exporter/fbxexportermodule.h"
#include "gcl/utilities/fbxsdkcommon.h"
#include "gcl/utilities/materialutility.h"

namespace GCL::Exporter {

using namespace std;
using namespace GCL::Bindings;
using namespace GCL::Utilities;

///
/// \brief The FbxExporterMesh class.
///
class FbxExporterMesh : public FbxExporterModule {
public:
    // Inherit constructor.
    using FbxExporterModule::FbxExporterModule;

    ///
    /// \brief Destructor
    ///
    virtual ~FbxExporterMesh() { }

    ///
    /// \brief Export the meshes of the given model to the fbx scene.
    /// \param model Model of which the meshes need to be exported of to the fbx scene.
    /// \param exportSkeleton
    ///
    void exportMeshes(Model::SharedPtr model, bool exportSkeleton = false);

protected:
    ///
    /// \brief Export the meshes of the given model to the fbx scene.
    /// \param model A model the mesh is related to.
    /// \param mesh The mesh which needs to be exported.
    /// \param exportSkeleton
    ///
    void exportMesh(Model::SharedPtr model, Mesh::SharedPtr mesh, bool exportSkeleton = false);

    ///
    /// \brief Export a mesh to a fbx mesh.
    /// \param mesh The mesh which needs to be exported as fbx mesh.
    /// \return Fbx mesh variant of mesh which needed to be exported as fbx mehs.
    ///
    FbxMesh* exportFbxMesh(Mesh::SharedPtr mesh);

    ///
    /// \brief Sanitizes a material name.
    /// \param name
    /// \return Sanitized name of the material.
    ///
    virtual string sanitizeMaterialName(string name);
};

} // namespace GCL::Exporter
