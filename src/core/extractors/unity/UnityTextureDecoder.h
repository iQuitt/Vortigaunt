#pragma once

#include <cstring>
#include <string>

// Forward declarations
struct UnityObjectInfo;
class UnityAssetParser;

/**
 * Decoder for Unity texture objects.
 * Supports conversion to 8-bit indexed BMP (GoldSrc-friendly) or 32-bit RGBA PNG.
 */
class UnityTextureDecoder {
public:
    /**
     * Convert a Unity texture object to an image file.
     * @param objInfo      Information about the Unity object to decode.
     * @param parser       Reference to a populated UnityAssetParser for deserialization.
     * @param outputDir    Directory where the image file will be written.
     * @param convertToBmp true -> 8-bit indexed BMP, false -> 32-bit RGBA PNG (keeps alpha).
     */
    static void Convert(const UnityObjectInfo& objInfo,
                         const UnityAssetParser& parser,
                         const std::string& outputDir,
                         bool convertToBmp = true);
};
