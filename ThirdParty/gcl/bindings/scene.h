#pragma once

#include "gcl/bindings/animation.h"
#include "gcl/bindings/material.h"
#include "gcl/bindings/model.h"
#include "gcl/importer/grannyformat.h"

#include <set>
#include <string>
#include <vector>

namespace GCL::Bindings {

using namespace std;

///
/// \brief The Scene class.
///
class Scene {
public:
    ///
    /// \brief Shared pointer alias
    ///
    using SharedPtr = shared_ptr<Scene>;

    ///
    /// \brief Returns all materials of the scene.
    /// \return Materials used in scene.
    ///
    vector<Material::SharedPtr> getMaterials();

    ///
    /// \brief Append a material to the scene.
    /// \param material Material used in scene.
    ///
    void addMaterial(Material::SharedPtr material);

    ///
    /// \brief Returns all models of the scene.
    /// \return Models of the scene.
    ///
    vector<Model::SharedPtr> getModels();

    ///
    /// \brief Append a model to the scene.
    /// \param Model to be added to the scene.
    ///
    void addModel(Model::SharedPtr model);

    ///
    /// \brief Returns all animations of the scene.
    /// \return Animations of the scene.
    ///
    vector<Animation::SharedPtr> getAnimations();

    ///
    /// \brief Append a animation to the scene.
    /// \param Animation to be added to the scene.
    ///
    void addAnimation(Animation::SharedPtr animation);

    ///
    /// \brief Returns all search paths of the scene.
    /// \return Search paths to look up for scene relevant files e.g. textures.
    ///
    set<string> getSearchPaths();

    ///
    /// \brief Append a search path to the scene.
    /// \param searchPath Search path to be added.
    ///
    void addSearchPath(string searchPath);

protected:
    ///
    /// \brief Models of the scene.
    ///
    vector<Model::SharedPtr> m_models;

    ///
    /// \brief Materials of the scene.
    ///
    vector<Material::SharedPtr> m_materials;

    ///
    /// \brief Animations of the scene.
    ///
    vector<Animation::SharedPtr> m_animations;

    ///
    /// \brief Search paths to look up for scene relevant files e.g. textures.
    ///
    set<string> m_searchPaths;
};

} // namespace GCL::Bindings
