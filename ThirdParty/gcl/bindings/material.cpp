#include "gcl/bindings/material.h"
#include "granny.h"

namespace GCL::Bindings {

Material::Material(granny_material* data)
    : m_data(data)
{
}

granny_material* Material::getData()
{
    return m_data;
}

FbxSurfaceMaterial* Material::getNode()
{
    return m_node;
}

void Material::setData(granny_material* data)
{
    m_data = data;
}

void Material::setNode(FbxSurfaceMaterial* node)
{
    m_node = node;
}

} // namespace GCL::Bindings
