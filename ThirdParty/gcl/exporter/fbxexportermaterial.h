#pragma once

#include "gcl/bindings/scene.h"
#include "gcl/exporter/fbxexportermodule.h"
#include "gcl/importer/grannyformat.h"
#include "gcl/utilities/devilimageutility.h"
#include "gcl/utilities/materialutility.h"
#include "gcl/utilities/textureutility.h"

#include <fbxsdk.h>

#include <fstream>
#include <map>
#include <string>

namespace GCL::Exporter {

using namespace std;
using namespace GCL::Bindings;

///
/// \brief The ExporterMaterial class.
///
class FbxExporterMaterial : public FbxExporterModule {
public:
    ///
    /// \brief ExporterModule
    /// \param scene Scene which needs to be exported.
    /// \param fbxScene Fbx scene which has to be used for the export.
    ///
    FbxExporterMaterial(Scene::SharedPtr scene, FbxScene* fbxScene);

    ///
    /// \brief Destructor
    ///
    virtual ~FbxExporterMaterial();

    ///
    /// \brief Export materials to the fbx scene.
    ///
    void exportMaterials(string outputFilepath);

protected:
    ///
    /// \brief Export all materials of the scene to the fbx scene.
    /// \param outputFilepath Output filepath of current model.
    /// \param material Material which should be exported.
    ///
    void exportMaterial(string outputFilepath, Material::SharedPtr material);

    ///
    /// \brief Exports a material of the scene to the fbx scene - part of exportMaterial.
    /// \param materialName Name of material which should be exported.
    /// \param textureFilePath Filepath of diffuse texture for current material.
    ///
    FbxSurfaceMaterial* addMaterial(const string materialName, const string textureFilePath = "");

    ///
    /// \brief Sanitizes a material name.
    /// \param name
    /// \return
    ///
    virtual string sanitizeMaterialName(string name)
    {
        return GCL::Utilities::sanitizeName(name);
    }

    ///
    /// \brief Sanitizes afile name without path.
    /// \param name
    /// \return
    ///
    virtual string sanitizeFileName(string name)
    {
        return GCL::Utilities::sanitizeName(name);
    }

protected:
    ///
    /// \brief Exported materials of the current exporting scene stored by material name.
    ///
    map<string, FbxSurfacePhong*> m_fbxMaterials;
};

} // namespace GCL::Exporter
