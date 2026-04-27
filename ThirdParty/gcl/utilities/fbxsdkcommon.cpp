#include "gcl/utilities/fbxsdkcommon.h"

#include "gcl/utilities/filestreamutility.h"



namespace GCL::Utilities {

using namespace std;

void FbxSdkCommon::InitializeSdkObjects(FbxManager*& manager, FbxScene*& scene)
{
    manager = FbxManager::Create();

    if (!manager) {
        printf("Unable to create FBX Manager!\n");
        exit(1);
    }

    manager->SetIOSettings(FbxIOSettings::Create(manager, IOSROOT));

    scene = FbxScene::Create(manager, "Scene");

    if (!scene) {
        printf("Unable to create FBX scene!\n");
        exit(1);
    }
}

void FbxSdkCommon::DestroySdkObjects(FbxManager* manager)
{
    manager->Destroy();
}

bool FbxSdkCommon::SaveScene(FbxManager* fbxManager, FbxDocument* document, const char* filename, bool embedMedia, bool ascii)
{
    FbxExporter* exporter = FbxExporter::Create(fbxManager, "");

    if (!exporter->SetFileExportVersion(FBX_2019_00_COMPATIBLE, FbxSceneRenamer::eNone)) {
        printf("Failed to set file export version to fbx 2019.\n");
    }

    auto ioPluginRegistry = fbxManager->GetIOPluginRegistry();
    auto fileFormat = -1;

    // Set ascii flag to true to export fbx file as ascii.
    // ascii = true;

    if (ascii) {
        fileFormat = ioPluginRegistry->FindWriterIDByDescription("FBX ascii (*.fbx)");
    } else {
        fileFormat = ioPluginRegistry->GetNativeWriterFormat();
    }

    FbxIOSettings* settings = fbxManager->GetIOSettings();
    settings->SetBoolProp(EXP_FBX_MATERIAL, true);
    settings->SetBoolProp(EXP_FBX_TEXTURE, true);
    settings->SetBoolProp(EXP_FBX_EMBEDDED, embedMedia);
    settings->SetBoolProp(EXP_FBX_SHAPE, true);
    settings->SetBoolProp(EXP_FBX_GOBO, true);
    settings->SetBoolProp(EXP_FBX_ANIMATION, true);
    settings->SetBoolProp(EXP_FBX_GLOBAL_SETTINGS, true);
    settings->SetBoolProp(EXP_ASCIIFBX, ascii);

    if (exporter->Initialize(filename, fileFormat, settings) == false) {
        printf("Failed to initialize fbx exporter!\n");
        printf("Error returned: %s\n", exporter->GetStatus().GetErrorString());
        return false;
    }

    // Change working directory to export location.
    wchar_t cwd[1024];
    _wgetcwd(cwd, 1024);

    auto sourceTextureDirname = string(filename);
    const auto dirnameEndsOffset = sourceTextureDirname.find_last_of('\\');

    if (dirnameEndsOffset != string::npos) {
        sourceTextureDirname = sourceTextureDirname.substr(0, dirnameEndsOffset);
    }

    _wchdir(wstring(sourceTextureDirname.begin(), sourceTextureDirname.end()).c_str());

    // Export scene to fbx file.
    bool status = true;
    status = exporter->Export(document);
    exporter->Destroy();

    // Change working directory back to previous location.
    _wchdir(cwd);

    return status;
}

} // namespace GCL::Utilities
