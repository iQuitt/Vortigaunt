#pragma once

#include "gcl/exporter/fbxexporteranimation.h"
#include "gcl/exporter/fbxexportermaterial.h"
#include "gcl/exporter/fbxexportermesh.h"
#include "gcl/exporter/fbxexportermodulefactory.h"
#include "gcl/exporter/fbxexporterskeleton.h"
#include "gcl/exporter/fbxexportoptions.h"

namespace GCL::Exporter {

using namespace std;
using namespace GCL::Bindings;

///
/// \brief Granny exporter - exports a scene to a filmbox file.
///
class FbxExporter {
public:
    ///
    /// \brief Constructer with data initialization.
    /// \param scene A scene to be exported.
    ///
    FbxExporter(Scene::SharedPtr scene);

    ///
    /// \brief Constructer with extended options and data initialization.
    /// \param options Export options to define what aspects of the scene needs to be exported.
    /// \param scene A scene to be exported.
    ///
    FbxExporter(FbxExportOptions options, Scene::SharedPtr scene);

    ///
    /// \brief Constructer with extended options and data initialization.
    /// \param exporterModuleFactory Factory to create exporter modules.
    /// \param options Export options to define what aspects of the scene needs to be exported.
    /// \param scene A scene to be exported.
    ///
    FbxExporter(ExporterModuleFactoryInterface* exporterModuleFactory, FbxExportOptions options, Scene::SharedPtr scene);

    ///
    /// \brief Destructor
    ///
    ~FbxExporter();

    ///
    /// \brief Initializes the exporter.
    ///
    void initialize();

    ///
    /// \brief Export the scene to a filmbox file.
    /// \param outputFilepath
    ///
    void exportToFile(string outputFilepath);

    ///
    /// \brief Export the models of the scene to the fbx scene.
    /// \param outputFilepath Output path where the fbx file will be exported to. It is required for material export.
    ///
    void exportModels(string outputFilepath);

    ///
    /// \brief Returns a scene.
    /// \return Scene
    ///
    Scene::SharedPtr getScene()
    {
        return m_scene;
    }

    ///
    /// \brief Returns fbx scene.
    /// \return Fbx scene
    ///
    FbxScene* getFbxScene()
    {
        return m_fbxScene;
    }

protected:
    ///
    /// \brief Factory to create exporter modules.
    ///
    ExporterModuleFactoryInterface* m_exporterModuleFactory = nullptr;

    ///
    /// \brief Export options which define what aspects of the scene needs to be exported.
    ///
    FbxExportOptions m_options;

    ///
    /// \brief Material exporter module.
    ///
    FbxExporterMaterial* m_exporterMaterial = nullptr;

    ///
    /// \brief Mesh exporter module.
    ///
    FbxExporterMesh* m_exporterMesh = nullptr;

    ///
    /// \brief Skeleton exporter module.
    ///
    FbxExporterSkeleton* m_exporterSkeleton = nullptr;

    ///
    /// \brief Animation exporter module.
    ///
    FbxExporterAnimation* m_exporterAnimation = nullptr;

    ///
    /// \brief Scene of the importing granny file.
    ///
    Scene::SharedPtr m_scene;

    ///
    /// \brief Fbx manager for the export by the fbx sdk.
    ///
    FbxManager* m_fbxManager = nullptr;

    ///
    /// \brief Fbx scene for the export by the fbx sdk.
    ///
    FbxScene* m_fbxScene = nullptr;
};

} // namespace GCL::Exporter
