#pragma once

#include "gcl/bindings/scene.h"
#include "gcl/exporter/fbxexporteranimation.h"
#include "gcl/exporter/fbxexportermaterial.h"
#include "gcl/exporter/fbxexportermesh.h"
#include "gcl/exporter/fbxexporterskeleton.h"

#include "fbxsdk.h"

namespace GCL::Exporter {

using namespace std;
using namespace GCL::Bindings;

///
/// \brief The ExporterModuleFactoryInterface interface.
///
class ExporterModuleFactoryInterface {
public:
    ///
    /// \brief Destructor
    ///
    virtual ~ExporterModuleFactoryInterface() { }

    ///
    /// \brief Returns material exporter module.
    /// \param scene Scene which needs to be exported.
    /// \param fbxScene Fbx scene which has to be used for the export.
    /// \return Exporter module for materials.
    ///
    virtual FbxExporterMaterial* createExporterModuleMaterial(Scene::SharedPtr scene, FbxScene* fbxScene) = 0;

    ///
    /// \brief Returns mesh exporter module.
    /// \param scene Scene which needs to be exported.
    /// \param fbxScene Fbx scene which has to be used for the export.
    /// \return Exporter module for meshes.
    ///
    virtual FbxExporterMesh* createExporterModuleMesh(Scene::SharedPtr scene, FbxScene* fbxScene) = 0;

    ///
    /// \brief Returns skeleton exporter module.
    /// \param scene Scene which needs to be exported.
    /// \param fbxScene Fbx scene which has to be used for the export.
    /// \return Exporter module for skeletons.
    ///
    virtual FbxExporterSkeleton* createExporterModuleSkeleton(Scene::SharedPtr scene, FbxScene* fbxScene) = 0;

    ///
    /// \brief Returns animation exporter module.
    /// \param scene Scene which needs to be exported.
    /// \param fbxScene Fbx scene which has to be used for the export.
    /// \return Exporter module for animations.
    ///
    virtual FbxExporterAnimation* createExporterModuleAnimation(Scene::SharedPtr scene, FbxScene* fbxScene) = 0;
};

///
/// \brief The ExporterModuleFactory class.
///
class FbxExporterModuleFactory : public ExporterModuleFactoryInterface {
public:
    ///
    /// \brief Returns material exporter module.
    /// \param scene Scene which needs to be exported.
    /// \param fbxScene Fbx scene which has to be used for the export.
    /// \return Exporter module for materials.
    ///
    FbxExporterMaterial* createExporterModuleMaterial(Scene::SharedPtr scene, FbxScene* fbxScene) override;

    ///
    /// \brief Returns mesh exporter module.
    /// \param scene Scene which needs to be exported.
    /// \param fbxScene Fbx scene which has to be used for the export.
    /// \return Exporter module for meshes.
    ///
    FbxExporterMesh* createExporterModuleMesh(Scene::SharedPtr scene, FbxScene* fbxScene) override;

    ///
    /// \brief Returns skeleton exporter module.
    /// \param scene Scene which needs to be exported.
    /// \param fbxScene Fbx scene which has to be used for the export.
    /// \return Exporter module for skeletons.
    ///
    FbxExporterSkeleton* createExporterModuleSkeleton(Scene::SharedPtr scene, FbxScene* fbxScene) override;

    ///
    /// \brief Returns animation exporter module.
    /// \param scene Scene which needs to be exported.
    /// \param fbxScene Fbx scene which has to be used for the export.
    /// \return Exporter module for animations.
    ///
    FbxExporterAnimation* createExporterModuleAnimation(Scene::SharedPtr scene, FbxScene* fbxScene) override;
};

} // namespace GCL::Exporter
