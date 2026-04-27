#include "gcl/exporter/fbxexporter.h"

#include "gcl/utilities/fbxsdkcommon.h"

namespace GCL::Exporter {

using namespace GCL::Utilities;

FbxExporter::FbxExporter(Scene::SharedPtr scene)
    : m_scene(scene)
{
    initialize();
}

FbxExporter::FbxExporter(FbxExportOptions options, Scene::SharedPtr scene)
    : m_options(options)
    , m_scene(scene)
{
    initialize();
}

FbxExporter::FbxExporter(ExporterModuleFactoryInterface* exportModuleFactory, FbxExportOptions options, Scene::SharedPtr scene)
    : m_options(options)
    , m_scene(scene)
{
    m_exporterModuleFactory = exportModuleFactory;
    initialize();
}

FbxExporter::~FbxExporter()
{
    FbxSdkCommon::DestroySdkObjects(m_fbxManager);

    if (m_exporterModuleFactory) {
        delete m_exporterModuleFactory;
    }

    delete m_exporterMaterial;
    delete m_exporterSkeleton;
    delete m_exporterAnimation;
}

void FbxExporter::initialize()
{
    // Initialize the fbx sdk.
    FbxSdkCommon::InitializeSdkObjects(m_fbxManager, m_fbxScene);

    // Convert the fbx scene to z-up.
    FbxAxisSystem::Max.ConvertScene(m_fbxScene);

    if (!m_exporterModuleFactory) {
        m_exporterModuleFactory = new FbxExporterModuleFactory();
    }

    m_exporterMaterial = m_exporterModuleFactory->createExporterModuleMaterial(m_scene, m_fbxScene);
    m_exporterMesh = m_exporterModuleFactory->createExporterModuleMesh(m_scene, m_fbxScene);
    m_exporterSkeleton = m_exporterModuleFactory->createExporterModuleSkeleton(m_scene, m_fbxScene);
    m_exporterAnimation = m_exporterModuleFactory->createExporterModuleAnimation(m_scene, m_fbxScene);
}

void FbxExporter::exportToFile(string outputFilepath)
{
    exportModels(outputFilepath);

    FbxSdkCommon::SaveScene(m_fbxManager, m_fbxScene, outputFilepath.c_str(), false, false);
}

void FbxExporter::exportModels(string outputFilepath)
{
    if (m_options.exportMaterials) {
        m_exporterMaterial->exportMaterials(outputFilepath);
    }

    for (auto model : m_scene->getModels()) {
        // Export skeleton if enabled.
        if (m_options.exportSkeleton && model->getBones().size() > 0) {
            auto exist = m_fbxScene->GetRootNode()->FindChild(model->getBones()[0]->getData().Name);

            if (!exist) {
                m_exporterSkeleton->exportBones(model);
            } else {
                for (auto otherModel : m_scene->getModels()) {
                    if (otherModel != model && otherModel->getBones()[0]->getNode() == exist) {
                        model->setBones(otherModel->getBones());
                    }
                }
            }
        }

        if (model->isExcluded()) {
            continue;
        }

        if (m_options.exportMeshes) {
            m_exporterMesh->exportMeshes(model, m_options.exportSkeleton);
        }

        if (m_options.exportSkeleton && model->getBones().size() > 1) {
            m_exporterSkeleton->exportPoses(model);
        }
    }

    if (m_options.exportAnimation) {
        m_exporterAnimation->exportAnimations();
    }
}

} // namespace GCL::Exporter
