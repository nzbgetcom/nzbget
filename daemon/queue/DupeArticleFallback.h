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


#ifndef DUPEARTICLEFALLBACK_H
#define DUPEARTICLEFALLBACK_H

#include <map>
#include <set>
#include <vector>
#include "NString.h"
#include "DownloadInfo.h"

/*
 * Recovers articles missing on all news servers from duplicate collections
 * (same title or dupe-key) found in download queue or history. The failed
 * article is requeued with the message-id of the equivalent article of the
 * duplicate; residual damage from slightly different segment boundaries is
 * left to par-repair. See option <DupeArticleFallback>.
 *
 * Once a file has recovered enough articles from duplicates, it "cuts over":
 * subsequent articles lead with the duplicate instead of failing on the
 * primary first (which would waste a full server sweep per article). The
 * primary remains a revert source in case the duplicate is the one missing
 * that particular article.
 *
 * All methods must be called within DownloadQueue-lock.
 */
class DupeArticleFallback
{
public:
	// after this many articles of a file are recovered from duplicates, lead
	// with the duplicate for the file's remaining articles (cutover)
	static constexpr int CutoverThreshold = 3;

	bool TryFallback(DownloadQueue* downloadQueue, FileInfo* fileInfo, ArticleInfo* articleInfo);

	/* Finds the file of the duplicate collection which corresponds to the target
	 * file: preferably by filename, otherwise by unambiguous structural identity
	 * (article count and sizes). Returns nullptr if no or multiple candidates. */
	static FileInfo* MatchDonorFile(FileInfo* targetFile, NzbInfo* donorNzb);
	static const char* FindDonorMessageId(FileInfo* donorFile, int partNumber);
	static bool SizesMatch(int64 size1, int64 size2, int div);
	static std::vector<CString> BuildCandidateMessageIds(
		const std::vector<NzbInfo*>& parsedDonors, FileInfo* targetFile, int partNumber);

	/* Orders the message-ids an article is tried against. Reactive (not cut
	 * over): donor candidates only (the primary was already tried normally).
	 * Cut over: [top donor, primary revert, remaining donors] so a cut-over
	 * file leads with the duplicate but still reverts to the primary quickly
	 * if the duplicate lacks that article. */
	static std::vector<CString> OrderSources(const std::vector<CString>& donorCandidates,
		bool cutover, const char* primaryMessageId);

private:
	// total declared size must match within 1/64 (~1.6%); the slack absorbs
	// differences in yEnc overhead (header line lengths, filenames)
	static constexpr int TotalSizeToleranceDiv = 64;
	// per-article declared size must match within 1/16 (~6%)
	static constexpr int PartSizeToleranceDiv = 16;
	static constexpr int MaxCachedDonors = 4;

	std::map<int, std::unique_ptr<NzbInfo>> m_parsedDonors;
	std::set<int> m_badDonors;

	static bool StructureMatches(FileInfo* targetFile, FileInfo* donorFile);
	std::vector<CString> CollectCandidateMessageIds(DownloadQueue* downloadQueue,
		FileInfo* fileInfo, ArticleInfo* articleInfo);
	NzbInfo* GetParsedDonor(NzbInfo* donorNzb);
};

#endif
