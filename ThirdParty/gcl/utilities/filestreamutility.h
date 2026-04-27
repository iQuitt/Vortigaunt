#pragma once

#include <fstream>
#include <vector>

namespace GCL::Utilities::FileStreamUtility {

using namespace std;

///
/// \brief Reads out bytes from file offset to a value.
/// \param file
/// \param offset
/// \return
///
template <typename T>
T binaryread(fstream& file, unsigned offset);

template <>
unsigned binaryread<unsigned>(fstream& file, unsigned offset);

///
/// \brief Writes bytes to a file at specified offset.
/// \param file
/// \param offset
/// \param bytes
/// \param size
///
void binarywrite(fstream& file, unsigned offset, const char* bytes, unsigned size = 0);

///
/// \brief Returns size of a file.
/// \param file
/// \return File size
///
unsigned filesize(fstream& file);

///
/// \brief Returns a file as character list.
/// \param file
/// \return File bytes as aharacter list.
///
vector<char> binarydata(fstream& file);

} // namespace GCL::Utilities
