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


#ifndef RELEASENAME_H
#define RELEASENAME_H

#include <set>
#include <string>
#include <vector>

/*
 * Release names as indexers list them: "Show.S01E02.Title.2160p.WEB-DL.DDP5.1.H.265-GRP",
 * "Show S01E02 Title 2160p WEB-DL DDP5 1 H 265-GRP", "...-GRP.mkv", "[01/17] \"...-GRP.par2\"".
 * Used by the duplicate search (option <DupeSearch>) to decide whether a
 * search result is another posting of the same release: the same title,
 * season and episode, release group, repack/proper flag and HDR format, and no
 * conflicting resolution, source, codec, bit depth, audio, channels, network
 * or edition. A value only one of two names carries is not a conflict.
 */
class ReleaseName
{
public:
	struct Attrs
	{
		std::set<std::string> resolution;
		std::set<std::string> quality;
		std::set<std::string> codec;
		std::set<std::string> bitDepth;
		std::set<std::string> audio;
		std::set<std::string> channels;
		std::set<std::string> network;
		std::set<std::string> edition;
		std::set<std::string> hdr;		// a name without an HDR tag is SDR
		std::string title;				// lowercase letters and digits only
		std::string group;				// lowercase
		std::vector<int> seasons;
		std::vector<int> episodes;
		int year = 0;
		bool repack = false;
		bool proper = false;
	};

	/* the name without extensions, volume suffixes, [tags] and {tags} and
	 * indexer junk ("-xpost", "-obfuscated", ...) */
	static std::string Clean(const std::string& name);

	/* the lowercase clean name with every run of separators turned into a dot */
	static std::string Normalize(const std::string& name);

	/* "lucifer s02e14 1080p": the title up to the season/episode or year, plus
	 * the resolution; empty when the name has neither marker */
	static std::string ShortQuery(const std::string& title);

	static Attrs Parse(const std::string& name);

	/* does the name carry release information, i.e. is it not obfuscated? */
	static bool Readable(const std::string& name);

	static bool SameRelease(const std::string& a, const std::string& b);
};

#endif
