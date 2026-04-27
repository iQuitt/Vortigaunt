#pragma once

#include <string>

namespace GCL::Utilities {

using namespace std;

///
/// \brief Uppers case of a string.
/// \param input String
/// \return A uppercase string.
///
void toUpper(string& input);

///
/// \brief Lowers case of a string.
/// \param input String
/// \return A lowercase string.
///
void toLower(string& input);

///
/// \brief Uppers case of first character of a string.
/// \param input String
/// \return A string with its first character is a uppercase.
///
void toUpperFirst(string& input);

///
/// \brief Lowers case of first character of a string.
/// \param input String
/// \return A string with its first character is a lowercase.
///
void toLowerFirst(string& input);

} // namespace GCL::Utilities
