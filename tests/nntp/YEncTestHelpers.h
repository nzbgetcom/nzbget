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
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef YENC_TEST_HELPERS_H
#define YENC_TEST_HELPERS_H

// Shared hex/encode helpers for the NNTP encryption test suites .
// Header-only inline definitions so every test TU sees identical symbols
// without ODR-risking duplicate non-static definitions.

#include "nzbget.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>

namespace YEncTest
{

inline std::vector<uint8_t> HexToBin(const std::string& hex)
{
	std::vector<uint8_t> bytes;
	bytes.reserve(hex.size() / 2);
	for (size_t i = 0; i + 1 < hex.size(); i += 2)
	{
		bytes.push_back(static_cast<uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16)));
	}
	return bytes;
}

inline std::string BinToHex(const uint8_t* data, size_t len)
{
	std::ostringstream oss;
	for (size_t i = 0; i < len; ++i)
	{
		oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
	}
	return oss.str();
}

inline std::string BinToHex(const std::vector<uint8_t>& data)
{
	return BinToHex(data.data(), data.size());
}

// Simple (escape-free) yEnc body encoding used to build small test fixtures.
inline std::string SimpleYencEncode(const std::vector<uint8_t>& data)
{
	std::string encoded;
	encoded.reserve(data.size());
	for (uint8_t c : data)
	{
		encoded += static_cast<char>((c + 42) % 256);
	}
	return encoded;
}

inline std::string SimpleYencEncode(const std::string& data)
{
	return SimpleYencEncode(std::vector<uint8_t>(data.begin(), data.end()));
}

inline std::string FormatHex8(uint32_t value)
{
	char buf[9];
	snprintf(buf, sizeof(buf), "%08x", value);
	return buf;
}

} // namespace YEncTest

#endif // YENC_TEST_HELPERS_H
