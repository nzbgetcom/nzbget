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
#include <set>
#include "StreamRepair.h"
#include "DupeStreamRepair.h"
#include "DupeArticleFallback.h"
#include "DupeCoordinator.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"

void StreamRepairController::StartJob(PostInfo* postInfo)
{
	StreamRepairController* streamRepairController = new StreamRepairController();
	streamRepairController->m_postInfo = postInfo;
	streamRepairController->SetAutoDestroy(false);

	postInfo->SetPostThread(streamRepairController);

	streamRepairController->Start();
}

void StreamRepairController::Run()
{
	BString<1024> nzbName;
	CString destDir;
	std::vector<RepairTarget> targets;
	std::vector<DonorSource> donors;

	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		NzbInfo* nzbInfo = m_postInfo->GetNzbInfo();
		nzbName = nzbInfo->GetName();
		destDir = nzbInfo->GetDestDir();
		CollectTargets(nzbInfo, targets);
		CollectDonors(downloadQueue, nzbInfo, donors);
		m_postInfo->SetProgressLabel("Repairing media files from duplicates");
	}

	BString<1024> infoName("stream repair for %s", *nzbName);
	SetInfoName(infoName);

	if (targets.empty())
	{
		PrintMessage(Message::mkInfo, "Nothing to stream-repair for %s", *nzbName);
	}
	else if (donors.empty())
	{
		PrintMessage(Message::mkInfo,
			"No duplicate collections found for stream repair of %s", *nzbName);
		m_holesRemain = true;
	}
	else
	{
		ExecRepair(destDir, targets, donors);
	}

	RepairCompleted();
}

void StreamRepairController::Stop()
{
	m_fetcher.Stop();
	Thread::Stop();
}

void StreamRepairController::CollectTargets(NzbInfo* nzbInfo, std::vector<RepairTarget>& targets)
{
	for (StreamRepairJob& job : *nzbInfo->GetStreamRepairJobs())
	{
		RepairTarget target;
		target.FileId = job.GetFileId();
		target.Filename = job.GetFilename();
		target.DecodedFileSize = job.GetDecodedFileSize();
		target.Holes = *job.GetHoles();

		// par-rename may have renamed the file since capture; the
		// completed-file record tracks the current on-disk name
		for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
		{
			if (completedFile.GetId() == target.FileId)
			{
				target.Filename = completedFile.GetFilename();
				break;
			}
		}

		targets.push_back(std::move(target));
	}
}

void StreamRepairController::CollectDonors(DownloadQueue* downloadQueue, NzbInfo* nzbInfo,
	std::vector<DonorSource>& donors)
{
	RawNzbList donorNzbs;

	for (NzbInfo* queuedNzbInfo : downloadQueue->GetQueue())
	{
		if (queuedNzbInfo != nzbInfo &&
			queuedNzbInfo->GetKind() == NzbInfo::nkNzb &&
			queuedNzbInfo->GetDupeMode() != dmForce &&
			DupeCoordinator::SameNameOrKey(queuedNzbInfo->GetName(), queuedNzbInfo->GetDupeKey(),
				nzbInfo->GetName(), nzbInfo->GetDupeKey()))
		{
			donorNzbs.push_back(queuedNzbInfo);
		}
	}

	for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
	{
		if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
			historyInfo->GetNzbInfo()->GetDupeMode() != dmForce &&
			DupeCoordinator::SameNameOrKey(historyInfo->GetNzbInfo()->GetName(),
				historyInfo->GetNzbInfo()->GetDupeKey(), nzbInfo->GetName(), nzbInfo->GetDupeKey()))
		{
			donorNzbs.push_back(historyInfo->GetNzbInfo());
		}
	}

	std::sort(donorNzbs.begin(), donorNzbs.end(),
		[](NzbInfo* donor1, NzbInfo* donor2)
		{
			return donor1->GetDupeScore() > donor2->GetDupeScore() ||
				(donor1->GetDupeScore() == donor2->GetDupeScore() &&
				 donor1->GetId() < donor2->GetId());
		});

	for (NzbInfo* donorNzbInfo : donorNzbs)
	{
		// an exact copy of the same posting shares the message-ids and cannot
		// supply articles that already failed everywhere
		if (nzbInfo->GetFullContentHash() > 0 &&
			nzbInfo->GetFullContentHash() == donorNzbInfo->GetFullContentHash())
		{
			continue;
		}

		if (!Util::EmptyStr(donorNzbInfo->GetQueuedFilename()))
		{
			DonorSource donor;
			donor.QueuedFilename = donorNzbInfo->GetQueuedFilename();
			donor.InfoName = donorNzbInfo->GetName();
			donors.push_back(std::move(donor));
		}
	}
}

void StreamRepairController::ExecRepair(const char* destDir,
	std::vector<RepairTarget>& targets, std::vector<DonorSource>& donors)
{
	for (DonorSource& donor : donors)
	{
		if (IsStopped())
		{
			break;
		}

		bool anyHoles = false;
		for (RepairTarget& target : targets)
		{
			anyHoles |= !target.Holes.empty();
		}
		if (!anyHoles)
		{
			break;
		}

		// parse lazily: a donor nzb is only read once a previous donor left
		// holes unfilled (file I/O and XML parsing happen unlocked)
		std::unique_ptr<NzbInfo> donorNzb = DupeArticleFallback::ParseDonorNzb(donor.QueuedFilename);
		if (!donorNzb)
		{
			continue;
		}

		for (RepairTarget& target : targets)
		{
			if (IsStopped())
			{
				break;
			}
			if (target.Holes.empty())
			{
				continue;
			}
			RepairFile(destDir, target, donorNzb.get(), donor.InfoName);
		}
	}

	for (RepairTarget& target : targets)
	{
		if (!target.Holes.empty())
		{
			m_holesRemain = true;
			PrintMessage(Message::mkInfo,
				"Stream repair of %s: %.1f MB still missing after all duplicates (left to par-repair)",
				*target.Filename, DupeStreamRepair::TotalSize(target.Holes) / 1024.0 / 1024.0);
		}
	}
}

bool StreamRepairController::RepairFile(const char* destDir, RepairTarget& target,
	NzbInfo* donorNzb, const char* donorName)
{
	BString<1024> filePath("%s%c%s", destDir, PATH_SEPARATOR, *target.Filename);

	DiskFile file;
	if (!file.Open(filePath, DiskFile::omReadWrite))
	{
		PrintMessage(Message::mkWarning, "Could not open %s for stream repair: %s",
			*filePath, *FileSystem::GetLastErrorMessage());
		return false;
	}

	bool patched = false;

	for (FileInfo* donorFile : FindDonorFiles(target, donorNzb))
	{
		if (IsStopped() || target.Holes.empty())
		{
			break;
		}

		StreamRangeList donorRanges = DupeStreamRepair::EstimateDonorRanges(
			donorFile, target.DecodedFileSize);
		if (donorRanges.empty())
		{
			continue;
		}

		if (!VerifyDonor(file, target, donorFile, donorRanges))
		{
			// mkInfo on purpose: a rejected donor is the safety net firing -
			// the user (and the e2e decoy test) should see it in the log
			PrintMessage(Message::mkInfo,
				"Skipping file %s of duplicate %s for %s: content identity not confirmed",
				donorFile->GetFilename(), donorName, *target.Filename);
			continue;
		}

		patched |= PatchFromDonor(file, target, donorFile, donorRanges, donorName) > 0;
	}

	file.Close();
	return patched;
}

std::vector<FileInfo*> StreamRepairController::FindDonorFiles(const RepairTarget& target,
	NzbInfo* donorNzb)
{
	std::vector<FileInfo*> candidates;

	for (FileInfo* donorFile : donorNzb->GetFileList())
	{
		// media identity is decoded content size; the nzb declares only
		// encoded sizes (~1-3% yEnc overhead above decoded), so filter
		// loosely here and let the probe verification decide
		if (DupeStreamRepair::IsStreamEligible(donorFile->GetFilename()) &&
			!donorFile->GetArticles()->empty() &&
			DupeArticleFallback::SizesMatch(donorFile->GetSize(), target.DecodedFileSize, 8))
		{
			candidates.push_back(donorFile);
		}
	}

	int64 decodedFileSize = target.DecodedFileSize;
	std::sort(candidates.begin(), candidates.end(),
		[decodedFileSize](FileInfo* file1, FileInfo* file2)
		{
			int64 diff1 = file1->GetSize() > decodedFileSize ?
				file1->GetSize() - decodedFileSize : decodedFileSize - file1->GetSize();
			int64 diff2 = file2->GetSize() > decodedFileSize ?
				file2->GetSize() - decodedFileSize : decodedFileSize - file2->GetSize();
			return diff1 < diff2;
		});

	return candidates;
}

bool StreamRepairController::VerifyDonor(DiskFile& file, const RepairTarget& target,
	FileInfo* donorFile, const StreamRangeList& donorRanges)
{
	std::vector<int> probeParts = DupeStreamRepair::SelectProbeParts(
		donorRanges, target.Holes, DupeStreamRepair::ProbeCount);

	bool verified = false;

	for (int partIndex : probeParts)
	{
		if (IsStopped())
		{
			return false;
		}

		ArticleInfo* article = (*donorFile->GetArticles())[partIndex].get();
		ArticleFetcher::FetchedArticle fetched = m_fetcher.Fetch(
			article->GetMessageId(), *donorFile->GetGroups());
		if (!fetched.Success)
		{
			continue; // inconclusive: the donor may simply miss this article
		}

		// the donor must declare exactly the same decoded file size
		if (fetched.FileSize != target.DecodedFileSize ||
			fetched.Offset + (int64)fetched.Data.size() > target.DecodedFileSize)
		{
			return false;
		}

		// byte-compare the fetched article against the regions we downloaded
		// ourselves (everything in the article's range that is not a hole)
		StreamRangeList present = { {fetched.Offset, (int64)fetched.Data.size()} };
		for (const StreamRange& hole : target.Holes)
		{
			DupeStreamRepair::SubtractCovered(present, hole);
		}

		int64 compared = 0;
		for (const StreamRange& range : present)
		{
			if (!CompareToFile(file, range.Offset,
				fetched.Data.data() + (range.Offset - fetched.Offset), range.Size))
			{
				return false; // same size but different content - wrong donor
			}
			compared += range.Size;
		}

		verified |= compared >= DupeStreamRepair::MinProbeCompareBytes;
	}

	return verified;
}

int StreamRepairController::PatchFromDonor(DiskFile& file, RepairTarget& target,
	FileInfo* donorFile, const StreamRangeList& donorRanges, const char* donorName)
{
	int recoveredParts = 0;
	int64 recoveredBytes = 0;
	int64 drift = 0;
	std::set<int> tried;

	// two selection passes: pass 2 re-selects with the measured drift between
	// estimated and actual donor offsets. A donor nzb with missing segment
	// entries (or regionally varying yEnc overhead) shifts ALL estimates by
	// whole parts, which a fixed margin alone cannot absorb.
	for (int pass = 0; pass < 2 && !target.Holes.empty() && !IsStopped(); pass++)
	{
		// select donor parts as if the estimate were exact: if actual offsets
		// sit at estimate+drift, the part covering hole h has estimate h-drift
		StreamRangeList shiftedHoles = target.Holes;
		for (StreamRange& hole : shiftedHoles)
		{
			hole.Offset -= drift;
		}

		std::vector<int> patchParts = DupeStreamRepair::SelectPatchParts(
			donorRanges, shiftedHoles, DupeStreamRepair::PatchMarginParts);

		bool fetchedAny = false;

		for (int partIndex : patchParts)
		{
			if (IsStopped() || target.Holes.empty())
			{
				break;
			}
			if (!tried.insert(partIndex).second)
			{
				continue; // already fetched in the previous pass
			}

			ArticleInfo* article = (*donorFile->GetArticles())[partIndex].get();
			ArticleFetcher::FetchedArticle fetched = m_fetcher.Fetch(
				article->GetMessageId(), *donorFile->GetGroups());
			if (!fetched.Success || fetched.FileSize != target.DecodedFileSize ||
				fetched.Data.empty() ||
				fetched.Offset + (int64)fetched.Data.size() > target.DecodedFileSize)
			{
				continue; // the donor misses this article too - the hole stays for par2
			}

			fetchedAny = true;
			drift = fetched.Offset - donorRanges[partIndex].Offset;

			StreamRange fetchedRange = { fetched.Offset, (int64)fetched.Data.size() };

			// write ONLY the bytes that fall into holes: everything else is
			// already on disk (and was used to verify this donor)
			StreamRangeList written;
			for (const StreamRange& hole : target.Holes)
			{
				int64 from = std::max(hole.Offset, fetchedRange.Offset);
				int64 to = std::min(hole.End(), fetchedRange.End());
				if (from < to)
				{
					file.Seek(from);
					if (file.Position() != from ||
						file.Write(fetched.Data.data() + (from - fetched.Offset), to - from) != to - from)
					{
						PrintMessage(Message::mkError,
							"Could not write to %s during stream repair: %s",
							*target.Filename, *FileSystem::GetLastErrorMessage());
						return recoveredParts;
					}
					written.push_back({from, to - from});
				}
			}

			for (const StreamRange& range : written)
			{
				DupeStreamRepair::SubtractCovered(target.Holes, range);
				recoveredBytes += range.Size;
			}

			if (!written.empty())
			{
				recoveredParts++;
			}
		}

		if (!fetchedAny || drift == 0)
		{
			break; // nothing measured, or estimates were exact - pass 2 would repeat pass 1
		}
	}

	if (recoveredParts > 0)
	{
		m_recoveredArticles += recoveredParts;
		m_recoveredBytes += recoveredBytes;
		PrintMessage(Message::mkInfo,
			"Recovered %.1f MB (%i donor article(s)) of %s from duplicate %s",
			recoveredBytes / 1024.0 / 1024.0, recoveredParts, *target.Filename, donorName);
	}

	return recoveredParts;
}

bool StreamRepairController::CompareToFile(DiskFile& file, int64 offset, const char* data,
	int64 size)
{
	file.Seek(offset);
	if (file.Position() != offset)
	{
		return false;
	}

	CharBuffer buffer(64 * 1024);
	int64 remaining = size;
	while (remaining > 0)
	{
		int64 chunk = std::min(remaining, (int64)buffer.Size());
		if (file.Read(buffer, chunk) != chunk)
		{
			return false; // short read (e.g. join-mode file shorter than expected)
		}
		if (memcmp(buffer, data + (size - remaining), chunk))
		{
			return false;
		}
		remaining -= chunk;
	}
	return true;
}

void StreamRepairController::RepairCompleted()
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	NzbInfo* nzbInfo = m_postInfo->GetNzbInfo();

	if (m_recoveredArticles > 0)
	{
		nzbInfo->SetDupeRecoveredArticles(nzbInfo->GetDupeRecoveredArticles() + m_recoveredArticles);
	}

	// draining the job list is the run-once gate for this stage
	nzbInfo->GetStreamRepairJobs()->clear();

	// whatever was written - and whatever is still missing - goes through
	// par-check as final verification (a no-op when no par2 files exist)
	if (m_recoveredArticles > 0 || m_holesRemain)
	{
		m_postInfo->SetRequestParCheck(true);
	}

	m_postInfo->SetWorking(false);
}

void StreamRepairController::AddMessage(Message::EKind kind, const char* text)
{
	m_postInfo->GetNzbInfo()->AddMessage(kind, text);
}
