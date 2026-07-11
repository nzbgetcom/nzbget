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


#include "nzbget.h"

#include <algorithm>
#include "DupeArticleFallback.h"
#include "DupeCoordinator.h"
#include "NzbFile.h"
#include "Options.h"
#include "DiskState.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"

bool DupeArticleFallback::TryFallback(DownloadQueue* downloadQueue, FileInfo* fileInfo, ArticleInfo* articleInfo)
{
	if (g_Options->GetDupeArticleFallback() == Options::dafNone || g_Options->GetRawArticle())
	{
		return false;
	}

	NzbInfo* nzbInfo = fileInfo->GetNzbInfo();
	if (fileInfo->GetDeleted() || nzbInfo->GetDeleting() || nzbInfo->GetParking() ||
		nzbInfo->GetDeleteStatus() != NzbInfo::dsNone || nzbInfo->GetDupeMode() == dmForce)
	{
		return false;
	}

	// preserve the article's own (primary) message-id before the first
	// substitution, so it can be used as a revert source when cut over
	if (Util::EmptyStr(articleInfo->GetDupeOriginalMessageId()))
	{
		articleInfo->SetDupeOriginalMessageId(articleInfo->GetMessageId());
	}

	int round = articleInfo->GetDupeFallbackRound();
	if (round == 0)
	{
		// the article's source order is decided once, at its first fallback:
		// the donors known now, rotated to the file's current lead, pinned on
		// the article so no later queue/history change or lead rotation can
		// shift the round->source mapping under it (which would skip or
		// repeat sources). The trade-off is deliberate: a duplicate appearing
		// later serves fresh articles, not articles already mid-fallback.
		PinSources(downloadQueue, fileInfo, articleInfo);
	}
	else if (round == 1 && RegisterLeadFailure(fileInfo, articleInfo))
	{
		nzbInfo->PrintMessage(Message::mkInfo,
			"Switching lead duplicate collection for %s (the lead duplicate is missing many articles)",
			fileInfo->GetFilename());
	}

	std::vector<CString>& sources = *articleInfo->GetDupeSources();
	if (round >= (int)sources.size())
	{
		return false;
	}

	// count this article once, the first time a duplicate source is tried for it
	// (the denominator of the per-file recovered/attempted completion summary).
	// No per-attempt log line: it would flood large files and, worse, read as a
	// success when it is only an attempt - the recovery is counted on success.
	if (round == 0)
	{
		fileInfo->SetDupeAttemptedArticles(fileInfo->GetDupeAttemptedArticles() + 1);
	}

	// pin the decoded byte range this article must occupy, as far as it is
	// already known from finished neighbour articles, so a donor article with
	// drifted decoded boundaries is rejected before its bytes are written
	articleInfo->SetDupeExpectedOffset(ExpectedSegmentOffset(fileInfo, articleInfo));
	articleInfo->SetDupeExpectedEnd(ExpectedSegmentEnd(fileInfo, articleInfo));

	articleInfo->SetMessageId(sources[round]);
	articleInfo->SetDupeFallbackRound(round + 1);

	return true;
}

std::vector<CString> DupeArticleFallback::OrderSources(const std::vector<CString>& donorCandidates,
	bool cutover, const char* primaryMessageId)
{
	std::vector<CString> sources;

	if (!cutover || donorCandidates.empty())
	{
		// reactive: the primary was already tried via the normal path; offer
		// the donor candidates only (empty if none, so the article just fails)
		for (const CString& candidate : donorCandidates)
		{
			sources.emplace_back((const char*)candidate);
		}
		return sources;
	}

	// cut over: lead with the top donor, then the remaining donors, then the
	// primary as the final fallback. The file cut over precisely because the
	// primary is missing many articles, so on a lead-donor miss the other
	// duplicates are better bets than paying a (likely failing) full server
	// sweep on the primary; the primary still backstops parts every duplicate
	// misses. NOTE: proactive fetches must never count as "recovered"
	// regardless of round (see QueueCoordinator::ArticleCompleted) - with the
	// primary last, no donor round proves the primary was missing the part
	sources.reserve(donorCandidates.size() + 1);
	for (const CString& candidate : donorCandidates)
	{
		sources.emplace_back((const char*)candidate);
	}
	sources.emplace_back(primaryMessageId);
	return sources;
}

void DupeArticleFallback::RotateToLead(RawNzbList& donors, int leadNzbId)
{
	if (leadNzbId == 0)
	{
		return;
	}

	RawNzbList::iterator lead = std::find_if(donors.begin(), donors.end(),
		[leadNzbId](NzbInfo* donor) { return donor->GetId() == leadNzbId; });
	if (lead != donors.end())
	{
		std::rotate(donors.begin(), lead, donors.end());
	}
}

void DupeArticleFallback::FinishPin(FileInfo* fileInfo, ArticleInfo* articleInfo,
	const std::vector<CString>& candidates, const std::vector<int>& contributors,
	bool cutover, const char* primaryMessageId)
{
	// the distinct donors behind the candidates, in candidate order: [0] is
	// the donor whose article the pinned slot 0 fetches, [1] the donor a
	// demotion would rotate the lead to
	std::vector<int> donorIds;
	for (int donorId : contributors)
	{
		if (std::find(donorIds.begin(), donorIds.end(), donorId) == donorIds.end())
		{
			donorIds.push_back(donorId);
		}
	}

	articleInfo->SetDupeLeadSnapshot(donorIds.empty() ? 0 : donorIds[0]);
	articleInfo->SetDupeNextLead(donorIds.size() > 1 ? donorIds[1] : 0);
	articleInfo->SetDupeDonorCount((int)donorIds.size());

	// the file's lead is decided by its first pinned article (the donors were
	// iterated in score order then, so this is the top-scored donor offering
	// that part); until rotated it stays this donor by nzb-id
	if (fileInfo->GetDupeLeadDonorId() == 0 && !donorIds.empty())
	{
		fileInfo->SetDupeLeadDonorId(donorIds[0]);
	}

	*articleInfo->GetDupeSources() = OrderSources(candidates, cutover, primaryMessageId);
}

bool DupeArticleFallback::RegisterLeadFailure(FileInfo* fileInfo, ArticleInfo* articleInfo)
{
	// only a failed fetch of the pinned lead donor counts, and only while that
	// donor is still the file's lead: late failures of an already demoted lead
	// reported by in-flight articles must not cascade-demote donors that were
	// never tried, and a part whose slot 0 belongs to ANOTHER donor (the lead
	// does not carry that part) must not charge the lead's streak
	if (articleInfo->GetDupeFallbackRound() != 1 ||
		articleInfo->GetDupeLeadSnapshot() == 0 ||
		articleInfo->GetDupeLeadSnapshot() != fileInfo->GetDupeLeadDonorId())
	{
		return false;
	}

	int failures = fileInfo->GetDupeLeadFailures() + 1;
	fileInfo->SetDupeLeadFailures(failures);

	// rotate only while another donor exists and the rotation budget lasts:
	// once every duplicate has led without a single lead success, the
	// duplicates are equally holed and further rotation (and its log line,
	// once per switch) would be pure noise
	if (failures >= LeadDemoteThreshold && articleInfo->GetDupeNextLead() != 0 &&
		fileInfo->GetDupeLeadSwitches() < articleInfo->GetDupeDonorCount())
	{
		fileInfo->SetDupeLeadDonorId(articleInfo->GetDupeNextLead());
		fileInfo->SetDupeLeadFailures(0);
		fileInfo->SetDupeLeadSwitches(fileInfo->GetDupeLeadSwitches() + 1);
		return true;
	}
	return false;
}

void DupeArticleFallback::RegisterLeadSuccess(FileInfo* fileInfo, ArticleInfo* articleInfo)
{
	if (articleInfo->GetDupeFallbackRound() != 1 ||
		articleInfo->GetDupeLeadSnapshot() == 0 ||
		articleInfo->GetDupeLeadSnapshot() != fileInfo->GetDupeLeadDonorId())
	{
		return;
	}

	fileInfo->SetDupeLeadFailures(0);

	// re-arm the rotation budget only when the success is verifiable: with
	// unfinished neighbours the alignment check passes vacuously, the article
	// may be demoted again later (DemoteMisalignedDupeNeighbors), and its
	// bogus success - although the streak reset is recharged at demotion -
	// must not bypass the rotation bound
	if (ExpectedSegmentOffset(fileInfo, articleInfo) != -1 &&
		ExpectedSegmentEnd(fileInfo, articleInfo) != -1)
	{
		fileInfo->SetDupeLeadSwitches(0);
	}
}

void DupeArticleFallback::VacateGhostLead(FileInfo* fileInfo, const RawNzbList& donors)
{
	int leadNzbId = fileInfo->GetDupeLeadDonorId();
	if (leadNzbId == 0 ||
		std::find_if(donors.begin(), donors.end(),
			[leadNzbId](NzbInfo* donor) { return donor->GetId() == leadNzbId; }) != donors.end())
	{
		return;
	}

	fileInfo->SetDupeLeadDonorId(0);
	fileInfo->SetDupeLeadFailures(0);
	fileInfo->SetDupeLeadSwitches(0);
}

RawNzbList DupeArticleFallback::CollectDonors(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
{
	RawNzbList donors;

	for (NzbInfo* queuedNzbInfo : downloadQueue->GetQueue())
	{
		if (queuedNzbInfo != nzbInfo &&
			queuedNzbInfo->GetKind() == NzbInfo::nkNzb &&
			queuedNzbInfo->GetDupeMode() != dmForce &&
			DupeCoordinator::SameNameOrKey(queuedNzbInfo->GetName(), queuedNzbInfo->GetDupeKey(),
				nzbInfo->GetName(), nzbInfo->GetDupeKey()))
		{
			donors.push_back(queuedNzbInfo);
		}
	}

	for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
	{
		if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
			historyInfo->GetNzbInfo()->GetDupeMode() != dmForce &&
			DupeCoordinator::SameNameOrKey(historyInfo->GetNzbInfo()->GetName(),
				historyInfo->GetNzbInfo()->GetDupeKey(), nzbInfo->GetName(), nzbInfo->GetDupeKey()))
		{
			donors.push_back(historyInfo->GetNzbInfo());
		}
	}

	std::sort(donors.begin(), donors.end(),
		[](NzbInfo* donor1, NzbInfo* donor2)
		{
			return donor1->GetDupeScore() > donor2->GetDupeScore() ||
				(donor1->GetDupeScore() == donor2->GetDupeScore() &&
				 donor1->GetId() < donor2->GetId());
		});

	return donors;
}

void DupeArticleFallback::PinSources(DownloadQueue* downloadQueue, FileInfo* fileInfo,
	ArticleInfo* articleInfo)
{
	NzbInfo* nzbInfo = fileInfo->GetNzbInfo();

	RawNzbList donors = CollectDonors(downloadQueue, nzbInfo);
	VacateGhostLead(fileInfo, donors);
	RotateToLead(donors, fileInfo->GetDupeLeadDonorId());

	std::vector<CString> candidates;
	std::vector<int> contributors;

	for (NzbInfo* donorNzbInfo : donors)
	{
		// an exact copy of the same posting shares the message-ids and cannot help
		if (nzbInfo->GetFullContentHash() > 0 &&
			nzbInfo->GetFullContentHash() == donorNzbInfo->GetFullContentHash())
		{
			continue;
		}

		// Extract this donor's candidate immediately: GetParsedDonor may evict a
		// previously-parsed donor from the bounded cache, so a parsed-donor pointer
		// must never be held across another GetParsedDonor call (use-after-free).
		NzbInfo* parsedDonor = GetParsedDonor(donorNzbInfo);
		if (parsedDonor)
		{
			AppendDonorCandidate(candidates, contributors, donorNzbInfo->GetId(),
				parsedDonor, fileInfo, articleInfo->GetPartNumber());
		}
	}

	FinishPin(fileInfo, articleInfo, candidates, contributors,
		fileInfo->GetDupeCutover(), articleInfo->GetDupeOriginalMessageId());
}

void DupeArticleFallback::AppendDonorCandidate(std::vector<CString>& candidates,
	std::vector<int>& contributors, int donorNzbId,
	NzbInfo* parsedDonor, FileInfo* targetFile, int partNumber)
{
	FileInfo* donorFile = MatchDonorFile(targetFile, parsedDonor);
	if (!donorFile)
	{
		return;
	}

	const char* messageId = FindDonorMessageId(donorFile, partNumber);
	if (!messageId)
	{
		return;
	}

	bool duplicate = std::find_if(candidates.begin(), candidates.end(),
		[messageId](const CString& candidate) { return !strcmp(candidate, messageId); }) != candidates.end();
	if (!duplicate)
	{
		candidates.emplace_back(messageId);
		contributors.push_back(donorNzbId);
	}
}

/*
 * Pure candidate builder over already-live donors, used by unit tests. The
	 * production path (PinSources) processes donors one at a time for
	 * cache-safety; both share AppendDonorCandidate so the logic stays in sync.
	 *
 * The candidate list must not depend on the current state of the article:
 * it is frozen on first fallback, so later donor insertion, removal, or lead
 * rotation cannot shift the round-to-source mapping.
 */
std::vector<CString> DupeArticleFallback::BuildCandidateMessageIds(
	const std::vector<NzbInfo*>& parsedDonors, FileInfo* targetFile, int partNumber)
{
	std::vector<CString> candidates;
	std::vector<int> contributors;

	for (NzbInfo* parsedDonor : parsedDonors)
	{
		AppendDonorCandidate(candidates, contributors, parsedDonor->GetId(),
			parsedDonor, targetFile, partNumber);
	}

	return candidates;
}

std::unique_ptr<NzbInfo> DupeArticleFallback::ParseDonorNzb(const char* queuedFilename)
{
	if (Util::EmptyStr(queuedFilename) || !FileSystem::FileExists(queuedFilename))
	{
		return nullptr;
	}

	NzbFile nzbFile(queuedFilename, "");
	if (!nzbFile.Parse())
	{
		detail("Could not parse duplicate nzb-file %s", queuedFilename);
		return nullptr;
	}

	std::unique_ptr<NzbInfo> parsedNzb = nzbFile.DetachNzbInfo();

	if (g_Options->GetServerMode())
	{
		// in server mode the parser offloads article lists to disk-state and
		// clears them from memory; load them back (the donor is used in memory
		// only) and remove the disk-state files written as parse side effect
		for (FileInfo* fileInfo : parsedNzb->GetFileList())
		{
			g_DiskState->LoadArticles(fileInfo);
		}
		g_DiskState->DiscardFiles(parsedNzb.get(), false);
	}

	return parsedNzb;
}

NzbInfo* DupeArticleFallback::GetParsedDonor(NzbInfo* donorNzbInfo)
{
	int donorId = donorNzbInfo->GetId();

	if (m_badDonors.find(donorId) != m_badDonors.end())
	{
		return nullptr;
	}

	auto it = m_parsedDonors.find(donorId);
	if (it != m_parsedDonors.end())
	{
		return it->second.get();
	}

	// duplicates in history don't keep their article lists in memory; the
	// retained source nzb-file is parsed again instead (like "Download again")
	std::unique_ptr<NzbInfo> parsedNzb = ParseDonorNzb(donorNzbInfo->GetQueuedFilename());
	if (!parsedNzb)
	{
		m_badDonors.insert(donorId);
		return nullptr;
	}

	if ((int)m_parsedDonors.size() >= MaxCachedDonors)
	{
		m_parsedDonors.erase(m_parsedDonors.begin());
	}

	// store the parsed collection in the cache and return a pointer to the
	// owned object (the cache keeps it alive for the daemon's lifetime)
	std::unique_ptr<NzbInfo>& cachedDonor = m_parsedDonors[donorId];
	cachedDonor = std::move(parsedNzb);

	return cachedDonor.get();
}

FileInfo* DupeArticleFallback::MatchDonorFile(FileInfo* targetFile, NzbInfo* donorNzb)
{
	FileInfo* structuralMatch = nullptr;
	bool ambiguous = false;
	FileInfo* filenameMatch = nullptr;
	bool filenameAmbiguous = false;

	for (FileInfo* donorFile : donorNzb->GetFileList())
	{
		if (!StructureMatches(targetFile, donorFile))
		{
			continue;
		}

		if (!strcasecmp(targetFile->GetFilename(), donorFile->GetFilename()))
		{
			if (filenameMatch)
			{
				filenameAmbiguous = true;
			}
			else
			{
				filenameMatch = donorFile;
			}
			continue;
		}

		ambiguous = structuralMatch != nullptr;
		structuralMatch = donorFile;
	}

	// An exact filename is preferred only when it is unique.  Choosing the
	// first exact match is nondeterministic for multi-file duplicate NZBs.
	if (filenameAmbiguous)
	{
		return nullptr;
	}
	if (filenameMatch)
	{
		return filenameMatch;
	}
	return ambiguous ? nullptr : structuralMatch;
}

bool DupeArticleFallback::StructureMatches(FileInfo* targetFile, FileInfo* donorFile)
{
	ArticleList* targetArticles = targetFile->GetArticles();
	ArticleList* donorArticles = donorFile->GetArticles();

	if (targetArticles->empty() ||
		targetArticles->size() != donorArticles->size() ||
		targetFile->GetTotalArticles() != donorFile->GetTotalArticles() ||
		!SizesMatch(targetFile->GetSize(), donorFile->GetSize(), TotalSizeToleranceDiv))
	{
		return false;
	}

	for (size_t i = 0; i < targetArticles->size(); i++)
	{
		ArticleInfo* targetArticle = (*targetArticles)[i].get();
		ArticleInfo* donorArticle = (*donorArticles)[i].get();
		if (targetArticle->GetPartNumber() != donorArticle->GetPartNumber() ||
			!SizesMatch(targetArticle->GetSize(), donorArticle->GetSize(), PartSizeToleranceDiv))
		{
			return false;
		}
	}

	return true;
}

const char* DupeArticleFallback::FindDonorMessageId(FileInfo* donorFile, int partNumber)
{
	for (ArticleInfo* article : donorFile->GetArticles())
	{
		if (article->GetPartNumber() == partNumber && !Util::EmptyStr(article->GetMessageId()))
		{
			return article->GetMessageId();
		}
	}

	return nullptr;
}

bool DupeArticleFallback::SizesMatch(int64 size1, int64 size2, int div)
{
	int64 diff = size1 > size2 ? size1 - size2 : size2 - size1;
	return diff <= std::max(size1, size2) / div;
}

static int FindArticleIndex(ArticleList* articles, ArticleInfo* articleInfo)
{
	for (size_t i = 0; i < articles->size(); i++)
	{
		if ((*articles)[i].get() == articleInfo)
		{
			return (int)i;
		}
	}
	return -1;
}

int64 DupeArticleFallback::ExpectedSegmentOffset(FileInfo* fileInfo, ArticleInfo* articleInfo)
{
	ArticleList* articles = fileInfo->GetArticles();
	int index = FindArticleIndex(articles, articleInfo);
	if (index < 0)
	{
		return -1;
	}

	if (index == 0)
	{
		// the first article of a file always decodes to offset 0
		return 0;
	}

	ArticleInfo* prev = (*articles)[index - 1].get();
	if (prev->GetStatus() == ArticleInfo::aiFinished && prev->GetSegmentSize() > 0)
	{
		return prev->GetSegmentOffset() + prev->GetSegmentSize();
	}

	return -1;
}

int64 DupeArticleFallback::ExpectedSegmentEnd(FileInfo* fileInfo, ArticleInfo* articleInfo)
{
	ArticleList* articles = fileInfo->GetArticles();
	int index = FindArticleIndex(articles, articleInfo);
	if (index < 0)
	{
		return -1;
	}

	if (index == (int)articles->size() - 1)
	{
		// the last article of a file must decode up to the decoded file size
		// (known once any article of the file was decoded)
		return fileInfo->GetDecodedFileSize() > 0 ? fileInfo->GetDecodedFileSize() : -1;
	}

	ArticleInfo* next = (*articles)[index + 1].get();
	if (next->GetStatus() == ArticleInfo::aiFinished && next->GetSegmentSize() > 0)
	{
		return next->GetSegmentOffset();
	}

	return -1;
}

bool DupeArticleFallback::SegmentAligned(FileInfo* fileInfo, ArticleInfo* articleInfo)
{
	int64 begin = articleInfo->GetSegmentOffset();
	int size = articleInfo->GetSegmentSize();
	if (size <= 0)
	{
		// no decoded placement recorded - nothing to validate against
		return true;
	}

	int64 expectedOffset = ExpectedSegmentOffset(fileInfo, articleInfo);
	if (expectedOffset >= 0 && begin != expectedOffset)
	{
		return false;
	}

	int64 expectedEnd = ExpectedSegmentEnd(fileInfo, articleInfo);
	if (expectedEnd >= 0 && begin + size != expectedEnd)
	{
		return false;
	}

	return true;
}

ArticleInfo* DupeArticleFallback::FirstUntiledArticle(FileInfo* fileInfo)
{
	// DecodedFileSize is only set from a decoded yEnc article; when it is 0 the
	// file is non-yEnc (e.g. uuencode, whose articles all record offset 0) or
	// its geometry is not yet known - either way the tiling cannot be judged.
	// After a restart it is repopulated from the first yEnc article that
	// completes, so it is available again by the time a reloaded file finishes.
	int64 decodedSize = fileInfo->GetDecodedFileSize();
	if (decodedSize <= 0)
	{
		return nullptr;
	}

	int64 expected = 0;
	for (ArticleInfo* article : fileInfo->GetArticles())
	{
		if (article->GetStatus() != ArticleInfo::aiFinished || article->GetSegmentSize() <= 0)
		{
			// an unfinished article or one without a recorded decoded placement
			// leaves the geometry incomplete: do not judge (avoids false positives
			// on partial or non-decoded states)
			return nullptr;
		}
		if (article->GetSegmentOffset() != expected)
		{
			// a gap (offset > expected) or overlap (offset < expected) at this seam
			return article;
		}
		expected = article->GetSegmentOffset() + article->GetSegmentSize();
	}

	if (expected != decodedSize)
	{
		// the assembled decoded bytes fall short of / exceed the file size
		ArticleList* articles = fileInfo->GetArticles();
		return articles->empty() ? nullptr : (*articles)[articles->size() - 1].get();
	}

	return nullptr;
}
