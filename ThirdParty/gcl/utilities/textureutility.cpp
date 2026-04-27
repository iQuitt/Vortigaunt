#include "gcl/utilities/textureutility.h"

namespace GCL::Utilities {

granny_texture* getMaterialTexture(granny_material* granny_material)
{
    granny_texture* texture = nullptr;

    if (granny_material->MapCount) {
        texture = granny_material->Maps[0].Material->Texture;

        if (!texture && granny_material->Maps[0].Material->MapCount) {
            texture = granny_material->Maps[0].Material->Maps[0].Material->Texture;
        }
    } else {
        texture = granny_material->Texture;
    }

    return texture;
}

} // namespace GCL::Utilities
