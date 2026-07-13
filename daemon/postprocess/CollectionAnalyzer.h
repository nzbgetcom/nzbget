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


#ifndef COLLECTION_ANALYZER_H
#define COLLECTION_ANALYZER_H

#include <string>
#include <vector>
#include <cstdint>
#include "PostDownloadRenamer.h"

namespace PostDownloadRenamer
{

class CollectionAnalyzer final
{
public:
	explicit CollectionAnalyzer(const std::vector<Candidate>& candidates);
	bool ShouldSkip(const Candidate& candidate) const;

private:
	struct FileGroup
	{
		fs::path parentDir;
		std::string extKey;
		uintmax_t largest = 0;
		uintmax_t second = 0;
		int count = 0;
		bool skip = false;
	};

	std::vector<FileGroup> m_groups;

	static std::vector<FileGroup> BuildGroups(const std::vector<Candidate>& candidates);
};

}

#endif
