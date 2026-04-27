#pragma once

#include "gcl/importer/grannyformat.h"

#include <fbxsdk.h>

#include <memory>
#include <vector>
#include "granny.h"

namespace GCL::Bindings {

using namespace std;

///
/// \brief Binding of granny bone data and the counterparts fbx node and the fbx skeleton.
///
class Bone {
public:
    ///
    /// \brief Shared pointer alias
    ///
    using SharedPtr = shared_ptr<Bone>;

    ///
    /// \brief Constructer with data initialization.
    /// \param data Granny bone data
    /// \param node Fbx node of the bone.
    /// \param skeleton Fbx skeleton of the bone.
    ///
    Bone(granny_bone data, FbxNode* node, FbxSkeleton* skeleton);

    ///
    /// \brief Constructer with data initialization.
    /// \param Granny bone data
    ///
    Bone(granny_bone data);

    ///
    /// \brief Constructer
    ///
    Bone();

    ///
    /// \brief Returns the granny bone data.
    /// \return
    ///
    granny_bone getData();

    ///
    /// \brief Returns the fbx node of the bone.
    /// \return Fbx bone node
    ///
    FbxNode* getNode();

    ///
    /// \brief Returns the fbx skeleton of the bone.
    /// \return Fbx skeleton
    ///
    FbxSkeleton* getSkeleton();

    ///
    /// \brief Returns the fbx clusters of the bone.
    /// \return Fbx clusters
    ///
    vector<FbxCluster*> getClusters();

    ///
    /// \brief Sets the granny bone data.
    /// \param data
    ///
    void setData(granny_bone data);

    ///
    /// \brief Sets the fbx node of the bone.
    /// \param node
    ///
    void setNode(FbxNode* node);

    ///
    /// \brief Sets the fbx skeleton of the bone.
    /// \param skeleton
    ///
    void setSkeleton(FbxSkeleton* skeleton);

    ///
    /// \brief Append a fbx cluster to the bone.
    /// \param cluster
    ///
    void addCluster(FbxCluster* cluster);

protected:
    ///
    /// \brief Granny data of the bone.
    ///
    granny_bone m_data;

    ///
    /// \brief Fbx node of the bone
    ///
    FbxNode* m_node = nullptr;

    ///
    /// \brief Fbx skeleton of the bone.
    ///
    FbxSkeleton* m_skeleton = nullptr;

    ///
    /// \brief Fbx clusters of the bone.
    ///
    vector<FbxCluster*> m_clusters;
};

} // namespace GCL::Bindings
