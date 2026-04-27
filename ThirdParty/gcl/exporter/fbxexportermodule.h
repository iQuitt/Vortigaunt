#pragma once

#include "gcl/bindings/scene.h"
#include "gcl/importer/grannyformat.h"

#include <fbxsdk.h>

namespace GCL::Exporter {

using namespace std;
using namespace GCL::Bindings;

///
/// \brief The ExporterModule class.
///
class FbxExporterModule {
public:
    ///
    /// \brief ExporterModule
    /// \param scene Scene which needs to be exported.
    /// \param fbxScene Fbx scene which has to be used for the export.
    ///
    FbxExporterModule(Scene::SharedPtr scene, FbxScene* fbxScene);

    ///
    /// \brief Destructor
    ///
    ~FbxExporterModule();

protected:
    ///
    /// \brief Scene of the importing granny file.
    ///
    Scene::SharedPtr m_scene;

    ///
    /// \brief Fbx scene for the export.
    ///
    FbxScene* m_fbxScene = nullptr;
};

} // namespace GCL::Exporter
