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
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"

bool DupeArticleFallback::TryFallback(DownloadQueue* downloadQueue, FileInfo* fileInfo, ArticleInfo* articleInfo)
{
	if (!g_Options->GetDupeArticleFallback() || g_Options->GetRawArticle())
	{
		return false;
	}

	NzbInfo* nzbInfo = fileInfo->GetNzbInfo();
	if (fileInfo->GetDeleted() || nzbInfo->GetDeleting() || nzbInfo->GetParking() ||
		nzbInfo->GetDeleteStatus() != NzbInfo::dsNone || nzbInfo->GetDupeMode() == dmForce)
	{
		return false;
	}

	std::vector<CString> candidates = CollectCandidateMessageIds(downloadQueue, fileInfo, articleInfo);

	int round = articleInfo->GetDupeFallbackRound();
	if (round >= (int)candidates.size())
	{
		if (!candidates.empty())
		{
			nzbInfo->PrintMessage(Message::mkDetail, "No more duplicate sources for article %s [%i/%i]",
				fileInfo->GetFilename(), articleInfo->GetPartNumber(), (int)fileInfo->GetArticles()->size());
		}
		return false;
	}

	nzbInfo->PrintMessage(Message::mkDetail, "Trying article %s from duplicate for %s [%i/%i]",
		*candidates[round], fileInfo->GetFilename(), articleInfo->GetPartNumber(),
		(int)fileInfo->GetArticles()->size());

	articleInfo->SetMessageId(candidates[round]);
	articleInfo->SetDupeFallbackRound(round + 1);

	return true;
}

std::vector<CString> DupeArticleFallback::CollectCandidateMessageIds(DownloadQueue* downloadQueue,
	FileInfo* fileInfo, ArticleInfo* articleInfo)
{
	NzbInfo* nzbInfo = fileInfo->GetNzbInfo();

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

	std::vector<NzbInfo*> parsedDonors;

	for (NzbInfo* donorNzbInfo : donors)
	{
		// an exact copy of the same posting shares the message-ids and cannot help
		if (nzbInfo->GetFullContentHash() > 0 &&
			nzbInfo->GetFullContentHash() == donorNzbInfo->GetFullContentHash())
		{
			continue;
		}

		NzbInfo* parsedDonor = GetParsedDonor(donorNzbInfo);
		if (parsedDonor)
		{
			parsedDonors.push_back(parsedDonor);
		}
	}

	return BuildCandidateMessageIds(parsedDonors, fileInfo, articleInfo->GetPartNumber());
}

/*
 * The candidate list must not depend on the current state of the article:
 * ArticleInfo::m_dupeFallbackRound indexes into it across repeated failures,
 * so filtering by the currently assigned message-id would shift the indexes
 * and skip untried donors.
 */
std::vector<CString> DupeArticleFallback::BuildCandidateMessageIds(
	const std::vector<NzbInfo*>& parsedDonors, FileInfo* targetFile, int partNumber)
{
	std::vector<CString> candidates;

	for (NzbInfo* parsedDonor : parsedDonors)
	{
		FileInfo* donorFile = MatchDonorFile(targetFile, parsedDonor);
		if (!donorFile)
		{
			continue;
		}

		const char* messageId = FindDonorMessageId(donorFile, partNumber);
		if (!messageId)
		{
			continue;
		}

		bool duplicate = std::find_if(candidates.begin(), candidates.end(),
			[messageId](CString& candidate) { return !strcmp(candidate, messageId); }) != candidates.end();
		if (!duplicate)
		{
			candidates.emplace_back(messageId);
		}
	}

	return candidates;
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
	const char* queuedFilename = donorNzbInfo->GetQueuedFilename();
	if (Util::EmptyStr(queuedFilename) || !FileSystem::FileExists(queuedFilename))
	{
		m_badDonors.insert(donorId);
		return nullptr;
	}

	NzbFile nzbFile(queuedFilename, "");
	if (!nzbFile.Parse())
	{
		detail("Could not parse duplicate nzb-file %s", queuedFilename);
		m_badDonors.insert(donorId);
		return nullptr;
	}

	if ((int)m_parsedDonors.size() >= MaxCachedDonors)
	{
		m_parsedDonors.erase(m_parsedDonors.begin());
	}

	std::unique_ptr<NzbInfo> parsedNzbInfo = nzbFile.DetachNzbInfo();
	NzbInfo* rawParsedNzbInfo = parsedNzbInfo.get();
	m_parsedDonors[donorId] = std::move(parsedNzbInfo);

	return rawParsedNzbInfo;
}

FileInfo* DupeArticleFallback::MatchDonorFile(FileInfo* targetFile, NzbInfo* donorNzb)
{
	FileInfo* structuralMatch = nullptr;
	bool ambiguous = false;

	for (FileInfo* donorFile : donorNzb->GetFileList())
	{
		if (!StructureMatches(targetFile, donorFile))
		{
			continue;
		}

		if (!strcasecmp(targetFile->GetFilename(), donorFile->GetFilename()))
		{
			return donorFile;
		}

		ambiguous = structuralMatch != nullptr;
		structuralMatch = donorFile;
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
