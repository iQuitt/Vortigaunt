#pragma once

#include "7zFile.h"
#include "LzmaDec.h"

enum {
	DEC_RET_SUCCESSFUL = 0,
	DEC_RET_OPEN_INPUT_FILE_FAILED = 1,
	DEC_RET_OPEN_OUTPUT_FILE_FAILED = 2,
	DEC_RET_CREATE_OUT_STREAM_FAILED = 3,
	DEC_RET_ERROR_DATA = 4,
	DEC_RET_ERROR_WRITE = 5,
	DEC_RET_ERROR_READ = 6,
	DEC_RET_ERROR_MEM = 7,
};

class LzmaDecoder {
public:
	LzmaDecoder();
	~LzmaDecoder();

	int Decode(const char* inputFilePath, const char* outFilePath);
	void Destroy();
private:
	SRes startDecoding(ISeqOutStream* outStream, ISeqInStream* inStream, UInt64* decodeDataSize);
	SRes decoding(CLzmaDec* state, ISeqOutStream* outStream, ISeqInStream* inStream, UInt64 unpackSize, UInt64* outSize);

};
