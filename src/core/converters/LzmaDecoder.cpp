
#include "Precomp.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "CpuArch.h"

#include "Alloc.h"
#include "7zVersion.h"
#include "LzFind.h"

#include "LzmaDecoder.h"

// from https://github.com/YoungFine0825/LTB2FBX/blob/main/Source/LzmaDecoder.cpp
LzmaDecoder::LzmaDecoder() 
{
}

LzmaDecoder::~LzmaDecoder() 
{
    Destroy();
}

void LzmaDecoder::Destroy() 
{

}

int LzmaDecoder::Decode(const char* inputFilePath, const char* outFilePath)
{

	CFileSeqInStream inStream;

	LzFindPrepare();

    //创建输入流
	FileSeqInStream_CreateVTable(&inStream);
	File_Construct(&inStream.file);
	inStream.wres = 0;

    //尝试打开输入文件
	WRes wres = InFile_Open(&inStream.file, inputFilePath);
	if (wres != 0)
	{
        File_Close(&inStream.file);
		return DEC_RET_OPEN_INPUT_FILE_FAILED;
	}

    //
    CFileOutStream outStream;
    FileOutStream_CreateVTable(&outStream);
    File_Construct(&outStream.file);
    outStream.wres = 0;

    //
    wres = OutFile_Open(&outStream.file, outFilePath);
    if (wres != 0)
    {
        File_Close(&inStream.file);
        File_Close(&outStream.file);
        return DEC_RET_OPEN_OUTPUT_FILE_FAILED;
    }

    UInt64 dataSize = 0;
    int res = startDecoding(&outStream.vt, &inStream.vt,&dataSize);
    
    File_Close(&inStream.file);
    File_Close(&outStream.file);

    if (res != SZ_OK)
    {
        if (res == SZ_ERROR_MEM)
        {
            return DEC_RET_ERROR_MEM;
        }
        else if (res == SZ_ERROR_DATA)
        {
            return DEC_RET_ERROR_DATA;
        }
        else if (res == SZ_ERROR_WRITE)
        {
            return DEC_RET_ERROR_WRITE;
        }
        else if (res == SZ_ERROR_READ)
        {
            return DEC_RET_ERROR_READ;
        }
    }

	return DEC_RET_SUCCESSFUL;
}

#define IN_BUF_SIZE (1 << 16)
#define OUT_BUF_SIZE (1 << 16)

SRes LzmaDecoder::decoding(CLzmaDec* state, ISeqOutStream* outStream, ISeqInStream* inStream,UInt64 unpackSize, UInt64* decodeDataSize)
{
    int thereIsSize = (unpackSize != (UInt64)(Int64)-1);
    Byte inBuf[IN_BUF_SIZE];
    Byte outBuf[OUT_BUF_SIZE];
    size_t inPos = 0, inSize = 0, outPos = 0;
    LzmaDec_Init(state);
    for (;;)
    {
        if (inPos == inSize)
        {
            inSize = IN_BUF_SIZE;
            RINOK(inStream->Read(inStream, inBuf, &inSize));
            inPos = 0;
        }
        {
            SRes res;
            SizeT inProcessed = inSize - inPos;
            SizeT outProcessed = OUT_BUF_SIZE - outPos;
            ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
            ELzmaStatus status;
            if (thereIsSize && outProcessed > unpackSize)
            {
                outProcessed = (SizeT)unpackSize;
                finishMode = LZMA_FINISH_END;
            }

            res = LzmaDec_DecodeToBuf(state, outBuf + outPos, &outProcessed,
                inBuf + inPos, &inProcessed, finishMode, &status);
            inPos += inProcessed;
            outPos += outProcessed;
            unpackSize -= outProcessed;

            if (outStream) 
            {
                if (outStream->Write(outStream, outBuf, outPos) != outPos)
                {
                    return SZ_ERROR_WRITE;
                }
                else
                {
                    (*decodeDataSize) += outPos;
                }
            }

            outPos = 0;

            if (res != SZ_OK || (thereIsSize && unpackSize == 0))
                return res;

            if (inProcessed == 0 && outProcessed == 0)
            {
                if (thereIsSize || status != LZMA_STATUS_FINISHED_WITH_MARK)
                    return SZ_ERROR_DATA;
                return res;
            }
        }
    }
}

SRes LzmaDecoder::startDecoding(ISeqOutStream* outStream, ISeqInStream* inStream, UInt64* decodeDataSize)
{
    UInt64 unpackSize;
    int i;
    SRes res = 0;

    CLzmaDec state;

    /* header: 5 bytes of LZMA properties and 8 bytes of uncompressed size */
    unsigned char header[LZMA_PROPS_SIZE + 8];

    /* Read and parse header */

    RINOK(SeqInStream_Read(inStream, header, sizeof(header)));

    unpackSize = 0;
    for (i = 0; i < 8; i++)
        unpackSize += (UInt64)header[LZMA_PROPS_SIZE + i] << (i * 8);

    LzmaDec_Construct(&state);
    RINOK(LzmaDec_Allocate(&state, header, LZMA_PROPS_SIZE, &g_Alloc));
    res = decoding(&state, outStream, inStream, unpackSize, decodeDataSize);
    LzmaDec_Free(&state, &g_Alloc);
    return res;
}
