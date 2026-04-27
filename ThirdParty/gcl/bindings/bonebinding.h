#pragma once

#include "gcl/bindings/bone.h"

#include <fbxsdk.h>

namespace GCL::Bindings {

using namespace std;

///
/// \brief Binding between a bone and a cluster.
///
class BoneBinding {
public:
    ///
    /// \brief Shared pointer alias
    ///
    using SharedPtr = shared_ptr<BoneBinding>;

    ///
    /// \brief Constructor
    /// \param bone Bone
    /// \param cluster Fbx cluster
    ///
    BoneBinding(Bone::SharedPtr bone, FbxCluster* cluster)
        : m_bone(bone)
        , m_cluster(cluster)
    {
    }

    ///
    /// \brief Returns the bone of the binding.
    /// \return Bone
    ///
    Bone::SharedPtr getBone()
    {
        return m_bone;
    }

    ///
    /// \brief Returns the clusters.
    /// \return Fbx cluster
    ///
    FbxCluster* getCluster()
    {
        return m_cluster;
    }

protected:
    ///
    /// \brief Scene bone of the binding.
    ///
    Bone::SharedPtr m_bone;

    ///
    /// \brief Fbx cluster of the bone.
    ///
    FbxCluster* m_cluster = nullptr;
};

} // namespace GCL::Bindings
