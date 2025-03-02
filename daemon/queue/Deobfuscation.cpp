/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2024-2025 Denis <denis@nzbget.com>
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


#include "nzbget.h"

#include "Deobfuscation.h"

namespace
{
	std::string ParseWithoutQuotes(const std::string& str) noexcept
	{
		if (str.size() < 8) return str;
		
		const std::string start = "Re: ";
		size_t startPos = str.find(start);
		size_t endPos = str.rfind(" (");
		if (endPos == std::string::npos)
		{
			return str;
		}

		if (startPos != std::string::npos)
		{
			startPos += start.size();

			size_t distance = endPos - startPos;
			if (distance < 1) return str;

			return str.substr(startPos, distance);
		}

		return str.substr(0, endPos);
	}

	std::string ParsePRiVATEnzb(const std::string& str) noexcept
	{
		const std::string signature = "[PRiVATE]-[";
		const std::string endOfSignature = "]-";

		size_t beginPos = str.find(signature);
		if (beginPos == std::string::npos) return str;

		beginPos += signature.size();

		beginPos = str.find("]-[", beginPos);
		if (beginPos == std::string::npos) return str;

		beginPos += endOfSignature.size();

		const std::string end = " - \"\"";
		size_t endPos = str.rfind(end);
		if (endPos == std::string::npos) return str;

		// can be 
		// [path/something[123].bin]-[1/10]
		// or
		// [1/10]-[path/something[123].bin]
		size_t distance = endPos - beginPos;
		const std::string middle = str.substr(beginPos, distance);

		std::string result;
		result.reserve(middle.size());

		bool foundExtOrWord = false;
		int depth = 0;
		for (unsigned char ch : middle)
		{
			if (ch == '[' && ++depth == 1) continue;
			if (ch == ']' && --depth == 0) continue;
			if (foundExtOrWord && !depth) break;

			if (!foundExtOrWord && !depth)
			{
				result.clear();
				continue;
			}

			if (!depth && ch == '-') continue;

			if (depth == 1 && (ch == '/' || ch == '\\'))
			{
				result.clear();
				continue;
			}

			if (depth == 1 && (ch == '.' || isalpha(ch) != 0))
				foundExtOrWord = true;

			result.push_back(ch);
		}

		result.shrink_to_fit();

		return result;
	}
}

namespace Deobfuscation
{
	bool IsExcessivelyObfuscated(const std::string& str) noexcept
	{
		if (str.empty())
			return false;

		for (auto& regex : EXCLUDED_HASHED_RELEASES_REGEXES)
		{
			if (std::regex_search(str, regex))
				return false;
		}

		for (auto& regex : HASHED_RELEASES_REGEXES)
		{
			if (std::regex_search(str, regex))
				return true;
		}

		return false;
	}

	std::string Deobfuscate(const std::string& str) noexcept
	{
		if (str.size() < 3) return str;

		size_t firstQuotPos = str.find("\"");

		if (firstQuotPos == std::string::npos)
			return ParseWithoutQuotes(str);

		firstQuotPos += 1;
		size_t secondQuotPos = str.find("\"", firstQuotPos);
		size_t distance = secondQuotPos - firstQuotPos;

		if (distance == 0)
			return ParsePRiVATEnzb(str);

		if (distance > 0)
			return str.substr(firstQuotPos, distance);

		return str;
	}
}
