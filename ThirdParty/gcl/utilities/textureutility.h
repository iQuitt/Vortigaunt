#pragma once

#include "gcl/importer/grannyformat.h"
#include "granny.h"

namespace GCL::Utilities {

///
/// \brief Returns diffuse texture of a granny material.
/// \param granny_material Granny material
/// \return Diffuse texture
///
granny_texture* getMaterialTexture(granny_material* granny_material);

} // namespace GCL::Utilities
