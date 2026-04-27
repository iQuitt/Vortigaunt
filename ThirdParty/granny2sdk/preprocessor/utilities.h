#if !defined(UTILITIES_H)
// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================

#include <granny.h>
#include "preprocessor.h"
#include <functional>
#include <string.h>

struct CIStrcmp : public std::binary_function<char const*, char const*, bool>
{
    bool operator()(char const* One, char const* Two) const
    {
        return _stricmp(One, Two) < 0;
    }
};


float GetPosError(granny_transform& One, granny_transform& Two);
float GetOriError(granny_transform& One, granny_transform& Two);
float GetSSError(granny_transform& One, granny_transform& Two);

bool RecomputeMeshBoneBounds(granny_model* Model, granny_mesh* Mesh);
bool RecomputeAllBoneBounds(granny_file_info* Info);



#define UTILITIES_H
#endif
