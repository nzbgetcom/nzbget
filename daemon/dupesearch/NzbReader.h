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


#ifndef NZBREADER_H
#define NZBREADER_H

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

/*
 * What the duplicate search needs to know about an nzb-file: how big it is,
 * which files it names, which articles (message-ids) it points at, and the
 * meta data (imdb, tvdb) it carries. A cheap summary instead of the full
 * NzbFile parse, which builds a download queue entry.
 */
struct NzbSummary
{
	int files = 0;
	long long totalBytes = 0;
	std::set<std::string> filenames;			// lowercase
	std::string poster;
	std::vector<std::string> messageIds;		// sorted, unique
	std::vector<std::string> groups;			// newsgroups, sorted, unique
	std::map<std::string, std::string> meta;	// lowercase type -> text
	std::string mainName;						// the largest file that isn't par2

	/* identifies the posting: a hash of its message-ids */
	std::string Fingerprint() const;
};

class NzbReader
{
public:
	/* false for a malformed nzb-file, one without files or articles, or one
	 * that declares XML entities */
	static bool Parse(const std::string& data, NzbSummary& summary);
};

#endif
