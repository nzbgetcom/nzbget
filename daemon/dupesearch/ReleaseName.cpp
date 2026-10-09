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


#include "nzbget.h"

#include <algorithm>
#include <regex>
#include "ReleaseName.h"

#include "DupeUtil.h"

using DupeUtil::Lower;
using DupeUtil::Trim;

namespace
{

bool IsDigits(const std::string& text)
{
	return !text.empty() && std::all_of(text.begin(), text.end(),
		[](char ch) { return isdigit((unsigned char)ch) != 0; });
}

// a file extension, volume suffix or other tag indexers append to a name
const std::regex& ExtensionRegex()
{
	static const std::regex regex(
		"\\.(part\\d+\\.rar|vol\\d+\\+\\d+\\.par2|7z\\.\\d{3}|r\\d{2}|z\\d{2}|nzb|mkv|mp4|m4v|avi|ts|"
		"rar|par2|7z|zip|nfo|sfv|srr|srt|sub|idx|jpg|png|txt)$", std::regex::icase);
	return regex;
}

const std::regex& JunkRegex()
{
	static const std::regex regex(
		"([.\\-_ ](xpost|postbot|obfuscated|scrambled|asrequested|rp|rakuv\\w*|buymore|"
		"chamele0n|sample|repost))+$", std::regex::icase);
	return regex;
}

// words that follow a hyphen inside a name and are not a release group
bool NotAGroup(const std::string& word)
{
	static const std::set<std::string> words = { "hd", "ma", "dl", "rip", "ray", "dts", "hdr", "dv",
		"ac3", "eac3", "dd", "ddp", "x", "e", "es" };
	return word.size() < 2 && !isalpha((unsigned char)word[0]) || words.count(word) > 0;
}

}

std::string ReleaseName::Clean(const std::string& name)
{
	// std::regex recurses once per repeated character: an indexer's title of
	// a megabyte ran the thread out of stack. No release name is this long
	std::string text = Trim(name.substr(0, MaxNameLength), " \t\r\n");

	for (;;)
	{
		std::string before = text;
		std::smatch match;
		if (std::regex_search(text, match, ExtensionRegex()))
		{
			text.erase(match.position(0));
		}
		else if (text.size() > 4 && text[text.size() - 4] == '.' && IsDigits(text.substr(text.size() - 3)) &&
			tolower((unsigned char)text[text.size() - 5]) != 'h' &&
			tolower((unsigned char)text[text.size() - 5]) != 'x')
		{
			// a split volume ".001", but not the codec of "H.265"
			text.erase(text.size() - 4);
		}
		if (text == before)
		{
			break;
		}
	}

	// [tags] and {tags}
	std::string stripped;
	int depth = 0;
	char closer = 0;
	for (char ch : text)
	{
		if (depth == 0 && (ch == '[' || ch == '{'))
		{
			depth = 1;
			closer = ch == '[' ? ']' : '}';
			stripped += ' ';
		}
		else if (depth == 1)
		{
			if (ch == closer)
			{
				depth = 0;
			}
		}
		else
		{
			stripped += ch;
		}
	}
	if (depth == 1)
	{
		// an unclosed bracket is no tag
		stripped = text;
	}
	text = Trim(stripped, " \t\r\n");

	std::smatch junk;
	if (std::regex_search(text, junk, JunkRegex()))
	{
		text.erase(junk.position(0));
	}
	return Trim(text, " ._-");
}

std::string ReleaseName::Normalize(const std::string& name)
{
	std::string text = Lower(Clean(name));
	std::string out;
	bool separator = false;
	for (char ch : text)
	{
		if (isspace((unsigned char)ch) || strchr("._-()+,", ch))
		{
			separator = true;
			continue;
		}
		if (separator && !out.empty())
		{
			out += '.';
		}
		separator = false;
		out += ch;
	}
	return out;
}

namespace
{

bool IsEpisodeToken(const std::string& token)
{
	// s02e14, s02e14e15, s02
	static const std::regex regex("^s\\d{1,3}(e\\d{1,4})*$");
	return std::regex_match(token, regex) && token.size() > 1;
}

bool IsYearToken(const std::string& token)
{
	return token.size() == 4 && IsDigits(token) && (token.compare(0, 2, "19") == 0 || token.compare(0, 2, "20") == 0);
}

bool IsResolutionToken(const std::string& token)
{
	static const std::regex regex("^(\\d{3,4})[pi]$|^4k$|^uhd$|^fhd$");
	return std::regex_match(token, regex);
}

}

std::string ReleaseName::ShortQuery(const std::string& title)
{
	std::string normalized = Normalize(title);
	std::vector<std::string> tokens;
	size_t start = 0;
	while (start <= normalized.size())
	{
		size_t end = normalized.find('.', start);
		if (end == std::string::npos)
		{
			end = normalized.size();
		}
		tokens.push_back(normalized.substr(start, end - start));
		start = end + 1;
	}

	size_t cut = tokens.size();
	for (size_t i = 0; i < tokens.size(); i++)
	{
		if ((IsEpisodeToken(tokens[i]) && tokens[i].find('e') != std::string::npos) || IsYearToken(tokens[i]))
		{
			cut = i;
			break;
		}
	}
	if (cut == tokens.size())
	{
		return "";
	}

	std::string query;
	for (size_t i = 0; i <= cut; i++)
	{
		query += (i ? " " : "") + tokens[i];
	}
	for (size_t i = cut + 1; i < tokens.size(); i++)
	{
		static const std::regex resolution("^(\\d{3,4}p|4k|uhd)$");
		if (std::regex_match(tokens[i], resolution))
		{
			query += " " + tokens[i];
			break;
		}
	}
	return query;
}

ReleaseName::Attrs ReleaseName::Parse(const std::string& name)
{
	Attrs attrs;
	std::string clean = Clean(name);

	// "S01E01-E03" and "S01E01-03" are episodes 1, 2 and 3: written as "S01E01E02E03",
	// before the release group is looked for (its hyphen isn't the group's). Only a
	// forward range of at most 50 episodes: "S01E01-2023" keeps its year
	static const std::regex episodeRange("([sS]\\d{1,3}(?:[eE]\\d{1,4})*[eE](\\d{1,4}))-[eE]?(\\d{1,4})(?![0-9A-Za-z])");
	std::string rewritten;
	std::string rest = clean;
	for (std::smatch range; std::regex_search(rest, range, episodeRange);)
	{
		int first = atoi(range[2].str().c_str());
		int last = atoi(range[3].str().c_str());
		rewritten += range.prefix().str();
		if (first < last && last - first <= 50)
		{
			rewritten += range[1].str();
			for (int episode = first + 1; episode <= last; episode++)
			{
				rewritten += "e" + std::to_string(episode);
			}
		}
		else
		{
			rewritten += range.str();
		}
		rest = range.suffix().str();
	}
	clean = rewritten + rest;

	// the release group follows the last hyphen
	std::string body = clean;
	size_t hyphen = clean.rfind('-');
	if (hyphen != std::string::npos)
	{
		std::string tail = Trim(clean.substr(hyphen + 1), "\"' ");
		std::string lowerTail = Lower(tail);
		if (!tail.empty() && tail.find_first_of(" .\"'") == std::string::npos && !NotAGroup(lowerTail))
		{
			attrs.group = lowerTail;
			body = clean.substr(0, hyphen);
		}
		else if (tail.empty())
		{
			body = clean.substr(0, hyphen);
		}
	}

	// spellings that contain separators become one word
	std::string text = Lower(body);
	static const std::vector<std::pair<std::string, std::string>> joins = {
		{ "hdr10+", "hdr10plus" }, { "dd+", "ddp" }, { "e-ac-3", "eac3" }, { "e-ac3", "eac3" },
		{ "blu-ray", "bluray" }, { "web-dl", "webdl" }, { "dts-hd", "dtshd" }, { "dts-x", "dtsx" },
		{ "dts-es", "dtses" }, { "ac-3", "ac3" }, { "dolby vision", "dovi" }, { "dolby.vision", "dovi" },
		{ "dolby-vision", "dovi" }, { "dolby_vision", "dovi" }, { "dolbyvision", "dovi" },
		{ "hdr 10", "hdr10" }, { "web dl", "webdl" }, { "dts hd", "dtshd" }, { "blu ray", "bluray" } };
	for (const auto& join : joins)
	{
		for (size_t pos = text.find(join.first); pos != std::string::npos; pos = text.find(join.first, pos + join.second.size()))
		{
			text.replace(pos, join.first.size(), join.second);
		}
	}

	std::vector<std::string> tokens;
	std::string token;
	for (char ch : text)
	{
		if (isalnum((unsigned char)ch))
		{
			token += ch;
		}
		else if (!token.empty())
		{
			tokens.push_back(token);
			token.clear();
		}
	}
	if (!token.empty())
	{
		tokens.push_back(token);
	}

	static const std::regex audioRegex("^(ddpa|ddp|dd|eac3|ac3|aac|dtshd|dtsx|dtses|dts|truehd|flac|opus|mp3|lpcm|pcm)(\\d?)$");
	static const std::regex bitRegex("^(\\d{1,2})bit$|^hi10p?$");
	static const std::regex seasonEpisode("^s(\\d{1,3})((?:e\\d{1,4})*)$");
	static const std::regex altEpisode("^(\\d{1,2})x(\\d{2,3})$");
	static const std::regex repackRegex("^(repack|rerip)\\d?$");
	static const std::regex properRegex("^proper\\d?$");
	static const std::set<std::string> networks = { "amzn", "nf", "max", "atvp", "dsnp", "hulu", "pcok", "pmtp",
		"cr", "stan", "itv", "bbc", "red", "sho", "hbo", "pmnt", "ctv", "crav", "nbc", "abc", "cbs", "ifc" };
	static const std::set<std::string> editions = { "extended", "unrated", "uncut", "remastered", "theatrical",
		"imax", "criterion", "hybrid", "dc" };

	bool titleDone = false;
	std::string title;
	int lastAudio = -100;
	for (size_t i = 0; i < tokens.size(); i++)
	{
		const std::string& tok = tokens[i];
		const std::string next = i + 1 < tokens.size() ? tokens[i + 1] : "";
		std::smatch match;
		bool marker = true;

		if (std::regex_match(tok, match, seasonEpisode) && tok.size() > 1)
		{
			attrs.seasons.push_back(atoi(match[1].str().c_str()));
			std::string episodes = match[2].str();
			for (size_t pos = 0; pos < episodes.size();)
			{
				size_t end = episodes.find('e', pos + 1);
				attrs.episodes.push_back(atoi(episodes.substr(pos + 1, end == std::string::npos ? end : end - pos - 1).c_str()));
				pos = end == std::string::npos ? episodes.size() : end;
			}
		}
		else if (std::regex_match(tok, match, altEpisode))
		{
			attrs.seasons.push_back(atoi(match[1].str().c_str()));
			attrs.episodes.push_back(atoi(match[2].str().c_str()));
		}
		else if (IsYearToken(tok) && title.empty() && !titleDone)
		{
			// "2001.A.Space.Odyssey.1968", "1917.2019", "2012.1080p": a title can start with
			// a year-like number; the release year, if any, follows it
			marker = false;
		}
		else if (IsYearToken(tok))
		{
			if (!attrs.year)
			{
				attrs.year = atoi(tok.c_str());
			}
			else
			{
				marker = false;
			}
		}
		else if (IsResolutionToken(tok))
		{
			if (tok == "4k" || tok == "uhd")
			{
				attrs.resolution.insert("2160p");
			}
			else if (tok == "fhd")
			{
				attrs.resolution.insert("1080p");
			}
			else
			{
				attrs.resolution.insert(tok);	// 1080i is another encode than 1080p
			}
		}
		else if (tok == "webdl") { attrs.quality.insert("web"); attrs.quality.insert("dl"); }
		else if (tok == "web" || tok == "webhd") { attrs.quality.insert("web"); }
		else if (tok == "webrip") { attrs.quality.insert("webrip"); }
		else if (tok == "bluray" || tok == "bd") { attrs.quality.insert("bluray"); }
		else if (tok == "bdrip" || tok == "brrip") { attrs.quality.insert("bdrip"); }
		else if (tok == "remux") { attrs.quality.insert("remux"); }
		else if (tok == "hdtv") { attrs.quality.insert("hdtv"); }
		else if (tok == "dvdrip") { attrs.quality.insert("dvdrip"); }
		else if (tok == "hdrip") { attrs.quality.insert("hdrip"); }
		else if (tok == "hdlight") { attrs.quality.insert("hdlight"); }
		else if (tok == "x265" || tok == "h265" || tok == "hevc") { attrs.codec.insert("hevc"); }
		else if (tok == "x264" || tok == "h264" || tok == "avc") { attrs.codec.insert("avc"); }
		else if ((tok == "h" || tok == "x") && (next == "265" || next == "264"))
		{
			attrs.codec.insert(next == "265" ? "hevc" : "avc");
			i++;
		}
		else if (tok == "av1" || tok == "xvid" || tok == "vc1" || tok == "mpeg2") { attrs.codec.insert(tok); }
		else if (std::regex_match(tok, match, bitRegex))
		{
			attrs.bitDepth.insert(match[1].matched ? match[1].str() + "bit" : "10bit");
		}
		else if (std::regex_match(tok, match, audioRegex))
		{
			std::string kind = match[1].str();
			if (kind == "ddpa") { attrs.audio.insert("ddp"); attrs.audio.insert("atmos"); }
			else if (kind == "ddp" || kind == "eac3") { attrs.audio.insert("ddp"); }
			else if (kind == "dd" || kind == "ac3") { attrs.audio.insert("dd"); }
			else if (kind == "dtshd") { attrs.audio.insert("dts"); attrs.audio.insert("hd"); }
			else { attrs.audio.insert(kind); }
			lastAudio = (int)i;
			std::string digit = match[2].str();
			if (!digit.empty() && (next == "1" || next == "0"))
			{
				attrs.channels.insert(digit + "." + next);
				i++;
			}
		}
		else if (tok == "atmos") { attrs.audio.insert("atmos"); lastAudio = (int)i; }
		else if (tok == "ma" && (int)i - lastAudio <= 2) { attrs.audio.insert("ma"); lastAudio = (int)i; }
		else if (tok == "hd" && (int)i - lastAudio <= 1) { attrs.audio.insert("hd"); lastAudio = (int)i; }
		else if ((tok == "5" || tok == "7" || tok == "2" || tok == "6" || tok == "1") &&
			(next == "1" || next == "0") && (int)i - lastAudio <= 3)
		{
			attrs.channels.insert(tok + "." + next);
			i++;
		}
		else if (tok.size() == 3 && tok[1] == 'c' && tok[2] == 'h' && isdigit((unsigned char)tok[0]))
		{
			attrs.channels.insert(tok);
		}
		else if (tok == "dv" || tok == "dovi") { attrs.hdr.insert("dv"); }
		else if (tok == "hdr" || tok == "hdr10") { attrs.hdr.insert("hdr"); }
		else if (tok == "hdr10plus" || tok == "hdr10p") { attrs.hdr.insert("hdr10+"); }
		else if (tok == "hlg") { attrs.hdr.insert("hlg"); }
		else if (tok == "sdr") { /* a name without an HDR tag is SDR */ }
		else if (std::regex_match(tok, repackRegex)) { attrs.repack = true; }		// repack, repack2, rerip
		else if (std::regex_match(tok, properRegex)) { attrs.proper = true; }
		else if (tok == "netflix") { attrs.network.insert("nf"); }
		else if (tok == "amazon") { attrs.network.insert("amzn"); }
		else if (tok == "hmax") { attrs.network.insert("max"); }
		else if (networks.count(tok) && titleDone) { attrs.network.insert(tok); }
		else if (editions.count(tok) && titleDone) { attrs.edition.insert(tok); }
		else
		{
			marker = false;
		}

		if (marker)
		{
			titleDone = true;
		}
		else if (!titleDone)
		{
			title += tok;
		}
	}

	std::sort(attrs.seasons.begin(), attrs.seasons.end());
	std::sort(attrs.episodes.begin(), attrs.episodes.end());
	attrs.title = title;
	return attrs;
}

bool ReleaseName::Readable(const std::string& name)
{
	Attrs attrs = Parse(name);
	return !attrs.title.empty() && (!attrs.group.empty() || !attrs.resolution.empty());
}

namespace
{

bool Compatible(const std::set<std::string>& a, const std::set<std::string>& b)
{
	return a.empty() || b.empty() ||
		std::includes(b.begin(), b.end(), a.begin(), a.end()) ||
		std::includes(a.begin(), a.end(), b.begin(), b.end());
}

}

bool ReleaseName::SameRelease(const std::string& aName, const std::string& bName)
{
	Attrs a = Parse(aName);
	Attrs b = Parse(bName);

	if (a.group.empty())
	{
		// nothing to anchor on: require the same normalized name
		return Normalize(aName) == Normalize(bName);
	}

	if (a.title != b.title || a.group != b.group || a.seasons != b.seasons || a.episodes != b.episodes ||
		a.repack != b.repack || a.proper != b.proper || a.hdr != b.hdr)
	{
		return false;
	}
	if (a.year && b.year && a.year != b.year)
	{
		return false;
	}
	return Compatible(a.resolution, b.resolution) && Compatible(a.quality, b.quality) &&
		Compatible(a.codec, b.codec) && Compatible(a.bitDepth, b.bitDepth) &&
		Compatible(a.audio, b.audio) && Compatible(a.channels, b.channels) &&
		Compatible(a.network, b.network) && Compatible(a.edition, b.edition);
}
