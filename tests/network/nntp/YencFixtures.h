/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Denis <denis@nzbget.com>
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef NNTP_YENC_FIXTURES_H
#define NNTP_YENC_FIXTURES_H

#include <cstdint>
#include <cstdio>
#include <string>

#include "daemon/util/Util.h"

namespace yenc_fixtures
{

inline std::string EncodeYenc(const std::string& data)
{
	std::string out;
	size_t linePos = 0;
	for (unsigned char c : data)
	{
		if (linePos == 128)
		{
			out += "\r\n";
			linePos = 0;
		}

		const unsigned char v = static_cast<unsigned char>(c + 42);
		if (v == 0 || v == 10 || v == 13 || v == 61)
		{
			out += '=';
			out += static_cast<char>((v + 64) & 0xff);
		}
		else if (linePos == 0 && v == '.')
		{
			out += '.';
			out += static_cast<char>(v);
		}
		else
		{
			out += static_cast<char>(v);
		}
		linePos++;
	}
	out += "\r\n";
	return out;
}

inline uint32_t Crc32Of(const std::string& data)
{
	Crc32 crc;
	crc.Append(reinterpret_cast<uchar*>(const_cast<char*>(data.data())), static_cast<uint32_t>(data.size()));
	return crc.Finish();
}

inline std::string CrcHex(uint32_t crc)
{
	char buf[9];
	std::snprintf(buf, sizeof(buf), "%08x", crc);
	return std::string(buf);
}

/// Complete article body as the server sends it after "222": lines plus the "." terminator.
inline std::string MakeArticle(const std::string& payload, int64_t begin = 1, uint32_t crc = 0,
	bool multipart = false)
{
	const uint32_t crcValue = crc ? crc : Crc32Of(payload);
	const int64_t end = begin + static_cast<int64_t>(payload.size()) - 1;
	const std::string crcParam = multipart ? "pcrc32=" : "crc32=";

	// =ybegin size= is the whole file, so it must cover the part's end.
	std::string article = "=ybegin part=1 total=1 line=128 size=" + std::to_string(end) +
		" name=test.bin\r\n=ypart begin=" + std::to_string(begin) + " end=" + std::to_string(end) + "\r\n";
	article += EncodeYenc(payload);
	article += "=yend size=" + std::to_string(payload.size()) + " part=1 " + crcParam + CrcHex(crcValue) + "\r\n";
	article += ".\r\n";
	return article;
}

inline std::string MakePayload()
{
	std::string data;
	for (int i = 0; i < 5000; i++)
	{
		// Cover the escaped bytes (0, 10, 13, 61) and a dot at the start of a line.
		static const unsigned char specials[] = {0, 10, 13, 61, 46, 200, 255};
		data += (i % 97 == 0) ? static_cast<char>(specials[(i / 97) % 7])
		                      : static_cast<char>((i * 37) % 256);
	}
	// Byte 4 encodes to '.', so at a line start EncodeYenc dot-stuffs it to "..".
	data[128] = 4;
	return data;
}

} // namespace yenc_fixtures

#endif // NNTP_YENC_FIXTURES_H
