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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef POST_UNPACK_RENAMER_H
#define POST_UNPACK_RENAMER_H

#include "Thread.h"
#include "DownloadInfo.h"
#include <set>
#include <vector>
#include <string>
#include <string_view>

namespace PostUnpackRenamer
{

class Controller final : public Thread
{
public:
	static void StartJob(PostInfo* postInfo);
	void Run() override;

	int RenameFiles(PostInfo* postInfo);

	std::string ResolveSubtitleName(std::string_view metaname, std::string_view stem, std::string_view ext);
	std::string ResolveUniqueName(std::string_view metaname, std::string_view stem, std::string_view ext,
		std::string_view baseName, const std::set<fs::path>& usedPaths, const fs::path& destPath);

private:
	PostInfo* m_postInfo = nullptr;
	int m_renamedCount = 0;

	std::vector<fs::path> CollectCandidates(const fs::path& dir);
	void RenameCompleted();
};

}

#endif
