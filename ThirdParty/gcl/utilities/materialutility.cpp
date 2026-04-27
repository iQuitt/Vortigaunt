#include "gcl/utilities/materialutility.h"

#include <algorithm>

namespace GCL::Utilities {

string sanitizeName(string name)
{
    replace(name.begin(), name.end(), '-', '_');
    replace(name.begin(), name.end(), ' ', '_');
    replace(name.begin(), name.end(), '#', '_');
    return name;
}

} // namespace GCL::Utilities
