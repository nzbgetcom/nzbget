/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2007-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2025 Denis <denis@nzbget.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#ifndef DECODER_H
#define DECODER_H

#include <string>
#include "NString.h"
#include "Util.h"

class Decoder
{
public:
	enum EStatus
	{
		dsUnknownError,
		dsFinished,
		dsArticleIncomplete,
		dsCrcError,
		dsInvalidSize,
		dsNoBinaryData
	};

	enum EFormat
	{
		efUnknown,
		efYenc,
		efUx,
	};

	Decoder();
	EStatus Check();
	void Clear();
	int DecodeBuffer(char* buffer, int len);
	void SetCrcCheck(bool crcCheck) { m_crcCheck = crcCheck; }
	void SetRawMode(bool rawMode) { m_rawMode = rawMode; }
	EFormat GetFormat() { return m_format; }
	int64 GetBeginPos() { return m_beginPos; }
	int64 GetEndPos() { return m_endPos; }
	int64 GetSize() { return m_size; }
	uint32 GetExpectedCrc() { return m_expectedCRC; }
	uint32 GetCalculatedCrc() { return m_calculatedCRC; }
	bool GetEof() { return m_eof; }
	const char* GetArticleFilename() { return m_articleFilename.c_str(); }

private:
	/**
	 * @brief Extracts the begin and end positions from a "=ypart" line.
	 *
	 * Parses the "=ypart" line to find " begin=" and " end=".
	*/
	void ParseYpart(const char* buffer);

	/**
	 * @brief Extracts the filename from a yEnc "name=" field.
	 *
	 * Parses the "name=" field, handling cases where "=ypart" is on the same line.
	 * If "=ypart" is found within the name, ParseYpart is called.
	 * @note This function handles the unusual case where "=ypart" information is present
	 *       on the same line as the "name=" field.  This can happen if the yEnc encoder
	 *       doesn't properly separate the header lines, e.g.:
	 * 		 =ybegin part=1 total=10 line=128 size=500000 name=mybinary.dat=ypart begin=1 end=100000
	 *		 .... data
	 *		 =yend size=100000 part=1 pcrc32=abcdef12
	 * 		 Normal message:
	 *	     =ybegin part=1 total=10 line=128 size=500000 name=mybinary.dat
	 *       =ypart begin=1 end=100000
	 *		 .... data
	 *		 =yend size=100000 part=1 pcrc32=abcdef12
	*/
	void ParseName(const char* buffer, int len);

	EFormat m_format = efUnknown;
	bool m_begin;
	bool m_part;
	bool m_body; 
	bool m_end;
	bool m_crc;
	uint32 m_expectedCRC;
	uint32 m_calculatedCRC;
	int64 m_beginPos;
	int64 m_endPos;
	int64 m_size;
	int64 m_endSize;
	int64 m_outSize;
	bool m_eof;
	bool m_crcCheck;
	char m_state;
	bool m_rawMode = false;
	std::string m_articleFilename;
	StringBuilder m_lineBuf;
	Crc32 m_crc32;

	EFormat DetectFormat(const char* buffer, int len);
	void ProcessYenc(char* buffer, int len);
	int DecodeYenc(char* buffer, char* outbuf, int len);
	EStatus CheckYenc();
	int DecodeUx(char* buffer, int len);
	EStatus CheckUx();
	void ProcessRaw(char* buffer, int len);
};

#endif
