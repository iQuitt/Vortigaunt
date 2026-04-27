#pragma once

#include "gcl/bindings/binding.h"
#include "gcl/bindings/bonebinding.h"
#include "gcl/importer/grannyformat.h"

#include <fbxsdk.h>

#include <Windows.h>
#include <vector>

namespace GCL::Bindings {

using namespace std;

///
/// \brief Binding of granny mesh data and the counterparts fbx node and the bone bindings.
///
class Mesh : public Binding<Mesh> {
public:
    ///
    /// \brief Constructer
    /// \param data Granny data of the mesh.
    ///
    Mesh(granny_mesh* data);

    ///
    /// \brief Returns the granny mesh data.
    /// \return Granny mesh data
    ///
    granny_mesh* getData();

    ///
    /// \brief Returns the fbx node of the mesh.
    /// \return Fbx mesh node
    ///
    FbxNode* getNode();

    ///
    /// \brief Returns the bone bindings of the mesh.
    /// \return Bone bindings
    ///
    vector<BoneBinding::SharedPtr> getBoneBindings();

    ///
    /// \brief Sets the granny mesh data.
    /// \param data Granny mesh data
    ///
    void setData(granny_mesh* data);

    ///
    /// \brief Sets the fbx node of the mesh.
    /// \param Fbx mesh node
    ///
    void setNode(FbxNode* node);

    ///
    /// \brief Add bone binding to the mesh.
    /// \param Bone binding
    ///
    void addBoneBinding(BoneBinding::SharedPtr binding);

    ///
    /// \brief Returns if mesh is rigid body.
    /// \return
    ///
    bool isRigid();

    ///
    /// \brief Returns rigid vertices.
    /// \return Vertices
    ///
    vector<granny_pwnt3432_vertex> getRigidVertices();

protected:
    ///
    /// \brief Granny data of the mesh.
    ///
    granny_mesh * m_data = nullptr;

    ///
    /// \brief Fbx node of the mesh.
    ///
    FbxNode* m_node = nullptr;

    ///
    /// \brief Bone bindings of the mesh.
    ///
    vector<BoneBinding::SharedPtr> m_boneBindings;
};

} // namespace GCL::Bindings
