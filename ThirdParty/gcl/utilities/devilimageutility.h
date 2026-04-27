#pragma once

#include <iostream>

namespace GCL::Utilities {

using namespace std;

///
/// \brief Initializes devil image library.
///
void initializeDevilImageLibrary();

///
/// \brief Shutdown devil image library.
///
void shutdownDevilImageLibrary();

///
/// \brief Converts a image using devil image library.
/// \param sourceFilePath
/// \param targetFilePath
///
void convertImage(string sourceFilePath, string targetFilePath, bool flipImage = false);

} // namespace GCL::Utilities
