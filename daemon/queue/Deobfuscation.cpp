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
	/**
	 * @brief Parses a subject string that does not contain quotes, extracting relevant information.
	 *
	 * It handles formats such as:
	 * 
	 *  - "[34/44] - id.bdmv yEnc (1/1) 104"
	 * 
	 *  - "Any.Show.2024.vol127+128.par2 (1/0)"
	 * 
	 *  - "Re: SubjectWithoutParentheses"
	 */
	std::string ParseWithoutQuotes(const std::string& str)
	{
		size_t start = 0;
		size_t end = str.find(" yEnc");
		if (end != std::string::npos)
		{
			start = str.find_last_of(" ", end - 1);
			if (start == std::string::npos)
				return str.substr(0, end);

			start += 1;
			if (start < end)
				return str.substr(start, end - start);
			return str;
		}

		start = str.find("Re: ");
		end = str.rfind(" (");

		if (start != std::string::npos && end != std::string::npos)
		{
			start += 4;
			if (start < end)
				return str.substr(start, end - start);
			return str;
		}

		if (start != std::string::npos && end == std::string::npos)
		{
			return str.substr(start + 4);
		}

		if (end != std::string::npos)
		{
			return str.substr(0, end);
		}

		return str;
	}

	std::string ParsePRiVATEnzb(const std::string& str)
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
		for (char ch : middle)
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
	bool IsExcessivelyObfuscated(const std::string& str)
	{
		if (str.empty())
			return false;

		for (const auto& regex : EXCLUDED_HASHED_RELEASES_REGEXES)
		{
			if (std::regex_search(str, regex))
				return false;
		}

		for (const auto& regex : HASHED_RELEASES_REGEXES)
		{
			if (std::regex_search(str, regex))
				return true;
		}

		return false;
	}

	std::string Deobfuscate(const std::string& str)
	{
		if (str.size() < 3) 
			return str;

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
