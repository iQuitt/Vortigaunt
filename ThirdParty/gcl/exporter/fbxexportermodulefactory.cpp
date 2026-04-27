#include "gcl/exporter/fbxexportermodulefactory.h"

namespace GCL::Exporter {

FbxExporterMaterial* FbxExporterModuleFactory::createExporterModuleMaterial(
    Scene::SharedPtr scene,
    FbxScene* fbxScene)
{
    return new FbxExporterMaterial(scene, fbxScene);
}

FbxExporterMesh* FbxExporterModuleFactory::createExporterModuleMesh(
    Scene::SharedPtr scene,
    FbxScene* fbxScene)
{
    return new FbxExporterMesh(scene, fbxScene);
}

FbxExporterSkeleton* FbxExporterModuleFactory::createExporterModuleSkeleton(
    Scene::SharedPtr scene,
    FbxScene* fbxScene)
{
    return new FbxExporterSkeleton(scene, fbxScene);
}

FbxExporterAnimation* FbxExporterModuleFactory::createExporterModuleAnimation(
    Scene::SharedPtr scene,
    FbxScene* fbxScene)
{
    return new FbxExporterAnimation(scene, fbxScene);
}

} // namespace GCL::Exporter
