#pragma once

#include <string>

namespace GCL::Utilities {

using namespace std;

///
/// \brief Sanitizes a name (e.g. material name) by replacing "-", " ", "#" by "_".
/// \param name Material name for instance.
/// \return Sanitized name
///
string sanitizeName(string name);

} // namespace GCL::Utilities
