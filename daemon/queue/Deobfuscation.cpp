/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2024-2026 Denis <denis@nzbget.com>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"

#include "Deobfuscation.h"
#include "FileTypes.h"
#include "FileSystem.h"

namespace
{
	constexpr size_t MAX_TITLE_LEN = 32;
	constexpr size_t MAX_CAMEL_TOKEN_LEN = 64;
	constexpr size_t MAX_PLAUSIBLE_EXT_LEN = 4;
	constexpr size_t MIN_DEOBFUSCATE_SIZE = 3;
	constexpr size_t RE_PREFIX_LEN = 4;
	constexpr size_t MIN_ALNUM_HASH_LEN = 10;
	constexpr size_t MIN_CAPS_HASH_LEN = 10;
	constexpr size_t MAX_MOVIE_TITLE_LEN = 15;
	constexpr size_t MIN_ALPHA_RUN_HASH_LEN = 16;
	constexpr size_t MIN_INTERIOR_CAPS_COUNT = 3;
	constexpr size_t EVASION_TOKEN_TARGET_COUNT = 2;

	static const std::regex TOKEN_SPLIT_REGEX{ R"([^._\- ]+)" };
	static const std::regex EXCLUDED_MULTIPART_REGEX{ 
		R"((part\d+\.(rar|par2)$)|(vol\d+\+\d+\.par2$))", std::regex::icase 
	};
	static const std::regex CAMEL_CASE_REGEX{
		R"(^(?:[A-Z][a-z]{2,}|On|In|To|Of|At|By|No)(?:(?:(?:19|20)\d{2}|\d{1,2})?(?:[A-Z][a-z]{2,}|On|In|To|Of|At|By|No))*(?:(?:19|20)\d{2}|\d{1,3}|XXX|XXI{0,3}|XIX|XIV|XVI{0,3}|XI{0,3}|IX|VI{0,3}|IV|I{1,3})?$)"
	};
	static const std::array<std::regex, 11> HASHED_RELEASES_REGEXES{
		std::regex{ "^[0-9a-zA-Z]{24,}" },
		std::regex{ "^[a-z0-9]{16,256}$" },
		std::regex{ "^[0-9]{16,}$" },
		std::regex{ "^abc$", std::regex::icase },
		std::regex{ "^abc[-_. ]xyz", std::regex::icase },
		std::regex{ "^123$" },
		std::regex{ "^b00bs$", std::regex::icase },
		std::regex{ "^Backup_[0-9]{5,256}S[0-9]{2}-[0-9]{2}$" },
		std::regex{ "^[0-9]{6}_[0-9]{2}$" },
		std::regex{ "^[A-Z]{11}[0-9]{3}$" },
		std::regex{ "^[a-z]{12}[0-9]{3}$" },
	};

	std::string_view StripOneExtension(std::string_view str)
	{
		size_t dot = str.rfind('.');
		if (dot == std::string_view::npos) return str;
		std::string_view ext = str.substr(dot + 1);
		if (ext.empty() || ext.size() > MAX_PLAUSIBLE_EXT_LEN) return str;
		if (!std::any_of(ext.begin(), ext.end(),
			[](unsigned char c) { return std::isalpha(c); })) return str;
		return str.substr(0, dot);
	}

	struct ExtensionInfo
	{
		std::string raw;
		std::string real;
	};

	ExtensionInfo GetExtensionInfo(std::string_view str)
	{
		ExtensionInfo info;
		info.raw = FileSystem::GetFileExtension(str).value_or("");
		if (info.raw.empty())
		{
			return info;
		}

		info.real = info.raw;
		if (info.raw.size() > 1 && info.raw[0] == '.' &&
			std::all_of(info.raw.begin() + 1, info.raw.end(),
				[](unsigned char c) { return std::isdigit(c); }))
		{
			std::string_view before = str.substr(0, str.size() - info.raw.size());
			info.real = FileSystem::GetFileExtension(before).value_or("");
		}

		return info;
	}

	bool LooksLikeHashBlob(std::string_view tok)
	{
		if (tok.empty()) return true;
		if (tok.size() <= MAX_CAMEL_TOKEN_LEN && std::regex_match(tok.begin(), tok.end(), CAMEL_CASE_REGEX))
			return false;

		size_t alpha = 0, digits = 0, uppers = 0, lowers = 0;
		for (char c : tok)
		{
			auto uc = static_cast<unsigned char>(c);
			if (std::isalpha(uc))
			{
				++alpha;
				if (std::isupper(uc)) ++uppers; else ++lowers;
			}
			if (std::isdigit(uc)) ++digits;
		}

		if (tok.size() >= MIN_ALNUM_HASH_LEN && alpha > 0 && digits > 0 &&
			alpha + digits == tok.size()) return true;
		if (tok.size() <= MAX_MOVIE_TITLE_LEN && alpha > 0 && digits == 0 &&
			uppers == alpha) return false;

		size_t interiorAllowed = MIN_INTERIOR_CAPS_COUNT + (std::isupper(static_cast<unsigned char>(tok.front())) ? 1 : 0);
		if (tok.size() >= MIN_CAPS_HASH_LEN && digits == 0 && uppers >= interiorAllowed)
			return true;
		if (tok.size() >= MIN_ALPHA_RUN_HASH_LEN && alpha == tok.size() &&
			(lowers == alpha || uppers == alpha)) return true;
		return false;
	}

	std::string_view ParseWithoutQuotes(std::string_view sv)
	{
		size_t end = sv.find(" yEnc");
		if (end != std::string_view::npos)
		{
			if (end == 0) return sv;
			size_t start = sv.find_last_of(' ', end - 1);
			if (start == std::string_view::npos)
				return sv.substr(0, end);

			start += 1;
			return (start < end) ? sv.substr(start, end - start) : sv;
		}

		size_t start = sv.find("Re: ");
		end = sv.rfind(" (");

		if (start != std::string_view::npos)
		{
			start += RE_PREFIX_LEN;
			if (end != std::string_view::npos && start < end)
				return sv.substr(start, end - start);
			return sv.substr(start);
		}

		if (end != std::string_view::npos)
		{
			return sv.substr(0, end);
		}

		return sv;
	}

	std::string ParsePRiVATEnzb(std::string_view sv)
	{
		constexpr std::string_view signature = "[PRiVATE]-[";
		constexpr std::string_view endOfSignature = "]-";
		constexpr std::string_view endMarker = " - \"\"";

		size_t beginPos = sv.find(signature);
		if (beginPos == std::string_view::npos)
			return std::string(sv);

		beginPos += signature.size();

		beginPos = sv.find("]-[", beginPos);
		if (beginPos == std::string_view::npos)
			return std::string(sv);

		beginPos += endOfSignature.size();

		size_t endPos = sv.rfind(endMarker);
		if (endPos == std::string_view::npos || endPos < beginPos)
		{
			return std::string(sv);
		}

		std::string_view middle = sv.substr(beginPos, endPos - beginPos);

		std::string result;
		result.reserve(middle.size());

		bool foundExtOrWord = false;
		int depth = 0;

		for (char ch : middle)
		{
			if (ch == '[' && ++depth == 1)
				continue;

			if (ch == ']' && --depth == 0)
				continue;

			if (foundExtOrWord && !depth)
				break;

			if (!foundExtOrWord && !depth)
			{
				result.clear();
				continue;
			}
			if (!depth && ch == '-')
				continue;

			if (depth == 1 && (ch == '/' || ch == '\\'))
			{
				result.clear();
				continue;
			}

			if (depth == 1 && (ch == '.' || std::isalpha(static_cast<unsigned char>(ch))))
				foundExtOrWord = true;

			result.push_back(ch);
		}

		return result;
	}
}

namespace Deobfuscation
{
	bool IsExcessivelyObfuscated(std::string_view str)
	{
		if (str.empty()) return false;

		auto ext = GetExtensionInfo(str);
		if (!ext.real.empty())
		{
			std::string_view beforeExt = str.substr(0, str.size() - ext.raw.size());
			if (!beforeExt.empty() && (beforeExt.back() == '.' ||
				beforeExt.back() == '_' || beforeExt.back() == '-' ||
				beforeExt.back() == ' '))
			{
				beforeExt.remove_suffix(1);
			}

			size_t maxRun = 0, curRun = 0;
			for (char c : beforeExt)
			{
				if (std::isalnum(static_cast<unsigned char>(c)))
					++curRun;
				else
					curRun = 0;
				maxRun = std::max(maxRun, curRun);
			}

			if (maxRun >= 16 && maxRun <= 256 &&
				(FileTypes::IsSevenZipExt(ext.real) ||
					FileTypes::IsRarExt(ext.real) ||
					FileTypes::IsRarVolumeExt(ext.real) ||
					FileTypes::IsParityExt(ext.real)))
			{
				return false;
			}
		}

		if (std::regex_search(str.begin(), str.end(), EXCLUDED_MULTIPART_REGEX))
		{
			return false;
		}

		std::string stem(StripOneExtension(str));
		std::string_view stemView(stem);
		for (const auto& rx : HASHED_RELEASES_REGEXES)
		{
			if (std::regex_search(stem, rx)) return true;
		}

		auto tokBegin = std::cregex_iterator(stem.data(), stem.data() + stem.size(), TOKEN_SPLIT_REGEX);
		auto tokEnd = std::cregex_iterator();
		size_t tokenCount = 0;
		std::array<std::string_view, EVASION_TOKEN_TARGET_COUNT> firstTwoTokens{};

		for (auto it = tokBegin; it != tokEnd; ++it)
		{
			std::string_view tok = stemView.substr(static_cast<size_t>(it->position()), static_cast<size_t>(it->length()));
			if (LooksLikeHashBlob(tok)) return true;

			if (tokenCount < EVASION_TOKEN_TARGET_COUNT) firstTwoTokens[tokenCount] = tok;
			++tokenCount;
		}

		if (tokenCount == EVASION_TOKEN_TARGET_COUNT && stem.size() <= MAX_TITLE_LEN)
		{
			auto isMixed = [](std::string_view tok)
			{
				bool hasUpper = false, hasLower = false;
				for (char c : tok)
				{
					if (std::isupper(static_cast<unsigned char>(c))) hasUpper = true;
					if (std::islower(static_cast<unsigned char>(c))) hasLower = true;
				}
				return hasUpper && hasLower;
			};

			if (isMixed(firstTwoTokens[0]) && isMixed(firstTwoTokens[1]))
			{
				std::string concatenated;
				concatenated.reserve(firstTwoTokens[0].size() + firstTwoTokens[1].size());
				concatenated.append(firstTwoTokens[0]).append(firstTwoTokens[1]);
				if (LooksLikeHashBlob(concatenated)) return true;
			}
		}

		return false;
	}

	std::string Deobfuscate(std::string_view str)
	{
		if (str.size() < MIN_DEOBFUSCATE_SIZE)
			return std::string(str);

		std::string_view sv(str);
		size_t firstQuotPos = sv.find('"');
		if (firstQuotPos == std::string_view::npos)
			return std::string(ParseWithoutQuotes(sv));

		if (firstQuotPos + 1 >= sv.size())
			return std::string(sv.substr(firstQuotPos));

		size_t secondQuotPos = sv.find('"', firstQuotPos + 1);
		if (secondQuotPos == std::string_view::npos)
			return std::string(sv.substr(firstQuotPos + 1));

		size_t distance = secondQuotPos - firstQuotPos - 1;
		if (distance == 0)
			return ParsePRiVATEnzb(sv);

		return std::string(sv.substr(firstQuotPos + 1, distance));
	}
}
