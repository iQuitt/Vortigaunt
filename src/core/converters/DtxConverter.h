#pragma once

#include <string>
#include <vector>
#include <cstdint>

#ifdef ENABLE_LITHTECH

#include "bdefs.h"
#include "dtxmgr.h"
#include "load_pcx.h"
#include "pixelformat.h"
#include "streamsim.h"

#include "core/converters/LzmaDecoder.h"

enum
{
	DTX_CONVERT_OK = 0,
	DTX_CONVERT_INVALID_INPUT_FILE = 1,
	DTX_CONVERT_DECODING_FAILED = 2,
	DTX_CONVERT_FAILED = 3,
};


class DtxConverter
{
public:
	DtxConverter();
	~DtxConverter();
	int ConvertSingleDTXFile(const std::string& format,const std::string& inputFilePath, const std::string& outFilePath);


    // Decode a DTX (handling optional LZMA compression) into a 32-bit RGBA buffer.
    // Returns true on success and fills outPixels (size = width*height), width and height.
    bool DecodeDTXToRGBA(const std::string& inputFilePath, std::vector<unsigned int>& outPixels, int& width, int& height);

    // Decode a DTX directly from memory (data already decompressed, e.g. from REZ).
    bool DecodeDTXBufferToRGBA(const uint8_t* data, size_t dataSize, std::vector<unsigned int>& outPixels, int& width, int& height);
private:
    bool DecodeStreamToRGBA(ILTStream* stream,
                            std::vector<unsigned int>& outPixels,
                            int& width,
                            int& height);

	LzmaDecoder* m_lzmaDecoder;
};

#else 

// Stubs for linux (Lithtech SDK Does not Support Linux)
enum
{
	DTX_CONVERT_OK = 0,
	DTX_CONVERT_INVALID_INPUT_FILE = 1,
	DTX_CONVERT_DECODING_FAILED = 2,
	DTX_CONVERT_FAILED = 3,
};

class DtxConverter
{
public:
	DtxConverter() = default;
	~DtxConverter() = default;
	int ConvertSingleDTXFile(const std::string&, const std::string&, const std::string&) { return DTX_CONVERT_FAILED; }
    bool DecodeDTXToRGBA(const std::string&, std::vector<unsigned int>&, int&, int&) { return false; }
    bool DecodeDTXBufferToRGBA(const uint8_t*, size_t, std::vector<unsigned int>&, int&, int&) { return false; }
};

#endif
