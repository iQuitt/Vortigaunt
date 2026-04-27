#include "de_file.h"

BaseFileStream::BaseFileStream()
{
	m_FileLen = 0;
	m_pFile = LTNULL;
	m_ErrorStatus = 0;
}

BaseFileStream::~BaseFileStream()
{
	if (m_pFile)
	{
		fclose(m_pFile);
		m_pFile = LTNULL;
	}
}

LTRESULT BaseFileStream::ErrorStatus()
{
	return m_ErrorStatus ? LT_ERROR : LT_OK;
}

LTRESULT BaseFileStream::GetLen(uint32 *len)
{
	*len = m_FileLen;
	return LT_OK;
}

LTRESULT BaseFileStream::Write(const void *pData, uint32 dataLen)
{
	return LT_ERROR;
}

void DosFileStream::Release()
{
}

LTRESULT DosFileStream::Open(const char *pName)
{
	FILE *fp = fopen(pName, "rb");

	if (!fp)
	{
		m_ErrorStatus = 1;
		return LT_ERROR;
	}

	fseek(fp, 0, SEEK_END);
	uint32 fileLen = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	m_pFile = fp;
	m_FileLen = fileLen;

	return LT_OK;
}

LTRESULT DosFileStream::Close()
{
	if (m_pFile)
	{
		fclose(m_pFile);
		m_pFile = LTNULL;
	}

	return LT_OK;
}

LTRESULT DosFileStream::GetPos(uint32 *pos)
{
	*pos = (uint32)(ftell(m_pFile));
	return LT_OK;
}

LTRESULT DosFileStream::SeekTo(uint32 offset)
{
	if(fseek(m_pFile, offset, SEEK_SET) == 0)
	{
		return LT_OK;
	}
	else
	{
		m_ErrorStatus = 1;
		return LT_ERROR;
	}
}

LTRESULT DosFileStream::Read(void *pData, uint32 size)
{
	if(!m_ErrorStatus && (fread(pData, 1, size, m_pFile) == size))
	{
		return LT_OK;
	}

	memset(pData, 0, size);	
	m_ErrorStatus = 1;
	return LT_ERROR;
}