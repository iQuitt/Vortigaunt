#include "gcl/exporter/fbxexportermaterial.h"

#include <filesystem>

namespace GCL::Exporter {

FbxExporterMaterial::FbxExporterMaterial(Scene::SharedPtr scene, FbxScene* fbxScene)
    : FbxExporterModule(scene, fbxScene)
{
    GCL::Utilities::initializeDevilImageLibrary();
}

FbxExporterMaterial::~FbxExporterMaterial()
{
    GCL::Utilities::shutdownDevilImageLibrary();
}

void FbxExporterMaterial::exportMaterials(string outputFilepath)
{
    for (auto material : m_scene->getMaterials()) {
        if (!material->isExcluded()) {
            exportMaterial(outputFilepath, material);
        }
    }
}

FbxSurfaceMaterial* FbxExporterMaterial::addMaterial(const string materialName, const string textureFilePath)
{
    auto texture = FbxFileTexture::Create(m_fbxScene, materialName.c_str());

    if (!textureFilePath.empty()) {
        texture->SetFileName(textureFilePath.c_str());
    }

    texture->SetTextureUse(FbxTexture::ETextureUse::eStandard);
    texture->SetMappingType(FbxTexture::EMappingType::eUV);
    texture->SetMaterialUse(FbxFileTexture::EMaterialUse::eModelMaterial);
    texture->SetSwapUV(false);
    texture->SetTranslation(0.0, 0.0);
    texture->SetScale(1.0, 1.0);
    texture->SetRotation(0.0, 0.0);
    texture->SetPremultiplyAlpha(false);

    auto material = FbxSurfacePhong::Create(m_fbxScene, materialName.c_str());
    material->Diffuse.ConnectSrcObject(texture);
    material->SpecularFactor.Set(0);

    return material;
}

void FbxExporterMaterial::exportMaterial(string outputFilepath, Material::SharedPtr material)
{
    const auto materialName = sanitizeMaterialName(string(material->getData()->Name));
    const auto texture = GCL::Utilities::getMaterialTexture(material->getData());

    if (!texture) {
        material->setNode(addMaterial(materialName));
    } else {
        auto sourceTextureFilePath = string(texture->FromFileName);
        auto sourceTextureFileName = sourceTextureFilePath;

        const auto fileNameBeginsOffset = sourceTextureFilePath.find_last_of('\\');

        if (fileNameBeginsOffset != string::npos) {
            sourceTextureFileName = sourceTextureFilePath.substr(fileNameBeginsOffset + 1);
        }

        const auto textureFileNameWithoutExtension = sourceTextureFileName.substr(0, sourceTextureFileName.find_first_of('.'));
        const auto textureFileNameWithExtension = sanitizeFileName(textureFileNameWithoutExtension).append(".png");

        if (!ifstream(sourceTextureFilePath.c_str()).good()) {
            if (ifstream(sourceTextureFileName.c_str()).good()) {
                sourceTextureFilePath = sourceTextureFileName;
            } else {
                for (auto& searchPath : m_scene->getSearchPaths()) {
                    if (ifstream((searchPath + sourceTextureFileName).c_str()).good()) {
                        sourceTextureFilePath = searchPath + sourceTextureFileName;
                        break;
                    }
                }
            }
        }

        if (ifstream(sourceTextureFilePath.c_str()).good()) {
            const auto fileSeparator = outputFilepath.find_last_of('\\');
            const auto targetTextureFilePath = outputFilepath.substr(0, fileSeparator + 1) + textureFileNameWithExtension;
            GCL::Utilities::convertImage(sourceTextureFilePath, targetTextureFilePath);
        }

        material->setNode(addMaterial(materialName, textureFileNameWithExtension));
    }
}

} // namespace GCL::Exporter
