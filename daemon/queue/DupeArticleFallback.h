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
 * The duplicate a file leads with rotates as well, independently of the
 * cutover: whenever duplicates are tried at all, the file's lead duplicate is
 * tried first, and when that lead misses enough articles in a row it is
 * demoted and the next duplicate takes over the lead for fresh articles, so a
 * holed lead does not cost a wasted fetch per article when other duplicates
 * could serve them. The rotation is bounded: once every duplicate has led
 * without a single lead success it stops (the duplicates are equally holed
 * and rotating - and logging - would be pure noise) until a lead success
 * resets it.
 *
 * An article's source order is PINNED at its first fallback (PinSources): the
 * donors known at that moment, rotated to the file's lead, primary revert
 * inserted when cut over. The fallback round indexes into that pinned list,
 * so neither a lead rotation nor a queue/history change while the article is
 * in flight can shift the round->source mapping under it (which would skip or
 * repeat sources). The lead is identified by donor nzb-id, never by list
 * position: a lead-round result only counts towards demotion while the pinned
 * slot-0 donor is still the file's lead, so a burst of stale failures cannot
 * cascade-demote donors that were never tried, and a donor's streak can never
 * be charged to another donor even when candidate lists differ per part.
 *
 * All methods must be called within DownloadQueue-lock.
 */
class DupeArticleFallback
{
public:
	// after this many articles of a file are recovered from duplicates, lead
	// with the duplicate for the file's remaining articles (cutover)
	static constexpr int CutoverThreshold = 3;
	// after this many consecutive articles missed by the lead duplicate, the
	// next duplicate becomes the lead for the file's fresh articles
	static constexpr int LeadDemoteThreshold = 3;

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
	 * Cut over: [top donor, remaining donors, primary revert last] so a
	 * cut-over file leads with the duplicate and tries the other duplicates
	 * before falling back to the proven-holed primary as the last resort. */
	static std::vector<CString> OrderSources(const std::vector<CString>& donorCandidates,
		bool cutover, const char* primaryMessageId);

	/* Rotates the donor list so the donor with the given nzb-id comes first
	 * (order among the others preserved); no-op when the id is 0 or absent. */
	static void RotateToLead(RawNzbList& donors, int leadNzbId);

	/* Pure pinning step: derives the article's lead bookkeeping (slot-0 donor,
	 * next-lead donor, distinct donor count) from the collected candidates and
	 * their contributing donor nzb-ids, lazily fixes the file's lead to the
	 * slot-0 donor when still undecided, and stores the ordered sources on the
	 * article. candidates/contributors run parallel (one contributor id per
	 * candidate, the donor that FIRST offered that message-id). */
	static void FinishPin(FileInfo* fileInfo, ArticleInfo* articleInfo,
		const std::vector<CString>& candidates, const std::vector<int>& contributors,
		bool cutover, const char* primaryMessageId);

	/* Counts a failed fetch of the article's pinned lead donor towards the
	 * file's consecutive lead-miss streak. Only a lead-round (round 1) result
	 * counts, and only while the pinned slot-0 donor is still the file's
	 * lead. Returns true when the streak reached LeadDemoteThreshold and the
	 * lead was rotated to the article's next donor (requires another donor
	 * and an unexhausted rotation budget; the caller logs the switch). Also
	 * called when a provisionally-accepted lead article is demoted later
	 * (QueueCoordinator::DemoteFinishedArticle): that fetch was in truth a
	 * lead miss and must not leave its bogus streak reset standing. */
	static bool RegisterLeadFailure(FileInfo* fileInfo, ArticleInfo* articleInfo);

	/* Ends the file's lead-miss streak after a successful fetch from the
	 * current lead (same gating as RegisterLeadFailure). The rotation budget
	 * is re-armed only when the success is verifiable (the article's decoded
	 * boundaries were pinned by finished neighbours): an article accepted
	 * provisionally may be demoted again later, and its bogus success must
	 * not bypass the rotation bound. */
	static void RegisterLeadSuccess(FileInfo* fileInfo, ArticleInfo* articleInfo);

	/* A lead donor deleted from queue/history would freeze the lead
	 * accounting for good: nzb-ids are never reused, so its id could never
	 * match a collected donor again and the snapshot gate would block
	 * RegisterLeadFailure/Success for every fresh pin. Treat such a lead as
	 * vacated - the next pin re-decides the lead and the accounting starts
	 * clean. */
	static void VacateGhostLead(FileInfo* fileInfo, const RawNzbList& donors);

	/* Parses a collection's retained queued .nzb from disk into a standalone
	 * NzbInfo (with article lists loaded back in server mode). Returns nullptr
	 * if the file is missing or unparseable. Unlike GetParsedDonor this does
	 * not use or fill the per-instance donor cache; safe to call without the
	 * DownloadQueue lock. */
	static std::unique_ptr<NzbInfo> ParseDonorNzb(const char* queuedFilename);

	/* Decoded byte offset the article must begin at, derived from currently
	 * known geometry: 0 for the first article of the file, the end of an
	 * already-finished preceding article otherwise. -1 = not derivable yet.
	 * Guards substituted donor articles against decoded-boundary drift: the
	 * structural gate compares posted (encoded) sizes only, so a donor with
	 * the same article count and total but non-uniform segmentation can pass
	 * it while its decoded boundaries drift at interior articles. */
	static int64 ExpectedSegmentOffset(FileInfo* fileInfo, ArticleInfo* articleInfo);

	/* Decoded byte offset the article must end at (exclusive): the begin of an
	 * already-finished following article, or the decoded file size for the last
	 * article of the file. -1 = not derivable yet. */
	static int64 ExpectedSegmentEnd(FileInfo* fileInfo, ArticleInfo* articleInfo);

	/* Verifies the decoded byte range recorded for a finished article
	 * (SegmentOffset/SegmentSize) exactly abuts the currently known geometry
	 * (see ExpectedSegmentOffset/ExpectedSegmentEnd). A substituted article
	 * failing this check would leave a zero-filled gap and/or overwrite a
	 * neighbour's bytes, so it must not count as a successful download. */
	static bool SegmentAligned(FileInfo* fileInfo, ArticleInfo* articleInfo);

	/* Whole-file decoded-boundary check, independent of the (non-persisted)
	 * fallback round. Returns the first finished article whose decoded range
	 * breaks the contiguous tiling of [0, DecodedFileSize) - the gap/overlap a
	 * mis-placed donor article leaves - or nullptr when the finished articles
	 * tile exactly OR the geometry cannot be judged (DecodedFileSize unknown /
	 * non-yEnc, an article without recorded placement, or an unfinished
	 * article). Meaningful only for a fully-downloaded yEnc file; used at
	 * completion to catch a provisionally-accepted drifted donor that survived
	 * a restart (reloaded as a plain finished article, so the round-gated
	 * checks no longer recognise it). Healthy yEnc parts tile by construction,
	 * so this never fires on legitimate downloads. */
	static ArticleInfo* FirstUntiledArticle(FileInfo* fileInfo);

private:
	// total declared size must match within 1/64 (~1.6%); the slack absorbs
	// differences in yEnc overhead (header line lengths, filenames)
	static constexpr int TotalSizeToleranceDiv = 64;
	// per-article declared size must match within 1/16 (~6%)
	static constexpr int PartSizeToleranceDiv = 16;
	// keep enough donors parsed to cover a whole dupe-set without re-parsing per
	// article; must comfortably exceed the number of duplicates of one release
	static constexpr int MaxCachedDonors = 16;

	std::map<int, std::unique_ptr<NzbInfo>> m_parsedDonors;
	std::set<int> m_badDonors;
	static bool StructureMatches(FileInfo* targetFile, FileInfo* donorFile);
	static void AppendDonorCandidate(std::vector<CString>& candidates,
		std::vector<int>& contributors, int donorNzbId,
		NzbInfo* parsedDonor, FileInfo* targetFile, int partNumber);
	static RawNzbList CollectDonors(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	void PinSources(DownloadQueue* downloadQueue, FileInfo* fileInfo, ArticleInfo* articleInfo);
	NzbInfo* GetParsedDonor(NzbInfo* donorNzb);
};

#endif
