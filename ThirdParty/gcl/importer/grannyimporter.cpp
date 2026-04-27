#include "gcl/importer/grannyimporter.h"


#include <filesystem>

namespace GCL::Importer {

GrannyImporter::GrannyImporter()
    : m_scene(new Scene())
{
    initialize();
}

GrannyImporter::GrannyImporter(Scene::SharedPtr scene)
    : m_scene(scene)
{
    initialize();
}

GrannyImporter::~GrannyImporter()
{
    for (const auto& granny_file : m_importedgranny_files) {
        GrannyFreeFile(granny_file);
    }

    delete m_importerMaterial;
    delete m_importerModel;
    delete m_importerSkeleton;
    delete m_importerAnimation;
}

void GrannyImporter::initialize()
{
    m_importerMaterial = new GrannyImporterMaterial(m_scene);
    m_importerModel = new GrannyImporterModel(m_scene);
    m_importerSkeleton = new GrannyImporterSkeleton(m_scene);
    m_importerAnimation = new GrannyImporterAnimation(m_scene);
}

void GrannyImporter::importFromFile(const char* granny_filePath)
{
    if (!std::filesystem::exists(granny_filePath)) {
        printf("Could not find file: %s\n", granny_filePath);
        printf("Press any key to exit...\n");
        getchar();
        throw new exception();
    }

    granny_file* granny_file = GrannyReadEntireFile(granny_filePath);
    granny_file_info* granny_file_info = GrannyGetFileInfo(granny_file);

    m_importedgranny_files.push_back(granny_file);

    // Add granny file base path as search path for textures.
    auto searchPath = filesystem::path(granny_filePath).parent_path();
    if (!searchPath.empty()) {
        searchPath = searchPath.string().append("/");
        m_scene->addSearchPath(searchPath.string());
    }

    // Import all materials and models of the granny file to the scene.
    importMaterials(granny_file_info, granny_filePath);
    importModels(granny_file_info, granny_filePath);

    // Import all bones of the granny file to the scene.
    for (const auto& model : m_scene->getModels()) {
        model->setBones(m_importerSkeleton->loadBones(model->getData()));
    }

    // Import all animations of the granny file to the scene.
    importAnimations(granny_file_info, granny_filePath);
}

void GrannyImporter::importMaterials(granny_file_info* granny_file_info, const char* granny_filePath)
{
    // Load materials from granny file only if it has at least one material.
    if (!granny_file_info->MaterialCount) {
        return;
    }

    printf("Import materials from granny file \"%s\".\n", granny_filePath);
    m_importerMaterial->importMaterials(granny_file_info);
}

void GrannyImporter::importModels(granny_file_info* granny_file_info, const char* granny_filePath)
{
    // Load models from granny file only if it has at least one model.
    if (!granny_file_info->ModelCount) {
        printf("Skip load models because granny file (file: \"%s\") has no models.\n", granny_filePath);
        return;
    }

    printf("Import models from granny file \"%s\".\n", granny_filePath);
    m_importerModel->importModels(granny_file_info);
}

void GrannyImporter::importAnimations(granny_file_info* granny_file_info, const char* granny_filePath)
{
    // Load animations from granny file only if it has at least one animation.
    if (!granny_file_info->AnimationCount) {
        printf("Skip load animations because granny file (file: \"%s\") has no animations.\n", granny_filePath);
        return;
    }

    printf("Import animations from granny file \"%s\".\n", granny_filePath);
    m_importerAnimation->importAnimations(granny_file_info);
}

Scene::SharedPtr GrannyImporter::getScene() const
{
    return m_scene;
}

} // namespace GCL::Importer
