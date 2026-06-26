#include "DtxConverter.h"
#include "dynarray.h"
#include "fileio.h"
#include "utils/Bmp.h"
#include <string>
#include <fstream>
#include "utils/FileIO.h"
#include <filesystem>
#include <atomic>
#include <sstream>
#include <thread>

static std::string getUniqueTempPath() {
    static std::atomic<unsigned int> counter(0);
    std::stringstream ss;
    ss << "__dtx_decoded_" << std::this_thread::get_id() << "_" << counter.fetch_add(1) << ".temp";
    return (std::filesystem::temp_directory_path() / ss.str()).string();
}

// from https://github.com/YoungFine0825/LTB2FBX
FormatMgr g_FormatMgr;

DtxConverter::DtxConverter() 
{
	m_lzmaDecoder = new LzmaDecoder();
}

DtxConverter::~DtxConverter() 
{
	m_lzmaDecoder->Destroy();
}


int DtxConverter::ConvertSingleDTXFile(const std::string& format, const std::string& inputFilePath, const std::string& outFilePath)
{
    std::string tempFilePath = getUniqueTempPath();
	FILE* f = FileIO::openFile(tempFilePath, "w");
	if (f)
	{
		fclose(f);
	}

	int ret = m_lzmaDecoder->Decode(inputFilePath.c_str(), tempFilePath.c_str());
	std::string realInputFilePath;
    bool isTemp = false;
	if (ret == DEC_RET_SUCCESSFUL)
	{
		realInputFilePath = tempFilePath;
        isTemp = true;
	}
	else 
	{
		realInputFilePath = inputFilePath;
	}

	std::vector<unsigned int> pixels;
	int w, h;
	if (!DecodeDTXToRGBA(realInputFilePath, pixels, w, h))
	{
		printf("Failed to decode DTX for BMP conversion.\n");
        if (isTemp) {
            std::error_code ec;
            std::filesystem::remove(tempFilePath, ec);
        }
		return DTX_CONVERT_DECODING_FAILED;
	}

	if (!BMP::saveAsIndexed8(outFilePath, w, h, pixels.data()))
	{
		printf("Failed to write 8-bit BMP file.\n");
        if (isTemp) {
            std::error_code ec;
            std::filesystem::remove(tempFilePath, ec);
        }
		return DTX_CONVERT_FAILED;
	}

    if (isTemp) {
        std::error_code ec;
        std::filesystem::remove(tempFilePath, ec);
    }
	return DTX_CONVERT_OK;
}

bool DtxConverter::DecodeDTXToRGBA(const std::string& inputFilePath, std::vector<unsigned int>& outPixels, int& width, int& height)
{
    std::string tempFilePath = getUniqueTempPath();
    FILE* f = FileIO::openFile(tempFilePath, "w");
    if (f)
    {
        fclose(f);
    }

    int decRet = m_lzmaDecoder->Decode(inputFilePath.c_str(), tempFilePath.c_str());
    std::string realInputFilePath;
    bool isTemp = false;
    if (decRet == DEC_RET_SUCCESSFUL)
    {
        realInputFilePath = tempFilePath;
        isTemp = true;
    }
    else
    {
        realInputFilePath = inputFilePath;
    }

    DStream* pStream = streamsim_Open(realInputFilePath.c_str(), "rb");
    if (!pStream) {
        if (isTemp) {
            std::error_code ec;
            std::filesystem::remove(tempFilePath, ec);
        }
        return false;
    }

    bool success = DecodeStreamToRGBA(pStream, outPixels, width, height);

    if (isTemp) {
        std::error_code ec;
        std::filesystem::remove(tempFilePath, ec);
    }
    return success;
}

bool DtxConverter::DecodeDTXBufferToRGBA(const uint8_t* data, size_t dataSize, std::vector<unsigned int>& outPixels, int& width, int& height)
{
    if (!data || dataSize == 0 ||
        dataSize > (std::numeric_limits<uint32_t>::max)())
    {
        return false;
    }

    ILTStream* stream = streamsim_MemStreamFromBuffer(const_cast<uint8_t*>(data),
                                                      static_cast<uint32>(dataSize));
    if (!stream)
        return false;

    return DecodeStreamToRGBA(stream, outPixels, width, height);
}

bool DtxConverter::DecodeStreamToRGBA(ILTStream* stream, std::vector<unsigned int>& outPixels, int& width, int& height)
{
    if (!stream)
        return false;

    TextureData* pData = nullptr;
    if (dtx_Create(stream, &pData, FALSE) != DE_OK || !pData)
    {
        stream->Release();
        return false;
    }

    TextureMipData* pMip = &pData->m_Mips[0];
    CMoArray<DWORD> outputBuf;
    if (!outputBuf.SetSize(pMip->m_Width * pMip->m_Height))
    {
        dtx_Destroy(pData);
        stream->Release();
        return false;
    }

    ConvertRequest cRequest;
    DRESULT dResult;

    cRequest.m_pSrc = (unsigned char*)pMip->m_Data;
    dtx_SetupDTXFormat(pData, cRequest.m_pSrcFormat);
    cRequest.m_SrcPitch = pMip->m_Pitch;

    cRequest.m_pDestFormat->Init(BPP_32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    cRequest.m_pDest = (unsigned char*)outputBuf.GetArray();
    cRequest.m_DestPitch = pMip->m_Width * sizeof(DWORD);
    cRequest.m_Width = pMip->m_Width;
    cRequest.m_Height = pMip->m_Height;
    cRequest.m_Flags = 0;

    dResult = g_FormatMgr.ConvertPixels(&cRequest);
    if (dResult != LT_OK)
    {
        dtx_Destroy(pData);
        stream->Release();
        return false;
    }

    width = static_cast<int>(pMip->m_Width);
    height = static_cast<int>(pMip->m_Height);

    outPixels.assign(outputBuf.GetArray(),
                     outputBuf.GetArray() + (width * height));

    dtx_Destroy(pData);
    stream->Release();

    return !outPixels.empty();
}
