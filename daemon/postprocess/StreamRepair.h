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


#ifndef STREAMREPAIR_H
#define STREAMREPAIR_H

#include <vector>
#include "NString.h"
#include "Thread.h"
#include "DownloadInfo.h"
#include "ScriptController.h"
#include "ArticleFetcher.h"

class DiskFile;

/*
 * Post-processing stage for option <DupeArticleFallback> value "stream":
 * fills the missing byte ranges of completed media files (captured as
 * StreamRepairJobs at download completion) with bytes fetched from duplicate
 * postings of the same content, regardless of how the duplicate was split
 * into articles. Content identity is verified by byte-comparing fetched
 * donor articles against already-downloaded regions before anything is
 * written; par-check remains the backstop for whatever stays missing.
 */
class StreamRepairController : public Thread, public ScriptController
{
public:
	virtual void Run();
	virtual void Stop();
	static void StartJob(PostInfo* postInfo);

protected:
	virtual void AddMessage(Message::EKind kind, const char* text);

private:
	struct RepairTarget
	{
		int FileId;
		CString Filename;		// current on-disk base name (in DestDir)
		int64 DecodedFileSize;
		StreamRangeList Holes;
	};

	struct DonorSource
	{
		CString QueuedFilename;
		CString InfoName;
	};

	PostInfo* m_postInfo;
	ArticleFetcher m_fetcher;
	int m_recoveredArticles = 0;
	int64 m_recoveredBytes = 0;
	bool m_holesRemain = false;

	void CollectTargets(NzbInfo* nzbInfo, std::vector<RepairTarget>& targets);
	void CollectDonors(DownloadQueue* downloadQueue, NzbInfo* nzbInfo,
		std::vector<DonorSource>& donors);
	void ExecRepair(const char* destDir, std::vector<RepairTarget>& targets,
		std::vector<DonorSource>& donors);
	bool RepairFile(const char* destDir, RepairTarget& target, NzbInfo* donorNzb,
		const char* donorName);
	static std::vector<FileInfo*> FindDonorFiles(const RepairTarget& target, NzbInfo* donorNzb);
	bool VerifyDonor(DiskFile& file, const RepairTarget& target, FileInfo* donorFile,
		const StreamRangeList& donorRanges);
	int PatchFromDonor(DiskFile& file, RepairTarget& target, FileInfo* donorFile,
		const StreamRangeList& donorRanges, const char* donorName);
	static bool CompareToFile(DiskFile& file, int64 offset, const char* data, int64 size);
	void RepairCompleted();
};

#endif
