#include "gcl/exporter/fbxexportermodule.h"

namespace GCL::Exporter {

FbxExporterModule::FbxExporterModule(Scene::SharedPtr scene, FbxScene* fbxScene)
    : m_scene(scene)
    , m_fbxScene(fbxScene)
{
}

FbxExporterModule::~FbxExporterModule()
{
}

} // namespace GCL::Exporter
