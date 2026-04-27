#pragma once

#include <fbxsdk.h>

namespace GCL::Utilities {

///
/// \brief The FbxSdkCommon class
///
class FbxSdkCommon {
public:
    ///
    /// \brief Initializes fbx sdk.
    /// \param manager Fbx manager instance
    /// \param scene Fbx scene instance
    ///
    static void InitializeSdkObjects(FbxManager*& manager, FbxScene*& scene);

    ///
    /// \brief Destroys fbx sdk.
    /// \param manager Fbx manager instance
    ///
    static void DestroySdkObjects(FbxManager* manager);

    ///
    /// \brief Exports a fbx scene to a fbx file.
    /// \param fbxManager Fbx manager instance
    /// \param document Fbx document
    /// \param filename Fbx filename
    /// \param embedMedia Sets if fbx should embed media.
    /// \param ascii Sets if fbx should be exported as ascii.
    /// \return Returns whether export was successful or not.
    ///
    static bool SaveScene(FbxManager* fbxManager, FbxDocument* document, const char* filename, bool embedMedia = false, bool ascii = false);
};

} // namespace GCL::Utilities
