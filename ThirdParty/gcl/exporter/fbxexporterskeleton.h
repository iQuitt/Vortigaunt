#pragma once

#include "gcl/exporter/fbxexportermodule.h"

namespace GCL::Exporter {

using namespace std;
using namespace GCL::Bindings;

///
/// \brief The ExporterSkeleton class.
///
class FbxExporterSkeleton : public FbxExporterModule {
public:
    // Inherit constructor.
    using FbxExporterModule::FbxExporterModule;

    ///
    /// \brief Export the bones of a model to the fbx scene.
    /// \param model Model of which the bones need to be exported of to the fbx scene.
    ///
    void exportBones(shared_ptr<Model> model);

    ///
    /// \brief Export the poses of a model to the fbx scene.
    /// \param model Model of which the poses need to be exported of to the fbx scene.
    ///
    void exportPoses(shared_ptr<Model> model);

protected:
    ///
    /// \brief Export a bone of the scene to the fbx scene.
    /// \param model A model the bone is related to.
    /// \param bone A bone which needs to be exported.
    ///
    void exportBone(shared_ptr<Model> model, shared_ptr<Bone> bone);

    ///
    /// \brief Export the bind pose of a model to the fbx scene.
    /// \param model Model of which the bind pose needs to be exported of to the fbx scene.
    ///
    void exportBindPose(shared_ptr<Model> model);

    ///
    /// \brief Export the rest pose of a model to the fbx scene.
    /// \param model Model of which the rest pose needs to be exported of to the fbx scene.
    ///
    void exportRestPose(shared_ptr<Model> model);
};

} // namespace GCL::Exporter
