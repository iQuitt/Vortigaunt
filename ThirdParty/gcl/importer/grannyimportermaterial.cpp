#include "gcl/importer/grannyimportermaterial.h"

namespace GCL::Importer {

GrannyImporterMaterial::GrannyImporterMaterial(Scene::SharedPtr scene)
    : m_scene(scene)
{
}

GrannyImporterMaterial::~GrannyImporterMaterial()
{
}

void GrannyImporterMaterial::importMaterials(granny_file_info* granny_file_info) const
{
    for (unsigned i = 0; i < static_cast<unsigned>(granny_file_info->MaterialCount); i++) {
        m_scene->addMaterial(make_shared<Material>(granny_file_info->Materials[i]));
    }
}

} // namespace GCL::Importer
