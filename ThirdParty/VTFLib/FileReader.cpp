/*
 * VTFLib
 * Copyright (C) 2005-2010 Neil Jedrzejewski & Ryan Gregg

 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later
 * version.
 */



#include "VTFLib.h"
#include "FileReader.h"

using namespace VTFLib;
using namespace VTFLib::IO::Readers;

CFileReader::CFileReader(const vlChar *cFileName)
{
	this->hFile = NULL;

	this->cFileName = new vlChar[strlen(cFileName) + 1];
	strcpy(this->cFileName, cFileName);
}

CFileReader::~CFileReader()
{
	this->Close();

	delete []this->cFileName;
}

vlBool CFileReader::Opened() const
{
	return this->hFile != NULL;
}

vlBool CFileReader::Open()
{
	this->Close();

	this->hFile = fopen(this->cFileName, "rb");

	if(this->hFile == NULL)
	{
		LastError.Set("Error opening file.", vlTrue);

		return vlFalse;
	}

	return vlTrue;
}

vlVoid CFileReader::Close()
{
	if(this->hFile != NULL)
	{
		fclose(this->hFile);
		this->hFile = NULL;
	}
}

vlUInt CFileReader::GetStreamSize() const
{
	if(this->hFile == NULL)
	{
		return 0;
	}

	long lPosition = ftell(this->hFile);
	fseek(this->hFile, 0, SEEK_END);
	long lSize = ftell(this->hFile);
	fseek(this->hFile, lPosition, SEEK_SET);

	return lSize < 0 ? 0 : (vlUInt)lSize;
}

vlUInt CFileReader::GetStreamPointer() const
{
	if(this->hFile == NULL)
	{
		return 0;
	}

	long lPosition = ftell(this->hFile);
	return lPosition < 0 ? 0 : (vlUInt)lPosition;
}

vlUInt CFileReader::Seek(vlLong lOffset, vlUInt uiMode)
{
	if(this->hFile == NULL)
	{
		return 0;
	}

	// FILE_BEGIN/FILE_CURRENT/FILE_END match SEEK_SET/SEEK_CUR/SEEK_END (0/1/2)
	fseek(this->hFile, (long)lOffset, (int)uiMode);

	return this->GetStreamPointer();
}

vlBool CFileReader::Read(vlChar &cChar)
{
	if(this->hFile == NULL)
	{
		return vlFalse;
	}

	size_t uiBytesRead = fread(&cChar, 1, 1, this->hFile);

	if(uiBytesRead != 1 && ferror(this->hFile))
	{
		LastError.Set("fread() failed.", vlTrue);
	}

	return uiBytesRead == 1;
}

vlUInt CFileReader::Read(vlVoid *vData, vlUInt uiBytes)
{
	if(this->hFile == NULL)
	{
		return 0;
	}

	size_t uiBytesRead = fread(vData, 1, (size_t)uiBytes, this->hFile);

	if(uiBytesRead != uiBytes && ferror(this->hFile))
	{
		LastError.Set("fread() failed.", vlTrue);
	}

	return (vlUInt)uiBytesRead;
}
