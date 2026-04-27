#include "gcl/bindings/scene.h"

namespace GCL::Bindings {

vector<Material::SharedPtr> Scene::getMaterials()
{
    return m_materials;
}

void Scene::addMaterial(Material::SharedPtr material)
{
    m_materials.push_back(material);
}

vector<Model::SharedPtr> Scene::getModels()
{
    return m_models;
}

void Scene::addModel(Model::SharedPtr model)
{
    m_models.push_back(model);
}

vector<Animation::SharedPtr> Scene::getAnimations()
{
    return m_animations;
}

void Scene::addAnimation(Animation::SharedPtr animation)
{
    m_animations.push_back(animation);
}

set<string> Scene::getSearchPaths()
{
    return m_searchPaths;
}

void Scene::addSearchPath(string searchPath)
{
    m_searchPaths.insert(searchPath);
}

} // namespace GCL::Bindings
