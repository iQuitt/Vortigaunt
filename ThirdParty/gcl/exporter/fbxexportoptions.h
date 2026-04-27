#pragma once

namespace GCL::Exporter {

///
/// \brief Enableable / disableable options for the export of a model.
///
struct FbxExportOptions {
    ///
    /// \brief Sets whether to export models.
    ///
    /// Enable this if you want that your export include models
    /// otherwise disable it if you do not want that.
    ///
    bool exportModels = true;

    ///
    /// \brief Sets whether to export meshes.
    ///
    /// Enable this if you want that your export model include geometral meshes
    /// otherwise disable it if you do not want that.
    ///
    bool exportMeshes = true;

    ///
    /// \brief Sets whether to export materials.
    ///
    /// Enable this if you want that your export model include materials and textures
    /// otherwise disable it if you do not want that.
    ///
    bool exportMaterials = true;

    ///
    /// \brief Sets whether to export skeleton.
    ///
    /// Enable this if you want that your export model includes a skeleton
    /// otherwise disable it if you do not want that.
    ///
    bool exportSkeleton = true;

    ///
    /// \brief Sets whether to export animation.
    ///
    /// Enable this if you want that your export model include animations
    /// otherwise disable it if you do not want that.
    ///
    bool exportAnimation = true;
};

} // namespace GCL::Exporter
