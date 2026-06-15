#pragma once

#include <cstring>
#include <string>

// Forward declarations
struct UnityObjectInfo;
class UnityAssetParser;

/**
 * Decoder for Unity texture objects.
 * Currently supports conversion to BMP files.
 */
class UnityTextureDecoder {
public:
    /**
     * Convert a Unity texture object to a BMP file.
     * @param objInfo   Information about the Unity object to decode.
     * @param parser    Reference to a populated UnityAssetParser for deserialization.
     * @param outputDir Directory where the BMP file will be written.
     */
    static void Convert(const UnityObjectInfo& objInfo,
                         const UnityAssetParser& parser,
                         const std::string& outputDir);
};
