#pragma once

#include <cstdint>
#include <string>

namespace PNG
{

// Writes ARGB32 pixels (0xAARRGGBB, row-major, top-down) as a 32-bit RGBA PNG.
bool saveArgb(const std::string& filename, int width, int height, const uint32_t* argbPixels);

} // namespace PNG
