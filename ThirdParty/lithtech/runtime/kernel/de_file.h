#ifndef __DE_FILE_H__
#define __DE_FILE_H__

#include "bdefs.h"
#include "iltstream.h"
#include "genltstream.h"

class BaseFileStream : public CGenLTStream
{
public:
	BaseFileStream();
	virtual ~BaseFileStream();

	LTRESULT ErrorStatus();
	LTRESULT GetLen(uint32 *len);
	LTRESULT Write(const void *pData, uint32 dataLen);

	uint32 m_FileLen;
	FILE *m_pFile;
	int m_ErrorStatus;
};

class DosFileStream : public BaseFileStream
{
public:
	void Release();

	LTRESULT Open(const char *pName);
	LTRESULT Close();
	LTRESULT GetPos(uint32 *pos);
	LTRESULT SeekTo(uint32 offset);
	LTRESULT Read(void *pData, uint32 size);
};

#endif