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
#include "FileSystem.h"
#include "FileTypes.h"

namespace
{
	/**
	 * @brief Extracts a filename from a subject line that has no quotes.
	 * 
	 * Handles standard Usenet subject patterns:
	 * - yEnc binaries: "[34/44] - filename.ext yEnc (1/1)" -> extracts "filename.ext"
	 * - Re: prefixes: "Re: Re: filename.ext (1/2)" -> strips "Re: " and extracts "filename.ext"
	 * - General fallbacks: "filename.ext (1/2)" -> strips trailing parentheses
	 */
	std::string ParseWithoutQuotes(std::string_view sv)
	{
		size_t start = 0;
		size_t end = sv.find(" yEnc");
		if (end != std::string_view::npos)
		{
			start = sv.find_last_of(" ", end - 1);
			if (start == std::string_view::npos)
				return std::string(sv.substr(0, end));

			start += 1;
			if (start < end)
				return std::string(sv.substr(start, end - start));
			return std::string(sv);
		}

		start = sv.find("Re: ");
		end = sv.rfind(" (");

		if (start != std::string_view::npos && end != std::string_view::npos)
		{
			start += 4;
			while (sv.compare(start, 4, "Re: ") == 0)
				start += 4;
			if (start < end)
				return std::string(sv.substr(start, end - start));
			return std::string(sv);
		}

		if (start != std::string_view::npos && end == std::string_view::npos)
		{
			size_t reStart = start + 4;
			while (sv.compare(reStart, 4, "Re: ") == 0)
				reStart += 4;
			return std::string(sv.substr(reStart));
		}

		if (end != std::string_view::npos)
		{
			return std::string(sv.substr(0, end));
		}

		return std::string(sv);
	}

	/**
	 * @brief Extracts a filename from the obfuscated "[PRiVATE]" subject format.
	 * 
	 * Handles the double-bracketed private indexer format:
	 * - "[PRiVATE]-[indexer]-[filename.ext]-[1/10] - \"\" yEnc" -> extracts "filename.ext"
	 * - "[PRiVATE]-[indexer]-[1/10]-[filename.ext] - \"\" yEnc" -> extracts "filename.ext"
	 */
	std::string ParsePRiVATEnzb(std::string_view sv)
	{
		constexpr std::string_view signature = "[PRiVATE]-[";
		constexpr std::string_view endOfSignature = "]-";

		size_t beginPos = sv.find(signature);
		if (beginPos == std::string_view::npos) return std::string(sv);

		beginPos += signature.size();

		beginPos = sv.find("]-[", beginPos);
		if (beginPos == std::string_view::npos) return std::string(sv);

		beginPos += endOfSignature.size();

		constexpr std::string_view end = " - \"\"";
		size_t endPos = sv.rfind(end);
		if (endPos == std::string_view::npos) return std::string(sv);

		// can be
		// [path/something[123].bin]-[1/10]
		// or
		// [1/10]-[path/something[123].bin]
		size_t distance = endPos - beginPos;
		std::string_view middle = sv.substr(beginPos, distance);

		// Split middle into bracket segments separated by "]-[".
		// One segment is the filename, the other is the counter pattern.
		// Handles nested brackets by tracking bracket depth.
		auto isCounter = [](std::string_view seg) -> bool
		{
			return !seg.empty() &&
				std::all_of(seg.begin(), seg.end(), [](char c)
				{
					return std::isdigit(c) || c == '/' || c == '_';
				});
		};

		std::string result;
		{
			size_t pos = 0;
			while (pos < middle.size())
			{
				if (middle[pos] != '[') break;

				// Find the matching ']' tracking bracket depth
				int depth = 1;
				size_t closeBracket = pos + 1;
				while (closeBracket < middle.size() && depth > 0)
				{
					if (middle[closeBracket] == '[') depth++;
					else if (middle[closeBracket] == ']') depth--;
					if (depth > 0) closeBracket++;
				}
				if (depth != 0) break;

				std::string_view segment = middle.substr(pos + 1, closeBracket - pos - 1);
				if (!isCounter(segment))
				{
					size_t lastSep = segment.rfind('/');
					size_t lastBackslash = segment.rfind('\\');
					size_t start = 0;
					if (lastBackslash != std::string_view::npos)
						start = lastBackslash + 1;
					if (lastSep != std::string_view::npos && lastSep + 1 > start)
						start = lastSep + 1;

					result = std::string(segment.substr(start));
					break;
				}

				pos = closeBracket + 1;
				if (pos + 1 < middle.size() && middle[pos] == '-' && middle[pos + 1] == '[')
				{
					pos += 1;
				}
			}
		}

		return result;
	}
}

namespace Deobfuscation
{
	/**
	 * @brief Counter-heuristic to check if a filename is likely legitimate (not a random hash).
	 * 
	 * Currently checks for the presence of spaces, which are common in human-readable
	 * filenames but virtually never present in automated obfuscation hashes.
	 */
	bool IsProbablyLegitimateFilename(std::string_view str)
	{
		if (str.find(' ') != std::string_view::npos)
			return true;

		return false;
	}

	/**
	 * @brief Checks if a filename matches known obfuscation hash patterns.
	 * 
	 * Matches against a list of common obfuscation regexes (e.g., 16+ char hex, 14+ char alphanumeric).
	 * Excludes standard archive extensions (.rar, .zip, etc.) and applies the
	 * `IsProbablyLegitimateFilename` counter-heuristic to prevent false positives.
	 */
	bool IsExcessivelyObfuscated(const std::string& str)
	{
		if (str.empty())
			return false;

		if (auto ext = FileSystem::GetFileExtension(str))
		{
			std::string_view extView(*ext);
			std::optional<std::string> nestedExt;

			// If it's a numeric split volume (e.g., .001, .15), check the nested extension
			if (extView.size() >= 2 && extView[0] == '.' &&
				std::all_of(extView.begin() + 1, extView.end(), [](unsigned char c) { return std::isdigit(c); }))
			{
				std::string base = str.substr(0, str.size() - extView.size());
				nestedExt = FileSystem::GetFileExtension(base);
				if (nestedExt)
				{
					extView = *nestedExt;
				}
			}

			if (FileTypes::IsArchiveExt(extView) || FileTypes::IsParityExt(extView) ||
				FileTypes::IsDiscStructureExt(extView))
			{
				return false;
			}
		}

		for (const auto& regex : HASHED_RELEASES_REGEXES)
		{
			if (std::regex_search(str, regex))
			{
				if (IsProbablyLegitimateFilename(str))
				{
					return false;
				}

				return true;
			}
		}

		return false;
	}

	/**
	 * @brief Main entry point to extract a clean filename from a Usenet subject line.
	 * 
	 * 1. Strips leading bracket tags (e.g., "[N3wZ] ").
	 * 2. If the subject contains quotes, extracts the quoted filename.
	 * 3. If quotes are empty (""), dispatches to the `[PRiVATE]` parser.
	 * 4. If no quotes exist, dispatches to the quote-less parser.
	 */
	std::string Deobfuscate(const std::string& str)
	{
		if (str.size() < 3) 
			return str;

		size_t contentStart = 0;
		while (contentStart < str.size() && str[contentStart] == '[')
		{
			size_t closeBracket = str.find(']', contentStart);
			if (closeBracket != std::string::npos && closeBracket + 1 < str.size() && str[closeBracket + 1] == ' ')
			{
				contentStart = closeBracket + 2;
			}
			else
			{
				break;
			}
		}

		std::string_view sv(str);
		sv.remove_prefix(contentStart);

		if (sv.size() < 3)
			return std::string(sv);

		size_t firstQuotPos = sv.find("\"");

		if (firstQuotPos == std::string_view::npos)
			return ParseWithoutQuotes(sv);

		firstQuotPos += 1;
		size_t secondQuotPos = sv.find("\"", firstQuotPos);
		size_t distance = secondQuotPos - firstQuotPos;

		if (distance == 0)
			return ParsePRiVATEnzb(sv);

		return std::string(sv.substr(firstQuotPos, distance));
	}
}
