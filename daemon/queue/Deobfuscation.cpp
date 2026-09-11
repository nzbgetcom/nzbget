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
	constexpr size_t MAX_PLAUSIBLE_EXT_LEN = 4;
	constexpr size_t MIN_DEOBFUSCATE_SIZE = 3;
	constexpr size_t RE_PREFIX_LEN = 4;
	constexpr size_t MIN_ALNUM_HASH_LEN = 12;
	constexpr size_t MIN_CAPS_HASH_LEN = 10;
	constexpr size_t MAX_MOVIE_TITLE_LEN = 15;
	constexpr size_t MIN_ALPHA_RUN_HASH_LEN = 24;
	constexpr size_t MIN_NUMERIC_HASH_LEN = 16;
	constexpr size_t MIN_INTERIOR_CAPS_COUNT = 3;
	constexpr size_t MIN_CASE_TRANSITIONS_FOR_HASH = 3;

	static const std::regex TOKEN_SPLIT_REGEX{ R"([^._\- ]+)" };
	static const std::regex EXCLUDED_MULTIPART_REGEX{ 
		R"((part\d+\.(rar|par2)$)|(vol\d+\+\d+\.par2$))", std::regex::icase 
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

	bool LooksLikeCamelCaseTitle(std::string_view tok)
	{
		if (tok.empty()) return false;

		static constexpr std::string_view LAST_CENTURY = "19";    // 1900s release year prefix
		static constexpr std::string_view CURRENT_CENTURY = "20"; // 2000s release year prefix
		static constexpr std::string_view CONNECTORS[] = {
			"On", "In", "To", "Of", "At", "By", "No"
		};
		static constexpr std::string_view ROMAN_NUMERALS[] = {
			"XXX", "XXIII", "XXII", "XXI", "XX",
			"XIX", "XVIII", "XVII", "XVI", "XV", "XIV", "XIII", "XII", "XI", "X",
			"IX", "VIII", "VII", "VI", "V", "IV", "III", "II", "I"
		};

		auto isYearFn = [](std::string_view s)
		{
			return s.size() == 4 && (s.starts_with(LAST_CENTURY) || s.starts_with(CURRENT_CENTURY));
		};

		auto matchWordFn = [&](size_t p) -> size_t
		{
			if (p >= tok.size() || !std::isupper(static_cast<unsigned char>(tok[p])))
				return std::string_view::npos;

			// Check 2-letter title prepositions ("On", "In", "To", etc.)
			for (std::string_view conn : CONNECTORS)
			{
				if (tok.substr(p).starts_with(conn))
				{
					// If followed by lowercase, it belongs to a longer word (e.g. "Into", "Only")
					if (p + 2 < tok.size() && std::islower(static_cast<unsigned char>(tok[p + 2])))
						break;
					return p + 2;
				}
			}

			// Standard title word: 1 uppercase followed by at least 2 lowercase letters
			size_t end = p + 1;
			while (end < tok.size() && std::islower(static_cast<unsigned char>(tok[end])))
				++end;

			return (end - p >= 3) ? end : std::string_view::npos;
		};

		// Title must start with a capitalized word or preposition
		size_t pos = matchWordFn(0);
		if (pos == std::string_view::npos) return false;

		// Consume intermediate segments: optional digits followed by a title word
		while (pos < tok.size())
		{
			size_t next = pos;
			while (next < tok.size() && std::isdigit(static_cast<unsigned char>(tok[next])))
				++next;

			size_t digitCount = next - pos;
			if (digitCount > 0)
			{
				std::string_view digits = tok.substr(pos, digitCount);
				// Only 1-2 digits or 4-digit release years allowed between words
				if (!isYearFn(digits) && digitCount > 2)
					break;
			}

			size_t wordEnd = matchWordFn(next);
			if (wordEnd == std::string_view::npos)
				break;
			pos = wordEnd;
		}

		if (pos == tok.size()) return true;

		// Check optional suffix: 1-3 digits (sequel number), 4-digit release year, or Roman numeral
		std::string_view tail = tok.substr(pos);
		bool isNumeric = std::all_of(tail.begin(), tail.end(),
			[](char c) { return std::isdigit(static_cast<unsigned char>(c)); });

		if (isNumeric)
		{
			if (tail.size() >= 1 && tail.size() <= 3)
				return true;
			if (isYearFn(tail))
				return true;
		}

		for (std::string_view roman : ROMAN_NUMERALS)
		{
			if (tail == roman) return true;
		}

		return false;
	}

	constexpr bool IsVowel(char c)
	{
		return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y' ||
			c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'Y';
	}

	size_t CountVowels(std::string_view str)
	{
		return static_cast<size_t>(std::count_if(str.begin(), str.end(), IsVowel));
	}

	bool LooksLikeHashBlob(std::string_view tok)
	{
		if (tok.empty()) return true;
		// Whitelist human-readable CamelCase titles before applying hash heuristics
		if (LooksLikeCamelCaseTitle(tok)) return false;

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

		// 1. Mixed alphanumeric token of 10+ chars with no punctuation (e.g. 5KzdcWdGVGUG83Q9jv8KXht4O2k57w)
		if (tok.size() >= MIN_ALNUM_HASH_LEN && alpha > 0 && digits > 0 &&
			alpha + digits == tok.size()) return true;

		// 2. Embedded 16+ digit numeric hash token (e.g. 1234567890123456)
		if (digits >= MIN_NUMERIC_HASH_LEN && digits == tok.size())
			return true;

		// 3. Whitelist short all-caps movie/show titles up to 15 chars with plausible vowels (e.g. INTERSTELLAR, OPPENHEIMER)
		if (tok.size() <= MAX_MOVIE_TITLE_LEN && alpha > 0 && digits == 0 && uppers == alpha)
		{
			// Allow short acronyms <= 3 chars (e.g. "TV", "DL", "HDR") or words with >= 20% vowels
			if (tok.size() <= 3 || CountVowels(tok) * 5 >= tok.size())
				return false;
		}

		// 4. Random interior-caps hash with 3+ uppercase letters inside (e.g. MQHeRbSCIoPs) or all-caps consonant string (e.g. BCDFGHJKLMNP)
		size_t interiorAllowed = MIN_INTERIOR_CAPS_COUNT + (std::isupper(static_cast<unsigned char>(tok.front())) ? 1 : 0);
		if (tok.size() >= MIN_CAPS_HASH_LEN && digits == 0 && uppers >= interiorAllowed)
		{
			if (uppers == alpha)
			{
				if (CountVowels(tok) == 0) return true;
			}
			else
			{
				size_t caseTransitions = 0;
				for (size_t i = 1; i < tok.size(); ++i)
				{
					bool prevUpper = std::isupper(static_cast<unsigned char>(tok[i - 1]));
					bool currUpper = std::isupper(static_cast<unsigned char>(tok[i]));
					if (prevUpper != currUpper)
					{
						++caseTransitions;
					}
				}
				if (caseTransitions >= MIN_CASE_TRANSITIONS_FOR_HASH) return true;
			}
		}

		// 5. Long single-case alphabetic hash with < 20% vowels (differentiates random hashes from real words)
		if (tok.size() >= MIN_ALPHA_RUN_HASH_LEN && alpha == tok.size() &&
			(lowers == alpha || uppers == alpha))
		{
			if (CountVowels(tok) * 5 < tok.size())
				return true;
		}
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

			// Preserve single-part archive/parity files so unrar/par2 can extract them
			if (maxRun >= 16 && maxRun <= 256 &&
				(FileTypes::IsSevenZipExt(ext.real) ||
					FileTypes::IsRarExt(ext.real) ||
					FileTypes::IsRarVolumeExt(ext.real) ||
					FileTypes::IsParityExt(ext.real)))
			{
				return false;
			}
		}

		// Preserve multipart archives (part01.rar, vol01.par2) so file sequences remain intact for extraction
		if (std::regex_search(str.begin(), str.end(), EXCLUDED_MULTIPART_REGEX))
		{
			return false;
		}

		std::string stem(StripOneExtension(str));
		std::string_view stemView(stem);
		// Check known hashed release patterns against the entire stem
		for (const auto& rx : HASHED_RELEASES_REGEXES)
		{
			if (std::regex_search(stem, rx)) return true;
		}

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

		auto tokBegin = std::cregex_iterator(stem.data(), stem.data() + stem.size(), TOKEN_SPLIT_REGEX);
		auto tokEnd = std::cregex_iterator();
		std::string_view prevTok{};

		// Check individual tokens and split adjacent mixed-case tokens against hash heuristics
		for (auto it = tokBegin; it != tokEnd; ++it)
		{
			std::string_view tok = stemView.substr(static_cast<size_t>(it->position()), static_cast<size_t>(it->length()));
			if (LooksLikeHashBlob(tok)) return true;

			if (!prevTok.empty() && stem.size() <= MAX_TITLE_LEN && isMixed(prevTok) && isMixed(tok))
			{
				std::string concatenated;
				concatenated.reserve(prevTok.size() + tok.size());
				concatenated.append(prevTok).append(tok);
				if (LooksLikeHashBlob(concatenated)) return true;
			}
			prevTok = tok;
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
			return std::string(sv.substr(firstQuotPos + 1));

		size_t secondQuotPos = sv.find('"', firstQuotPos + 1);
		if (secondQuotPos == std::string_view::npos)
			return std::string(sv.substr(firstQuotPos + 1));

		size_t distance = secondQuotPos - firstQuotPos - 1;
		// Empty quotes indicate [PRiVATE]-[signature] formatted releases
		if (distance == 0)
			return ParsePRiVATEnzb(sv);

		return std::string(sv.substr(firstQuotPos + 1, distance));
	}
}
