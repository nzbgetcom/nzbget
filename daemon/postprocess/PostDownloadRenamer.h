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


#ifndef POST_DOWNLOAD_RENAMER_H
#define POST_DOWNLOAD_RENAMER_H

#include "Thread.h"
#include "DownloadInfo.h"
#include <set>
#include <vector>
#include <string>
#include <string_view>

namespace PostDownloadRenamer
{

struct Candidate
{
	fs::path path;
	fs::path parentDir;
	std::string filename;
	std::string stem;
	std::string ext;
	std::string extLower;
	uintmax_t size = 0;
};

enum class Scope
{
	Downloaded,
	Extracted
};

struct RenameResult
{
	int downloadedCount = 0;
	int extractedCount = 0;
	bool downloadedRan = false;
	bool extractedRan = false;
};

std::vector<Candidate> CollectCandidates(Thread& thread, const fs::path& dir);
std::string ResolveSubtitleName(std::string_view metaname, std::string_view stem, std::string_view ext);
std::string ResolveUniqueName(std::string_view metaname, std::string_view stem, std::string_view ext,
	std::string_view baseName, const std::set<fs::path>& usedPaths, const fs::path& destPath);
int RenameFiles(Thread& thread, PostInfo* postInfo, Scope scope);
RenameResult RenameFiles(Thread& thread, PostInfo* postInfo);
int RenameFiles(Thread& thread, NzbInfo* nzbInfo, const std::string& metaname,
	const std::vector<Candidate>& candidates, bool includeDownloaded, std::set<fs::path>& usedPaths);
void FinishStage(Thread& thread, PostInfo* postInfo, const RenameResult& result, const char* infoName = nullptr);

class Controller final : public Thread
{
public:
	static void StartJob(PostInfo* postInfo);
	void Run() override;
	void SetInfoName(const char* infoName) { m_infoName = infoName; }
	const char* GetInfoName() { return m_infoName; }

private:
	PostInfo* m_postInfo = nullptr;
	CString m_infoName;
};

}

#endif
