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
#include "StreamCrypto.h"
#include "DupeStreamRepair.h"
#include "DupeArticleFallback.h"
#include "DupeCoordinator.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"
#include "Unpack.h"

ContentSource* DiskSourceSet::GetSource(int memberIndex)
{
	if (memberIndex < 0 || memberIndex >= (int)m_entries.size())
	{
		return nullptr;
	}
	Entry& entry = m_entries[memberIndex];
	if (!entry.Tried)
	{
		entry.Tried = true;
		BString<1024> path("%s%c%s", *m_destDir, PATH_SEPARATOR,
			m_members[memberIndex].Name.c_str());
		int64 size = FileSystem::FileSize(path);
		if (size > 0 && entry.File.Open(path, DiskFile::omRead))
		{
			entry.Source = std::make_unique<DiskContentSource>(entry.File, size);
		}
	}
	return entry.Source.get();
}

int DonorMemberSource::TakeServedParts()
{
	int count = (int)m_servedParts.size();
	m_servedParts.clear();
	return count;
}

bool DonorMemberSource::EnsureInit()
{
	if (m_size >= 0)
	{
		return true;
	}
	if (m_bad || m_donorFile->GetArticles()->empty())
	{
		return false;
	}
	const ArticleFetcher::FetchedArticle* first = FetchPart(0);
	if (!first || !first->Success || first->Offset != 0)
	{
		m_bad = true;	// without article 0 neither headers nor size exist
		return false;
	}
	m_size = first->FileSize;
	m_ranges = DupeStreamRepair::EstimateDonorRanges(m_donorFile, m_size);
	if (m_ranges.empty())
	{
		// a malformed donor nzb (all-zero segment sizes) yields no estimates:
		// without them offsets cannot resolve, and a committed m_size would
		// let a later Read pass the fast-path into indexing an empty vector
		m_bad = true;
		m_size = -1;
		return false;
	}
	return true;
}

const ArticleFetcher::FetchedArticle* DonorMemberSource::FetchPart(int partIndex)
{
	if (partIndex < 0 || partIndex >= (int)m_donorFile->GetArticles()->size() || m_bad)
	{
		return nullptr;
	}
	auto cached = m_cache.find(partIndex);
	if (cached != m_cache.end())
	{
		return cached->second.Success ? &cached->second : nullptr;
	}
	if (m_fetchBudget && *m_fetchBudget <= 0)
	{
		return nullptr;
	}
	if (m_fetchBudget)
	{
		(*m_fetchBudget)--;
	}

	ArticleInfo* article = (*m_donorFile->GetArticles())[partIndex].get();
	ArticleFetcher::FetchedArticle fetched = m_fetcher.Fetch(
		article->GetMessageId(), *m_donorFile->GetGroups());

	// consistency: every article of the member must declare the same
	// decoded size and stay inside it, or the whole member is unusable
	if (fetched.Success &&
		(fetched.Data.empty() || fetched.Offset < 0 ||
		 fetched.Offset + (int64)fetched.Data.size() > fetched.FileSize ||
		 (m_size >= 0 && fetched.FileSize != m_size)))
	{
		m_bad = m_size >= 0 && fetched.FileSize != m_size;
		fetched.Success = false;
	}

	if ((int)m_cache.size() >= MaxCachedParts && !m_cacheOrder.empty())
	{
		m_cache.erase(m_cacheOrder.front());
		m_cacheOrder.pop_front();
	}
	m_cacheOrder.push_back(partIndex);
	auto inserted = m_cache.emplace(partIndex, std::move(fetched)).first;
	return inserted->second.Success ? &inserted->second : nullptr;
}

const ArticleFetcher::FetchedArticle* DonorMemberSource::PartForOffset(int64 offset,
	int& partIndex)
{
	// index guess from the estimates, corrected by the measured drift
	int64 target = offset - m_drift;
	int index = 0;
	while (index + 1 < (int)m_ranges.size() && m_ranges[index].End() <= target)
	{
		index++;
	}

	for (int step = 0; step < MaxResolveSteps; step++)
	{
		const ArticleFetcher::FetchedArticle* fetched = FetchPart(index);
		if (!fetched)
		{
			return nullptr;
		}
		m_drift = fetched->Offset - m_ranges[index].Offset;
		if (offset < fetched->Offset)
		{
			if (index == 0)
			{
				return nullptr;
			}
			index--;
		}
		else if (offset >= fetched->Offset + (int64)fetched->Data.size())
		{
			if (index + 1 >= (int)m_ranges.size())
			{
				return nullptr;
			}
			index++;
		}
		else
		{
			partIndex = index;
			return fetched;
		}
	}
	return nullptr;
}

bool DonorMemberSource::Read(int64 offset, void* buffer, int64 size)
{
	if (!EnsureInit() || offset < 0 || size < 0 || offset + size > m_size)
	{
		return false;
	}

	char* out = (char*)buffer;
	int64 pos = offset;
	while (pos < offset + size)
	{
		int partIndex;
		const ArticleFetcher::FetchedArticle* fetched = PartForOffset(pos, partIndex);
		if (!fetched)
		{
			return false;
		}
		int64 from = pos - fetched->Offset;
		int64 len = std::min<int64>((int64)fetched->Data.size() - from, offset + size - pos);
		memcpy(out, fetched->Data.data() + from, len);
		m_servedParts.insert(partIndex);
		out += len;
		pos += len;
	}
	return true;
}

DonorSetSources::DonorSetSources(ArticleFetcher& fetcher, NzbInfo* donorNzb) :
	m_fetcher(fetcher)
{
	for (FileInfo* fileInfo : donorNzb->GetFileList())
	{
		m_files.push_back(fileInfo);
	}
	m_sources.resize(m_files.size());
}

DonorMemberSource* DonorSetSources::GetDonorSource(int memberIndex)
{
	if (memberIndex < 0 || memberIndex >= (int)m_files.size())
	{
		return nullptr;
	}
	if (!m_sources[memberIndex])
	{
		m_sources[memberIndex] = std::make_unique<DonorMemberSource>(
			m_fetcher, m_files[memberIndex]);
		m_sources[memberIndex]->SetFetchBudget(m_fetchBudget);
	}
	return m_sources[memberIndex].get();
}

std::vector<SetMember> DonorSetSources::BuildMembers()
{
	std::vector<SetMember> members;
	for (FileInfo* fileInfo : m_files)
	{
		members.push_back({FileSystem::BaseFileName(fileInfo->GetFilename()),
			fileInfo->GetSize()});
	}
	return members;
}

void DonorSetSources::SetFetchBudget(int* budget)
{
	m_fetchBudget = budget;
	for (std::unique_ptr<DonorMemberSource>& source : m_sources)
	{
		if (source)
		{
			source->SetFetchBudget(budget);
		}
	}
}

int DonorSetSources::TakeServedParts()
{
	int total = 0;
	for (std::unique_ptr<DonorMemberSource>& source : m_sources)
	{
		if (source)
		{
			total += source->TakeServedParts();
		}
	}
	return total;
}

DiskFile* TargetSetFiles::GetFile(int memberIndex)
{
	auto existing = m_files.find(memberIndex);
	if (existing != m_files.end())
	{
		return existing->second && existing->second->Active() ?
			existing->second.get() : nullptr;
	}
	std::unique_ptr<DiskFile> file = std::make_unique<DiskFile>();
	BString<1024> path("%s%c%s", *m_destDir, PATH_SEPARATOR,
		m_members[memberIndex].Name.c_str());
	if (!file->Open(path, DiskFile::omReadWrite))
	{
		file.reset();
	}
	DiskFile* result = file.get();
	m_files.emplace(memberIndex, std::move(file));
	return result;
}

void StreamRepairController::StartJob(PostInfo* postInfo)
{
	StreamRepairController* streamRepairController = new StreamRepairController();
	streamRepairController->m_postInfo = postInfo;
	streamRepairController->SetAutoDestroy(false);

	postInfo->SetPostThread(streamRepairController);

	streamRepairController->Start();
}

void StreamRepairController::StartLive(NzbInfo* nzbInfo)
{
	StreamRepairController* streamRepairController = new StreamRepairController();
	streamRepairController->m_liveMode = true;
	streamRepairController->m_nzbId = nzbInfo->GetId();
	streamRepairController->SetAutoDestroy(true);

	nzbInfo->SetLiveRepairThread(streamRepairController);

	streamRepairController->Start();
}

void StreamRepairController::Run()
{
	if (m_liveMode)
	{
		RunLive();
		return;
	}

	BString<1024> nzbName;
	CString destDir;
	std::vector<RepairTarget> targets;
	std::vector<DonorSource> donors;
	std::vector<CString> memberNames;

	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		NzbInfo* nzbInfo = m_postInfo->GetNzbInfo();
		nzbName = nzbInfo->GetName();
		destDir = nzbInfo->GetDestDir();
		CollectTargets(nzbInfo, targets, memberNames);
		CollectDonors(downloadQueue, nzbInfo, donors);
		m_postInfo->SetProgressLabel("Repairing files from duplicates");
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
	}
	else
	{
		ComputePositionalRanks(destDir, targets, memberNames);
		ExecRepair(destDir, targets, donors);

		bool anyHoles = false;
		for (RepairTarget& target : targets)
		{
			anyHoles |= !target.Holes.empty();
		}
		if (anyHoles && !IsStopped())
		{
			ExecCrossPackRepair(destDir, targets, donors, memberNames);
		}
	}

	ReportRemainingHoles(targets);
	m_targets = std::move(targets);
	RepairCompleted();
}

/*
 * Download-concurrent repair pass (option <DupeArticleFallback> value
 * "live"): repairs the holes of the collection's already-completed files
 * while the remaining files still download. Every locked touchpoint re-finds
 * the NzbInfo by id and verifies this thread is still attached, so a deleted
 * collection (detached via PrePostProcessor::NzbDeleted) aborts the pass
 * without touching freed state. See the class comment for what a live pass
 * deliberately does NOT do.
 */
void StreamRepairController::RunLive()
{
	BString<1024> nzbName;
	CString destDir;
	std::vector<RepairTarget> targets;
	std::vector<DonorSource> donors;
	std::vector<CString> memberNames;

	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(m_nzbId);
		if (!nzbInfo || nzbInfo->GetLiveRepairThread() != this)
		{
			return;
		}
		nzbName = nzbInfo->GetName();
		destDir = nzbInfo->GetDestDir();
		CollectTargets(nzbInfo, targets, memberNames);
		CollectDonors(downloadQueue, nzbInfo, donors);

		// each job gets exactly ONE live attempt: donors are static, so
		// re-fetching for holes an earlier live pass could not fill would only
		// re-pay the fetch traffic; whatever remains is retried once more by
		// the post-processing pass with the complete member universe. Already
		// attempted jobs stay in the target list with their holes DECLARED
		// (PatchEligible=false): the cross-pack member universe must know
		// their holes, or their zero-filled ranges would be treated as valid
		// present bytes and poison map building and donor verification
		for (RepairTarget& target : targets)
		{
			for (StreamRepairJob& job : *nzbInfo->GetStreamRepairJobs())
			{
				if (job.GetFileId() == target.FileId)
				{
					target.PatchEligible = !job.GetLiveAttempted();
					job.SetLiveAttempted(true);
					break;
				}
			}
		}
	}

	BString<1024> infoName("live stream repair for %s", *nzbName);
	SetInfoName(infoName);

	bool anyEligible = false;
	for (RepairTarget& target : targets)
	{
		anyEligible |= target.PatchEligible && !target.Holes.empty();
	}

	if (anyEligible && !donors.empty())
	{
		PrintMessage(Message::mkInfo, "Starting live stream repair for %s", *nzbName);

		ComputePositionalRanks(destDir, targets, memberNames);
		ExecRepair(destDir, targets, donors);

		bool anyHoles = false;
		for (RepairTarget& target : targets)
		{
			anyHoles |= target.PatchEligible && !target.Holes.empty();
		}
		if (anyHoles && !IsStopped())
		{
			// completed files may have been renamed since the pass started
			// (e.g. direct rename after its trigger par2 arrived): re-resolve
			// the on-disk names so the member universe is built against them
			RefreshLiveNames(targets, memberNames);
			ExecCrossPackRepair(destDir, targets, donors, memberNames);
		}
	}

	RepairCompletedLive(targets);
}

/*
 * Re-resolves the targets' and member universe's on-disk names from the
 * live CompletedFiles records (live mode only): direct rename can rename
 * completed files while the pass runs, and repairing under a stale name
 * would consume the job's one live attempt with zero work.
 */
void StreamRepairController::RefreshLiveNames(std::vector<RepairTarget>& targets,
	std::vector<CString>& memberNames)
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(m_nzbId);
	if (!nzbInfo || nzbInfo->GetLiveRepairThread() != this)
	{
		return;
	}

	for (RepairTarget& target : targets)
	{
		for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
		{
			if (completedFile.GetId() == target.FileId)
			{
				target.Filename = completedFile.GetFilename();
				break;
			}
		}
	}

	memberNames.clear();
	for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
	{
		memberNames.emplace_back(completedFile.GetFilename());
	}
}

void StreamRepairController::RefreshLiveTargetName(RepairTarget& target)
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(m_nzbId);
	if (!nzbInfo || nzbInfo->GetLiveRepairThread() != this)
	{
		return;
	}

	for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
	{
		if (completedFile.GetId() == target.FileId)
		{
			target.Filename = completedFile.GetFilename();
			break;
		}
	}
}

/*
 * Live-pass epilogue: shrink the jobs' hole lists to what is still missing
 * and add the recovered bytes/holes statistics, then detach. Deliberately
 * NO health credit, NO job draining, NO par-check request - the
 * post-processing stage stays the sole accounting authority (its per-file
 * credit gate then simply finds the holes already empty). Must not print
 * messages while holding the lock (AddMessage re-acquires it in live mode).
 */
void StreamRepairController::RepairCompletedLive(std::vector<RepairTarget>& targets)
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(m_nzbId);
	if (!nzbInfo || nzbInfo->GetLiveRepairThread() != this)
	{
		// deleted or detached mid-pass: nothing may be written back
		return;
	}

	if (m_recoveredBytes > 0 || m_recoveredHoles > 0)
	{
		nzbInfo->SetDupeRecoveredBytes(nzbInfo->GetDupeRecoveredBytes() + m_recoveredBytes);
		nzbInfo->SetDupeRecoveredHoles(nzbInfo->GetDupeRecoveredHoles() + m_recoveredHoles);
	}

	for (RepairTarget& target : targets)
	{
		if (!target.PatchEligible)
		{
			// declared-only target: its job's holes were never touched
			continue;
		}
		for (StreamRepairJob& job : *nzbInfo->GetStreamRepairJobs())
		{
			if (job.GetFileId() == target.FileId)
			{
				job.SetHoles(std::move(target.Holes));
				break;
			}
		}
	}

	nzbInfo->SetLiveRepairThread(nullptr);
	if (nzbInfo->GetPostInfo() && !nzbInfo->GetUnpackThread())
	{
		// the download completed while this pass ran and the post-processing
		// job is waiting for it (see PrePostProcessor::NzbDownloaded). The
		// working flag is shared with direct unpack: when that thread is
		// still attached it stays the flag's owner and clears it on its own
		// exit (which symmetrically leaves the flag alone while this pass is
		// attached - see DirectUnpack's epilogue)
		nzbInfo->GetPostInfo()->SetWorking(false);
	}

	// jobs (and their shrunk holes) are persisted; keep disk-state honest
	downloadQueue->Save();
}

void StreamRepairController::Stop()
{
	m_fetcher.Stop();
	Thread::Stop();
}

void StreamRepairController::CollectTargets(NzbInfo* nzbInfo, std::vector<RepairTarget>& targets,
	std::vector<CString>& memberNames)
{
	// the target's own archive password enables encrypted store-rar target
	// sets to map (M3); an empty parameter means plain M2 behavior
	NzbParameter* parameter = nzbInfo->GetParameters()->Find("*Unpack:Password");
	if (parameter)
	{
		m_targetPassword = parameter->GetValue();
	}

	for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
	{
		memberNames.emplace_back(completedFile.GetFilename());
	}

	for (StreamRepairJob& job : *nzbInfo->GetStreamRepairJobs())
	{
		RepairTarget target;
		target.FileId = job.GetFileId();
		target.Filename = job.GetFilename();
		target.DecodedFileSize = job.GetDecodedFileSize();
		// encoded failed size and par2 flag come from the job (source of truth,
		// captured while the FileInfo was still alive at job creation)
		target.FailedSize = job.GetFailedSize();
		target.MissedSize = job.GetMissedSize();
		target.FailedArticles = job.GetFailedArticles();
		target.IsParFile = job.GetParFile();
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

void StreamRepairController::ComputePositionalRanks(const char* destDir,
	std::vector<RepairTarget>& targets, const std::vector<CString>& memberNames)
{
	// a target's rank = how many same-size members sort before it by name;
	// the window size (incl. the target itself) must match the donor side's
	// for positional pairing to fire (the set-signature check). Complete
	// members' sizes come from disk (DirectWrite pre-allocates full size; a
	// join-mode short file drops out of the window, degrading to later tiers)
	std::vector<std::pair<const char*, int64>> memberSizes;
	for (const CString& memberName : memberNames)
	{
		BString<1024> memberPath("%s%c%s", destDir, PATH_SEPARATOR, *memberName);
		memberSizes.emplace_back(*memberName, FileSystem::FileSize(memberPath));
	}

	for (RepairTarget& target : targets)
	{
		int rank = 0;
		int windowSize = 0;
		bool selfSeen = false;
		for (std::pair<const char*, int64>& member : memberSizes)
		{
			if (member.second <= 0 ||
				!DupeArticleFallback::SizesMatch(member.second, target.DecodedFileSize, 8))
			{
				continue;
			}
			windowSize++;
			if (!strcasecmp(member.first, target.Filename))
			{
				selfSeen = true;
				continue;
			}
			if (strcasecmp(member.first, target.Filename) < 0)
			{
				rank++;
			}
		}
		target.PositionalRank = selfSeen ? rank : -1;
		target.PositionalWindow = selfSeen ? windowSize : 0;
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
			// the donor's password must come from the live NzbInfo (queue or
			// history - both persist parameters): the later ParseDonorNzb
			// re-parse of the raw .nzb cannot see intake- or API-set passwords
			NzbParameter* parameter = donorNzbInfo->GetParameters()->Find("*Unpack:Password");
			if (parameter)
			{
				donor.Password = parameter->GetValue();
			}
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
			anyHoles |= target.PatchEligible && !target.Holes.empty();
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

		if (m_postInfo)
		{
			GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
			m_postInfo->SetProgressLabel(BString<1024>(
				"Repairing from duplicate %s", *donor.InfoName));
		}

		int consecutiveFailures = 0;

		for (RepairTarget& target : targets)
		{
			if (IsStopped())
			{
				break;
			}
			if (!target.PatchEligible || target.Holes.empty())
			{
				continue;
			}
			ERepairOutcome outcome = RepairFile(destDir, target, donorNzb.get(), donor.InfoName);
			if (outcome == roProductive)
			{
				consecutiveFailures = 0;
			}
			else if (outcome == roUnproductive && ++consecutiveFailures >= DonorFailureBail)
			{
				PrintMessage(Message::mkInfo,
					"Skipping remaining files for duplicate %s (%i consecutive files without a byte-identical match)",
					*donor.InfoName, consecutiveFailures);
				break;
			}
		}
	}
}

StreamRepairController::ERepairOutcome StreamRepairController::RepairFile(const char* destDir,
	RepairTarget& target, NzbInfo* donorNzb, const char* donorName)
{
	if (m_liveMode)
	{
		// direct rename can rename the completed file while this pass runs;
		// opening a stale name would consume the job's one live attempt with
		// zero work, so re-resolve it right before the open
		RefreshLiveTargetName(target);
	}

	BString<1024> filePath("%s%c%s", destDir, PATH_SEPARATOR, *target.Filename);

	DiskFile file;
	if (!file.Open(filePath, DiskFile::omReadWrite))
	{
		PrintMessage(Message::mkWarning, "Could not open %s for stream repair: %s",
			*filePath, *FileSystem::GetLastErrorMessage());
		return roNoCost;
	}

	bool patched = false;
	bool spentFetches = false;

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
		spentFetches = true;

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
	return patched ? roProductive : spentFetches ? roUnproductive : roNoCost;
}

std::vector<FileInfo*> StreamRepairController::FindDonorFiles(const RepairTarget& target,
	NzbInfo* donorNzb)
{
	return DupeStreamRepair::SelectDonorCandidates(target.Filename, target.DecodedFileSize,
		target.PositionalRank, target.PositionalWindow, donorNzb,
		DupeStreamRepair::MaxDonorCandidates);
}

bool StreamRepairController::VerifyDonor(DiskFile& file, const RepairTarget& target,
	FileInfo* donorFile, const StreamRangeList& donorRanges)
{
	std::vector<int> probeParts = DupeStreamRepair::SelectProbeParts(
		donorRanges, target.Holes, DupeStreamRepair::ProbeCount);

	if (probeParts.empty())
	{
		// small or heavily-holed files: no donor article clears the holes
		// entirely. Probe the articles with the LARGEST present overlap
		// instead (margin 1 pulls in hole-adjacent, fully-present parts);
		// the compare below still covers only the target's present regions
		std::vector<int> pool = DupeStreamRepair::SelectPatchParts(
			donorRanges, target.Holes, 1);
		std::vector<std::pair<int64, int>> overlaps;
		for (int partIndex : pool)
		{
			StreamRangeList present = { donorRanges[partIndex] };
			for (const StreamRange& hole : target.Holes)
			{
				DupeStreamRepair::SubtractCovered(present, hole);
			}
			int64 overlap = DupeStreamRepair::TotalSize(present);
			if (overlap > 0)
			{
				overlaps.emplace_back(overlap, partIndex);
			}
		}
		std::sort(overlaps.begin(), overlaps.end(),
			[](const std::pair<int64, int>& part1, const std::pair<int64, int>& part2)
			{
				return part1.first > part2.first;
			});
		int64 base = DupeStreamRepair::BaseCompareFloor(
			target.DecodedFileSize - DupeStreamRepair::TotalSize(target.Holes));
		int64 pooled = 0;
		for (const std::pair<int64, int>& candidate : overlaps)
		{
			if ((int)probeParts.size() >= DupeStreamRepair::ProbeCount &&
				(pooled >= base || (int)probeParts.size() >= DupeStreamRepair::ProbeCount * 2))
			{
				break;
			}
			probeParts.push_back(candidate.second);
			pooled += candidate.first;
		}
	}

	// the compare floor scales down for small files (a repost's par2
	// volumes) and clamps to what the selected probes can actually reach;
	// below 64 reachable bytes identity is unknowable and par2 owns it.
	// Compared bytes accumulate ACROSS probes.
	int64 requiredCompare = DupeStreamRepair::RequiredCompareFloor(
		target.DecodedFileSize, target.Holes, donorRanges, probeParts);
	int64 totalCompared = 0;
	bool sawVariedData = false;

	for (int partIndex : probeParts)
	{
		if (IsStopped())
		{
			return false;
		}

		ArticleInfo* article = (*donorFile->GetArticles())[partIndex].get();
		ArticleFetcher::FetchedArticle fetched = m_fetcher.Fetch(
			article->GetMessageId(), *donorFile->GetGroups());
		if (!fetched.Success || fetched.Data.empty())
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

		for (const StreamRange& range : present)
		{
			const char* rangeData = fetched.Data.data() + (range.Offset - fetched.Offset);
			if (!CompareToFile(file, range.Offset, rangeData, range.Size))
			{
				return false; // same size but different content - wrong donor
			}
			totalCompared += range.Size;

			// constant-byte runs (zero padding, sparse regions) match ANY
			// same-size sibling and prove nothing; identity needs at least
			// one compared range with two distinct byte values
			for (int64 i = 1; i < range.Size && !sawVariedData; i++)
			{
				sawVariedData = rangeData[i] != rangeData[0];
			}
		}
	}

	return totalCompared >= requiredCompare && sawVariedData;
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

void StreamRepairController::ReportRemainingHoles(std::vector<RepairTarget>& targets)
{
	for (RepairTarget& target : targets)
	{
		if (!target.Holes.empty())
		{
			m_holesRemain = true;
			PrintMessage(Message::mkInfo,
				"Stream repair of %s: %.1f MB still missing after stream repair (left to par-repair)",
				*target.Filename, DupeStreamRepair::TotalSize(target.Holes) / 1024.0 / 1024.0);
		}
		else
		{
			// Preserve the documented API unit: failed target articles repaired,
			// never donor chunks, proactive cutover probes, or hole count.
			m_recoveredArticles += target.FailedArticles;
			// fully repaired (every hole filled): credit this file's ENCODED
			// failed size back to health, exactly reversing its contribution to
			// m_currentFailedSize (DownloadInfo.cpp:675) and, for a par2 file,
			// m_parCurrentFailedSize (:687). A target with any remaining hole
			// credits nothing, so a partial repair can never raise health -
			// both correct and the safety guarantee.
			m_recoveredFailedSize += target.FailedSize + target.MissedSize;
			if (target.IsParFile)
			{
				m_recoveredFailedParSize += target.FailedSize + target.MissedSize;
			}
		}
	}
}

void StreamRepairController::ExecCrossPackRepair(const char* destDir,
	std::vector<RepairTarget>& targets, std::vector<DonorSource>& donors,
	const std::vector<CString>& memberNames)
{
	// the member universe: every completed file, with the CURRENT holes of
	// the ones that are repair targets (holes only shrink after M1). Holes
	// are declared for ALL holed targets - including live-ineligible ones -
	// but only PatchEligible targets get a patch slot: an undeclared hole
	// would be read as valid present bytes (zero-fill parsed as headers,
	// probes comparing donors against zeroes)
	std::vector<SetMember> setMembers;
	std::vector<StreamRangeList> memberHoles(memberNames.size());
	std::vector<int> memberTargets(memberNames.size(), -1);
	for (size_t i = 0; i < memberNames.size(); i++)
	{
		BString<1024> path("%s%c%s", destDir, PATH_SEPARATOR, *memberNames[i]);
		setMembers.push_back({*memberNames[i], FileSystem::FileSize(path)});
		for (size_t t = 0; t < targets.size(); t++)
		{
			if (!strcasecmp(*targets[t].Filename, *memberNames[i]))
			{
				memberHoles[i] = targets[t].Holes;
				memberTargets[i] = targets[t].PatchEligible ? (int)t : -1;
				break;
			}
		}
	}

	DiskSourceSet diskSources(destDir, setMembers);
	HoledSourceSet holedSources(diskSources, memberHoles);
	// the target's own password (if any) lets encrypted store-rar target sets
	// map; plaintext sets are unaffected by it
	std::vector<RepairSetData> repairSets =
		ContentMapper::BuildRepairSets(setMembers, memberHoles, holedSources,
			m_targetPassword.Empty() ? nullptr : *m_targetPassword);

	bool anyMapped = false;
	for (RepairSetData& repairSet : repairSets)
	{
		if (repairSet.Map)
		{
			anyMapped = true;
			PrintMessage(Message::mkInfo,
				"Cross-packing repair of %s: %.1f MB missing of inner file %s",
				setMembers[repairSet.Set.Members[0]].Name.c_str(),
				DupeStreamRepair::TotalSize(repairSet.InnerHoles) / 1024.0 / 1024.0,
				repairSet.Map->GetInnerName());
		}
		else
		{
			PrintMessage(Message::mkInfo,
				"Cross-packing repair of %s not possible: %s; left to par-repair",
				setMembers[repairSet.Set.Members[0]].Name.c_str(),
				repairSet.SkipReason.c_str());
		}
	}
	if (!anyMapped)
	{
		return;
	}

	TargetSetFiles targetFiles(destDir, setMembers);

	// M4 (option <DupeStreamDecompress>): one decompression attempt per
	// (target set x donor set) for the whole pass - materialize+extract is
	// far too heavy to ever repeat against the same pair
	std::set<std::pair<const RepairSetData*, std::string>> decompressTried;

	for (DonorSource& donor : donors)
	{
		if (IsStopped())
		{
			break;
		}
		bool anyInnerHoles = false;
		for (RepairSetData& repairSet : repairSets)
		{
			anyInnerHoles |= repairSet.Map && !repairSet.InnerHoles.empty();
		}
		if (!anyInnerHoles)
		{
			break;
		}

		std::unique_ptr<NzbInfo> donorNzb =
			DupeArticleFallback::ParseDonorNzb(donor.QueuedFilename);
		if (!donorNzb)
		{
			continue;
		}

		if (m_postInfo)
		{
			GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
			m_postInfo->SetProgressLabel(BString<1024>(
				"Cross-packing repair from duplicate %s", *donor.InfoName));
		}

		DonorSetSources donorSources(m_fetcher, donorNzb.get());
		std::vector<SetMember> donorMembers = donorSources.BuildMembers();
		std::vector<MemberSet> donorSets = ContentMapper::GroupSets(donorMembers);

		for (RepairSetData& repairSet : repairSets)
		{
			if (!repairSet.Map || repairSet.InnerHoles.empty() || IsStopped())
			{
				continue;
			}

			for (const MemberSet& donorSet : donorSets)
			{
				if (IsStopped() || repairSet.InnerHoles.empty())
				{
					break;
				}

				// map building is fetch-budgeted; probing and patching are
				// naturally bounded by the holes
				int mappingBudget = 8 + 4 * (int)donorSet.Members.size();
				donorSources.SetFetchBudget(&mappingBudget);
				std::string donorSkip;
				std::unique_ptr<ContentMap> donorMap =
					ContentMapper::BuildMap(donorMembers, donorSet, donorSources, donorSkip);
				// M3 ladder: a donor that failed ONLY for encryption may map
				// with its own password (already-fetched articles are cached,
				// so the retry mostly re-parses); anything else stays skipped.
				// The reasons are the constants BuildRarMap sets, so a reword
				// there cannot silently disable this retry.
				if (!donorMap && !donor.Password.Empty() &&
					(donorSkip == ContentMapper::SkipEncryptedData ||
					 donorSkip == ContentMapper::SkipEncryptedHeaders))
				{
					mappingBudget = 8 + 4 * (int)donorSet.Members.size();
					donorMap = ContentMapper::BuildMap(donorMembers, donorSet,
						donorSources, donorSkip, donor.Password);
					if (!donorMap)
					{
						// the safety net firing (e.g. "archive password
						// rejected") must be visible; never log the password
						PrintMessage(Message::mkInfo,
							"Skipping packing %s of duplicate %s: %s",
							donorMembers[donorSet.Members[0]].Name.c_str(),
							*donor.InfoName, donorSkip.c_str());
					}
				}
				donorSources.SetFetchBudget(nullptr);

				if (!donorMap ||
					donorMap->GetInnerSize() != repairSet.Map->GetInnerSize())
				{
					// M4 ladder (option <DupeStreamDecompress>): a donor set
					// that cannot map for byte-copy may still donate through
					// extraction if it is a compressed archive. Skipped in a
					// live pass - materializing and extracting whole archives
					// beside an active download is too disk/CPU-heavy; those
					// holes wait for the post-processing pass
					if (!m_liveMode && g_Options->GetDupeStreamDecompress() &&
						ContentMapper::IsCompressibleArchive(donorSet) &&
						decompressTried.emplace(&repairSet,
							std::string(*donor.QueuedFilename) + "|" +
							donorMembers[donorSet.Members[0]].Name).second)
					{
						// an encrypted target is a store-mode encrypted rar (a
						// compressed archive never maps, so it is never a repair
						// target): the extracted plaintext is re-encrypted under
						// the target's stream context by the M3 write core
						ExecDecompressRepair(destDir, repairSet, donorNzb.get(),
							donorMembers, donorSet, donor, targetFiles,
							targets, memberTargets, setMembers);
					}
					continue;	// not this target's content (or not mappable)
				}

				if (!VerifyDonorSet(repairSet, *donorMap, donorSources, targetFiles))
				{
					// mkInfo on purpose: a rejected donor is the safety net
					// firing - it must be visible in the log
					PrintMessage(Message::mkInfo,
						"Skipping packing %s of duplicate %s for %s: content identity not confirmed",
						donorMembers[donorSet.Members[0]].Name.c_str(),
						*donor.InfoName, repairSet.Map->GetInnerName());
					continue;
				}
				donorSources.TakeServedParts();	// probe fetches are not "recovered"

				PatchFromDonorSet(repairSet, *donorMap, donorSources,
					targetFiles, targets, memberTargets, setMembers, donor.InfoName);
			}
		}
	}
}

namespace
{
	/* Adapts a mapped donor to the ContentSource plaintext interface the
	 * encrypted verify/patch cores read through, so the M3 (mapped-donor) and
	 * M4-into-encrypted (extracted-file, a DiskContentSource) paths share one
	 * crypto implementation. Reads decode/decrypt through the donor map. */
	class DonorInnerPlaintextSource : public ContentSource
	{
	public:
		DonorInnerPlaintextSource(ContentMap& map, DonorSetSources& sources)
			: m_map(map), m_sources(sources) {}
		int64 Size() override { return m_map.GetInnerSize(); }
		bool Read(int64 offset, void* buffer, int64 size) override
		{
			if (!StreamRepairController::ReadDonorInner(m_map, m_sources,
				{offset, size}, m_scratch))
			{
				return false;
			}
			memcpy(buffer, m_scratch.data(), (size_t)size);
			return true;
		}
	private:
		ContentMap& m_map;
		DonorSetSources& m_sources;
		std::vector<char> m_scratch;
	};
}

bool StreamRepairController::ReadDonorInner(ContentMap& donorMap,
	DonorSetSources& donorSources, const StreamRange& innerRange, std::vector<char>& buffer)
{
	buffer.resize(innerRange.Size);

	// an encrypted donor serves PLAINTEXT transparently: the map fetches the
	// covering cipher blocks (their predecessor included, across member
	// boundaries) and decrypts. Any unavailable byte fails the whole read.
	if (donorMap.GetEncrypted())
	{
		return donorMap.ReadInnerDecrypted(donorSources, innerRange, buffer.data());
	}

	int64 covered = 0;
	for (const MemberRange& piece : donorMap.MapFromInner(innerRange))
	{
		StreamRangeList innerPos = donorMap.MapToInner(piece.MemberIndex, piece.Range);
		if (innerPos.size() != 1)
		{
			return false;
		}
		DonorMemberSource* source = donorSources.GetDonorSource(piece.MemberIndex);
		if (!source || !source->Read(piece.Range.Offset,
			buffer.data() + (innerPos[0].Offset - innerRange.Offset), piece.Range.Size))
		{
			return false;
		}
		covered += piece.Range.Size;
	}
	// a donor map with an excluded member cannot cover such a range fully
	return covered == innerRange.Size;
}

/* probe windows: present bytes hugging each CURRENT hole (the most
 * drift-sensitive positions), clipped against the ORIGINAL holes:
 * identity evidence must anchor to primary-downloaded bytes only, and
 * a previous donor may have partially filled a hole - no window byte
 * may overlap any region that was EVER a hole. A hole whose front or
 * tail was donor-written is still covered by its containing original
 * hole on that side, which zeroes the window there (fail closed).
 * Shared by every inner-space verifier (M2 plain, M3 ciphertext, M4
 * decompressed donors). */
std::vector<StreamRange> StreamRepairController::BuildProbeWindows(const RepairSetData& repairSet)
{
	const StreamRangeList& holes = repairSet.InnerHoles;
	const StreamRangeList& originalHoles = repairSet.OriginalInnerHoles;

	std::vector<StreamRange> windows;
	for (const StreamRange& hole : holes)
	{
		if ((int)windows.size() >= DupeStreamRepair::ProbeCount * 2)
		{
			break;
		}
		int64 lowClip = 0;
		int64 highClip = repairSet.Map->GetInnerSize();
		for (const StreamRange& other : originalHoles)
		{
			// any original hole starting before the before-window's end
			// bounds it from below; any one ending past the after-window's
			// start bounds it from above (equivalent to the plain neighbor
			// clip while no donor has written, since holes are disjoint)
			if (other.Offset < hole.Offset)
			{
				lowClip = std::max(lowClip, other.End());
			}
			if (other.End() > hole.End())
			{
				highClip = std::min(highClip, other.Offset);
			}
		}
		int64 beforeFrom = std::max(lowClip,
			hole.Offset - DupeStreamRepair::MinProbeCompareBytes);
		if (beforeFrom < hole.Offset)
		{
			windows.push_back({beforeFrom, hole.Offset - beforeFrom});
		}
		int64 afterTo = std::min(highClip,
			hole.End() + DupeStreamRepair::MinProbeCompareBytes);
		if (hole.End() < afterTo)
		{
			windows.push_back({hole.End(), afterTo - hole.End()});
		}
	}

	// neighboring holes hugging a small present island produce windows over
	// the SAME bytes (A's after-window == B's before-window): coalesce so
	// shared bytes count once toward achievable and totalCompared, or the
	// 64-byte identity floor silently weakens
	return ContentMapper::CoalesceRanges(std::move(windows));
}

/* the plaintext-target compare floor: everything reachable must match,
 * scaled to the mapped present bytes (anchored to the ORIGINAL holes:
 * donor-filled bytes are not primary evidence), clamped to what the
 * windows can actually compare, never below 64. Returns -1 when fewer
 * than 64 bytes are reachable - identity unknowable, par2 owns the set. */
int64 StreamRepairController::PlainCompareFloor(RepairSetData& repairSet,
	const std::vector<StreamRange>& windows)
{
	ContentMap& targetMap = *repairSet.Map;
	int64 mappedBytes = 0;
	for (const ContentRun& run : *targetMap.GetRuns())
	{
		mappedBytes += run.Size;
	}
	int64 base = DupeStreamRepair::BaseCompareFloor(
		mappedBytes - DupeStreamRepair::TotalSize(repairSet.OriginalInnerHoles));
	int64 achievable = 0;
	for (const StreamRange& window : windows)
	{
		for (const MemberRange& piece : targetMap.MapFromInner(window))
		{
			achievable += piece.Range.Size;
		}
	}
	if (achievable < 64)
	{
		return -1;
	}
	return std::min(base, achievable);
}

bool StreamRepairController::VerifyDonorSet(RepairSetData& repairSet, ContentMap& donorMap,
	DonorSetSources& donorSources, TargetSetFiles& targetFiles)
{
	ContentMap& targetMap = *repairSet.Map;

	std::vector<StreamRange> windows = BuildProbeWindows(repairSet);

	// an encrypted target is verified in CIPHERTEXT space: the window
	// semantics (anchoring, coalescing) are identical, only the expected-bytes
	// computation differs
	if (targetMap.GetEncrypted())
	{
		return VerifyDonorSetEncrypted(repairSet, donorMap, donorSources,
			targetFiles, windows);
	}

	// floor semantics as in M1 (Task 1)
	int64 required = PlainCompareFloor(repairSet, windows);
	if (required < 0)
	{
		return false;	// identity unknowable - par2 owns this set
	}

	int64 totalCompared = 0;
	bool sawVariedData = false;
	std::vector<char> donorData;

	for (const StreamRange& window : windows)
	{
		if (IsStopped())
		{
			return false;
		}
		if (totalCompared >= required && sawVariedData)
		{
			break;
		}
		if (!ReadDonorInner(donorMap, donorSources, window, donorData))
		{
			continue;	// inconclusive: the donor may miss these articles
		}
		for (const MemberRange& piece : targetMap.MapFromInner(window))
		{
			StreamRangeList innerPos = targetMap.MapToInner(piece.MemberIndex, piece.Range);
			DiskFile* file = targetFiles.GetFile(piece.MemberIndex);
			if (innerPos.size() != 1 || !file)
			{
				return false;
			}
			const char* pieceData = donorData.data() + (innerPos[0].Offset - window.Offset);
			if (!CompareToFile(*file, piece.Range.Offset, pieceData, piece.Range.Size))
			{
				return false;	// same inner size but different bytes
			}
			totalCompared += piece.Range.Size;
			for (int64 i = 1; i < piece.Range.Size && !sawVariedData; i++)
			{
				sawVariedData = pieceData[i] != pieceData[0];
			}
		}
	}

	return totalCompared >= required && sawVariedData;
}

/* writes `data` (the bytes of innerRange, index 0 = innerRange.Offset) through
 * the target map with the M2 containment guards: every piece must be provably
 * inside a captured hole of a known repair target - never overwrite bytes the
 * primary (or an earlier donor) owns. Returns the bytes written, or -1 when a
 * write was blocked or failed (the caller must stop patching this set). */
int64 StreamRepairController::WriteInnerRange(ContentMap& targetMap, TargetSetFiles& targetFiles,
	std::vector<RepairTarget>& targets, const std::vector<int>& memberTargets,
	const std::vector<SetMember>& setMembers, const StreamRange& innerRange, const char* data)
{
	int64 written = 0;
	for (const MemberRange& piece : targetMap.MapFromInner(innerRange))
	{
		StreamRangeList innerPos = targetMap.MapToInner(piece.MemberIndex, piece.Range);
		DiskFile* file = targetFiles.GetFile(piece.MemberIndex);
		if (innerPos.size() != 1 || !file)
		{
			return -1;
		}

		// defense in depth: fail closed unless the write is provably
		// inside a captured hole of a known repair target
		if (memberTargets[piece.MemberIndex] < 0)
		{
			PrintMessage(Message::mkWarning,
				"Stream repair write outside a repair target blocked for %s",
				setMembers[piece.MemberIndex].Name.c_str());
			return -1;
		}
		RepairTarget& pieceTarget = targets[memberTargets[piece.MemberIndex]];
		StreamRangeList remainder = { piece.Range };
		for (const StreamRange& targetHole : pieceTarget.Holes)
		{
			DupeStreamRepair::SubtractCovered(remainder, targetHole);
		}
		if (!remainder.empty())
		{
			PrintMessage(Message::mkWarning,
				"Stream repair write outside a captured hole blocked for %s",
				setMembers[piece.MemberIndex].Name.c_str());
			return -1;
		}

		file->Seek(piece.Range.Offset);
		if (file->Position() != piece.Range.Offset ||
			file->Write(data + (innerPos[0].Offset - innerRange.Offset),
				piece.Range.Size) != piece.Range.Size)
		{
			PrintMessage(Message::mkError,
				"Could not write to %s during stream repair: %s",
				setMembers[piece.MemberIndex].Name.c_str(),
				*FileSystem::GetLastErrorMessage());
			return -1;
		}
		DupeStreamRepair::SubtractCovered(pieceTarget.Holes, piece.Range);
		written += piece.Range.Size;
	}
	return written;
}

int64 StreamRepairController::PatchFromDonorSet(RepairSetData& repairSet, ContentMap& donorMap,
	DonorSetSources& donorSources, TargetSetFiles& targetFiles,
	std::vector<RepairTarget>& targets, const std::vector<int>& memberTargets,
	const std::vector<SetMember>& setMembers, const char* donorName)
{
	ContentMap& targetMap = *repairSet.Map;

	// an encrypted target needs whole-cipher-block patching with boundary
	// verification; the plain path below also serves encrypted DONORS
	// transparently (ReadDonorInner decrypts through the donor map)
	if (targetMap.GetEncrypted())
	{
		return PatchFromDonorSetEncrypted(repairSet, donorMap, donorSources,
			targetFiles, targets, memberTargets, setMembers, donorName);
	}

	int64 written = 0;
	std::vector<char> donorData;

	StreamRangeList holes = repairSet.InnerHoles;	// iterate a stable copy
	for (const StreamRange& hole : holes)
	{
		int64 pos = hole.Offset;
		while (pos < hole.End() && !IsStopped())
		{
			int64 chunk = std::min<int64>(hole.End() - pos, 4 * 1024 * 1024);
			if (!ReadDonorInner(donorMap, donorSources, {pos, chunk}, donorData))
			{
				break;	// this donor cannot supply the rest of this hole
			}
			int64 wrote = WriteInnerRange(targetMap, targetFiles, targets,
				memberTargets, setMembers, {pos, chunk}, donorData.data());
			if (wrote < 0)
			{
				return written;
			}
			written += wrote;
			DupeStreamRepair::SubtractCovered(repairSet.InnerHoles, {pos, chunk});
			pos += chunk;
		}
	}

	if (written > 0)
	{
		int recoveredParts = donorSources.TakeServedParts();
		m_recoveredBytes += written;
		PrintMessage(Message::mkInfo,
			"Recovered %.1f MB (%i donor article(s)) of %s from duplicate %s (cross-packing)",
			written / 1024.0 / 1024.0, recoveredParts,
			repairSet.Map->GetInnerName(), donorName);
	}
	return written;
}

/* assembles raw ciphertext of the target's contiguous cipher space from its
 * member files on disk (a 16-byte block regularly straddles two volumes);
 * false when any byte is unmappable or unreadable */
bool StreamRepairController::ReadTargetCipher(ContentMap& targetMap, TargetSetFiles& targetFiles,
	const StreamRange& cipherRange, char* buffer)
{
	std::vector<MemberRange> pieces = targetMap.MapCipherRange(cipherRange);
	if (pieces.empty() && cipherRange.Size > 0)
	{
		return false;
	}
	int64 filled = 0;
	for (const MemberRange& piece : pieces)
	{
		DiskFile* file = targetFiles.GetFile(piece.MemberIndex);
		if (!file)
		{
			return false;
		}
		file->Seek(piece.Range.Offset);
		if (file->Position() != piece.Range.Offset ||
			file->Read(buffer + filled, piece.Range.Size) != piece.Range.Size)
		{
			return false;
		}
		filled += piece.Range.Size;
	}
	return true;
}

/* the encrypted-target probe (M3): identity evidence in CIPHERTEXT space. Per
 * window the donor's plaintext is fetched for the covering whole cipher
 * blocks, re-encrypted under the TARGET's stream context - chained from the
 * predecessor cipher block read from the target's own disk, or the header IV
 * at stream start - and byte-compared against the target's on-disk ciphertext.
 * A wrong password, wrong donor or wrong block arithmetic fails the compare:
 * rejection, never a write. Window semantics (anchoring to the original holes,
 * coalescing, the 64-byte floor, varied-data demand) are M2's. */
bool StreamRepairController::VerifyEncryptedFromSource(RepairSetData& repairSet,
	ContentSource& plaintext, TargetSetFiles& targetFiles,
	const std::vector<StreamRange>& windows)
{
	constexpr int64 blockSize = RarCryptoContext::CryptoBlockSize;
	ContentMap& targetMap = *repairSet.Map;
	const StreamRangeList& originalHoles = repairSet.OriginalInnerHoles;
	const RunCrypto* runCrypto = targetMap.GetRunCrypto(0);
	if (!runCrypto)
	{
		return false;
	}

	// the final partial plaintext block (innerSize % 16 != 0) is never
	// checkable: its ciphertext depends on the poster's padding bytes, which
	// no donor carries
	int64 lastComputable = targetMap.GetInnerSize() / blockSize * blockSize;

	// the recomputation must chain from a TRUSTWORTHY predecessor block: one
	// fully outside the ORIGINAL holes (identity evidence anchors to primary
	// bytes only), or the header IV at position 0. Advancing forfeits the
	// window's head - fail toward less evidence, never toward trusting garbage
	auto chainStart = [&originalHoles](const StreamRange& window, int64 windowEnd)
	{
		int64 blockFrom = window.Offset / blockSize * blockSize;
		while (blockFrom > 0 && blockFrom < windowEnd)
		{
			bool prevPresent = true;
			for (const StreamRange& hole : originalHoles)
			{
				prevPresent &= hole.End() <= blockFrom - blockSize ||
					hole.Offset >= blockFrom;
			}
			if (prevPresent)
			{
				break;
			}
			blockFrom += blockSize;
		}
		return blockFrom;
	};

	// floor semantics as in the plain path, with the achievable evidence
	// clipped by the last computable block and each window's chain start
	int64 mappedBytes = 0;
	for (const ContentRun& run : *targetMap.GetRuns())
	{
		mappedBytes += run.Size;
	}
	int64 base = DupeStreamRepair::BaseCompareFloor(
		mappedBytes - DupeStreamRepair::TotalSize(originalHoles));
	int64 achievable = 0;
	for (const StreamRange& window : windows)
	{
		int64 windowEnd = std::min(window.End(), lastComputable);
		int64 from = std::max(window.Offset, chainStart(window, windowEnd));
		if (from < windowEnd)
		{
			achievable += windowEnd - from;
		}
	}
	if (achievable < 64)
	{
		return false;	// identity unknowable in cipher space - par2 owns this set
	}
	int64 required = std::min(base, achievable);

	int64 totalCompared = 0;
	bool sawVariedData = false;
	std::vector<char> donorData;
	std::vector<uint8> cipher;

	for (const StreamRange& window : windows)
	{
		if (IsStopped())
		{
			return false;
		}
		if (totalCompared >= required && sawVariedData)
		{
			break;
		}
		int64 windowEnd = std::min(window.End(), lastComputable);
		int64 blockFrom = chainStart(window, windowEnd);
		int64 from = std::max(window.Offset, blockFrom);
		if (from >= windowEnd)
		{
			continue;
		}
		int64 blockTo = (windowEnd + blockSize - 1) / blockSize * blockSize;
		uint8 prev[RarCryptoContext::CryptoBlockSize];
		if (blockFrom > 0 && !ReadTargetCipher(targetMap, targetFiles,
			{blockFrom - blockSize, blockSize}, (char*)prev))
		{
			continue;	// unreadable chain input - no evidence from this window
		}
		// boundary blocks need the donor's plaintext OUTSIDE the window too;
		// if the donor cannot supply the whole expansion, the window drops
		donorData.resize((size_t)(blockTo - blockFrom));
		if (!plaintext.Read(blockFrom, donorData.data(), blockTo - blockFrom))
		{
			continue;	// inconclusive: the donor may miss these articles
		}
		cipher.resize((size_t)(blockTo - blockFrom));
		if (!runCrypto->Crypto->EncryptRange(blockFrom > 0 ? prev : nullptr,
			(const uint8*)donorData.data(), cipher.data(),
			(blockTo - blockFrom) / blockSize))
		{
			continue;
		}
		std::vector<MemberRange> pieces = targetMap.MapCipherRange({from, windowEnd - from});
		if (pieces.empty())
		{
			continue;
		}
		int64 seen = 0;
		for (const MemberRange& piece : pieces)
		{
			DiskFile* file = targetFiles.GetFile(piece.MemberIndex);
			if (!file)
			{
				return false;
			}
			if (!CompareToFile(*file, piece.Range.Offset,
				(const char*)cipher.data() + (from - blockFrom) + seen, piece.Range.Size))
			{
				return false;	// same geometry, different ciphertext - wrong donor
			}
			seen += piece.Range.Size;
		}
		totalCompared += windowEnd - from;

		// constant PLAINTEXT proves nothing even in cipher space (any
		// zero-padded sibling matches): variedness is judged on the plaintext
		const char* plainWindow = donorData.data() + (from - blockFrom);
		for (int64 i = 1; i < windowEnd - from && !sawVariedData; i++)
		{
			sawVariedData = plainWindow[i] != plainWindow[0];
		}
	}

	return totalCompared >= required && sawVariedData;
}

/* the encrypted-target patch (M3): per hole, expand to whole cipher blocks,
 * fetch the donor's plaintext for the expansion, re-encrypt under the target's
 * stream context (chained from the predecessor block on disk, the header IV at
 * stream start, or the previous chunk's recomputed tail), VERIFY every
 * recomputed byte that is present on disk (the boundary tripwire - a mismatch
 * means the chain, donor or password is wrong and aborts this donor for this
 * set), and write ONLY bytes inside captured holes through the M2 guards. */
int64 StreamRepairController::PatchEncryptedFromSource(RepairSetData& repairSet,
	ContentSource& plaintext, TargetSetFiles& targetFiles,
	std::vector<RepairTarget>& targets, const std::vector<int>& memberTargets,
	const std::vector<SetMember>& setMembers, const char* donorName)
{
	constexpr int64 blockSize = RarCryptoContext::CryptoBlockSize;
	ContentMap& targetMap = *repairSet.Map;
	const RunCrypto* runCrypto = targetMap.GetRunCrypto(0);
	if (!runCrypto)
	{
		return 0;
	}
	// bytes at or past the last full plaintext block are unpatchable: their
	// cipher depends on the poster's padding, which no donor can supply
	int64 lastComputable = targetMap.GetInnerSize() / blockSize * blockSize;
	int64 written = 0;
	int64 uncomputableTail = 0;
	std::vector<char> donorData;
	std::vector<uint8> cipher;

	StreamRangeList holes = repairSet.InnerHoles;	// iterate a stable copy
	for (const StreamRange& hole : holes)
	{
		if (IsStopped())
		{
			break;
		}

		// the live remainder: an earlier hole's block expansion may have
		// already filled parts of this one
		int64 holeFrom = -1;
		int64 holeTo = -1;
		for (const StreamRange& live : repairSet.InnerHoles)
		{
			int64 from = std::max(live.Offset, hole.Offset);
			int64 to = std::min(live.End(), hole.End());
			if (from < to)
			{
				holeFrom = holeFrom < 0 ? from : holeFrom;
				holeTo = to;
			}
		}
		if (holeFrom < 0)
		{
			continue;	// already covered
		}
		int64 patchTo = std::min(holeTo, lastComputable);
		uncomputableTail += holeTo - std::max(patchTo, holeFrom);
		if (patchTo <= holeFrom)
		{
			continue;
		}

		int64 blockFrom = holeFrom / blockSize * blockSize;
		int64 blockTo = (patchTo + blockSize - 1) / blockSize * blockSize;

		// the chain input for the first block: the predecessor cipher block
		// from the target's disk - REQUIRED to be outside every current hole
		// (its bytes are primary or verified donor writes), or the header IV.
		// Chaining from garbage could write garbage with no tripwire when the
		// hole edges are block-aligned, so this check is load-bearing.
		uint8 prev[RarCryptoContext::CryptoBlockSize];
		bool havePrev = false;
		if (blockFrom > 0)
		{
			bool prevPresent = true;
			for (const StreamRange& live : repairSet.InnerHoles)
			{
				prevPresent &= live.End() <= blockFrom - blockSize ||
					live.Offset >= blockFrom;
			}
			if (!prevPresent || !ReadTargetCipher(targetMap, targetFiles,
				{blockFrom - blockSize, blockSize}, (char*)prev))
			{
				PrintMessage(Message::mkInfo,
					"Skipping a hole of %s for duplicate %s: predecessor cipher block is missing",
					targetMap.GetInnerName(), donorName);
				continue;
			}
			havePrev = true;
		}

		while (blockFrom < blockTo && !IsStopped())
		{
			int64 chunkEnd = std::min<int64>(blockFrom + 4 * 1024 * 1024, blockTo);
			donorData.resize((size_t)(chunkEnd - blockFrom));
			if (!plaintext.Read(blockFrom, donorData.data(), chunkEnd - blockFrom))
			{
				break;	// this donor cannot supply the rest of this hole
			}
			cipher.resize((size_t)(chunkEnd - blockFrom));
			if (!runCrypto->Crypto->EncryptRange(havePrev ? prev : nullptr,
				(const uint8*)donorData.data(), cipher.data(),
				(chunkEnd - blockFrom) / blockSize))
			{
				break;
			}

			// partition the chunk by the LIVE holes: hole bytes get written,
			// every other byte must equal its recomputation exactly
			StreamRangeList holeParts;
			for (const StreamRange& live : repairSet.InnerHoles)
			{
				int64 from = std::max(live.Offset, blockFrom);
				int64 to = std::min(live.End(), chunkEnd);
				if (from < to)
				{
					holeParts.push_back({from, to - from});
				}
			}
			StreamRangeList presentParts = { {blockFrom, chunkEnd - blockFrom} };
			for (const StreamRange& part : holeParts)
			{
				DupeStreamRepair::SubtractCovered(presentParts, part);
			}

			// verify BEFORE writing: block-expansion bytes OUTSIDE holes are
			// verified-match, never written
			for (const StreamRange& part : presentParts)
			{
				std::vector<MemberRange> pieces = targetMap.MapCipherRange(part);
				if (pieces.empty())
				{
					PrintMessage(Message::mkInfo,
						"Skipping packing of duplicate %s for %s: unmappable boundary bytes",
						donorName, targetMap.GetInnerName());
					return written;	// cannot verify - fail closed
				}
				int64 seen = 0;
				for (const MemberRange& piece : pieces)
				{
					DiskFile* file = targetFiles.GetFile(piece.MemberIndex);
					if (!file || !CompareToFile(*file, piece.Range.Offset,
						(const char*)cipher.data() + (part.Offset - blockFrom) + seen,
						piece.Range.Size))
					{
						// the corruption tripwire, not an error: this donor is
						// dropped for the whole set (mkInfo on purpose)
						PrintMessage(Message::mkInfo,
							"Skipping packing of duplicate %s for %s: boundary block mismatch",
							donorName, targetMap.GetInnerName());
						return written;
					}
					seen += piece.Range.Size;
				}
			}

			for (const StreamRange& part : holeParts)
			{
				int64 wrote = WriteInnerRange(targetMap, targetFiles, targets,
					memberTargets, setMembers, part,
					(const char*)cipher.data() + (part.Offset - blockFrom));
				if (wrote < 0)
				{
					return written;
				}
				written += wrote;
				DupeStreamRepair::SubtractCovered(repairSet.InnerHoles, part);
			}

			// the next chunk chains from this chunk's recomputed tail block
			memcpy(prev, cipher.data() + (chunkEnd - blockFrom) - blockSize, blockSize);
			havePrev = true;
			blockFrom = chunkEnd;
		}
	}

	if (uncomputableTail > 0)
	{
		PrintMessage(Message::mkInfo,
			"Cross-packing repair of %s: %lli byte(s) in the final partial cipher block stay for par-repair",
			targetMap.GetInnerName(), (long long)uncomputableTail);
	}
	// the caller owns the recovered accounting + the "Recovered ..." log: a
	// mapped donor counts served articles, an extracted file counts holes
	return written;
}

/* M3 entry: the encrypted target's donor is a MAPPED duplicate. Wraps the
 * donor map as the plaintext source and does the mapped-donor recovered
 * accounting (served articles) + the "(cross-packing)" log. */
bool StreamRepairController::VerifyDonorSetEncrypted(RepairSetData& repairSet,
	ContentMap& donorMap, DonorSetSources& donorSources, TargetSetFiles& targetFiles,
	const std::vector<StreamRange>& windows)
{
	DonorInnerPlaintextSource plaintext(donorMap, donorSources);
	return VerifyEncryptedFromSource(repairSet, plaintext, targetFiles, windows);
}

int64 StreamRepairController::PatchFromDonorSetEncrypted(RepairSetData& repairSet,
	ContentMap& donorMap, DonorSetSources& donorSources, TargetSetFiles& targetFiles,
	std::vector<RepairTarget>& targets, const std::vector<int>& memberTargets,
	const std::vector<SetMember>& setMembers, const char* donorName)
{
	DonorInnerPlaintextSource plaintext(donorMap, donorSources);
	int64 written = PatchEncryptedFromSource(repairSet, plaintext, targetFiles,
		targets, memberTargets, setMembers, donorName);
	if (written > 0)
	{
		int recoveredParts = donorSources.TakeServedParts();
		m_recoveredBytes += written;
		PrintMessage(Message::mkInfo,
			"Recovered %.1f MB (%i donor article(s)) of %s from duplicate %s (cross-packing)",
			written / 1024.0 / 1024.0, recoveredParts,
			repairSet.Map->GetInnerName(), donorName);
	}
	return written;
}

/* the M4 rung (option <DupeStreamDecompress>): materialize the donor set's
 * member files into a scratch dir under DestDir, run the configured extractor
 * (unrar/7z) on the main volume, pick the extracted file matching the target's
 * inner size, and let it donate through the plain inner-space verify/patch
 * path. Every degradation is a logged skip - never an error, never a write;
 * the scratch tree is deleted on EVERY exit path (skip, success, exception,
 * stop). Runs UNLOCKED except for the progress-label set. */
void StreamRepairController::ExecDecompressRepair(const char* destDir,
	RepairSetData& repairSet, NzbInfo* donorNzb,
	const std::vector<SetMember>& donorMembers, const MemberSet& donorSet,
	const DonorSource& donor, TargetSetFiles& targetFiles,
	std::vector<RepairTarget>& targets, const std::vector<int>& memberTargets,
	const std::vector<SetMember>& setMembers)
{
	const char* setName = donorMembers[donorSet.Members[0]].Name.c_str();

	// the main volume is the member the extractor accepts as an entry point
	// (rar: .rar/.part01.rar = data-order first; 7z: .7z/.7z.001 = first;
	// spanned zip: the final .zip = data-order LAST); the other volumes are
	// found next to it in the scratch dir
	const char* mainVolume = nullptr;
	for (int memberIndex : donorSet.Members)
	{
		const std::string& memberName = donorMembers[memberIndex].Name;
		if (Unpack::IsArchive(fs::path(memberName)))
		{
			mainVolume = memberName.c_str();
			break;
		}
	}
	if (!mainVolume)
	{
		PrintMessage(Message::mkInfo,
			"Skipping decompression of %s of duplicate %s: no extractable main volume",
			setName, *donor.InfoName);
		return;
	}

	// probe the extractor tool BEFORE materializing up to MaxDecompressBytes:
	// a missing/unconfigured unrar/7z is a static config state, so without it
	// the whole donor download would be wasted for every gated pair. Mirror
	// the validation MakeExtractor itself does (dispatch order: 7z, then
	// unrar); the real MakeExtractor call below stays the authoritative gate.
	fs::path mainVolumePath(mainVolume);
	const fs::path& toolPath = Unpack::SevenZip::IsSupported(mainVolumePath) ?
		g_Options->GetSevenZipPath() : g_Options->GetUnrarPath();
	fs::error_code toolEc;
	if (toolPath.empty() || !fs::exists(toolPath, toolEc) || toolEc)
	{
		PrintMessage(Message::mkInfo,
			"Skipping decompression of %s of duplicate %s: no extraction tool configured",
			setName, *donor.InfoName);
		return;
	}

	// a unique scratch dir (a stale leftover from a crashed run is never reused)
	BString<1024> tempDir;
	for (int suffix = 0; ; suffix++)
	{
		tempDir.Format("%s%c.stream-decompress.%i", destDir, PATH_SEPARATOR, suffix);
		if (FileSystem::CreateDirectoryExclusive(tempDir))
		{
			break;
		}
		if (errno != EEXIST)
		{
			PrintMessage(Message::mkInfo,
				"Skipping decompression of %s of duplicate %s: could not create %s: %s",
				setName, *donor.InfoName, *tempDir, *FileSystem::GetLastErrorMessage());
			return;
		}
	}

	// removes the whole scratch tree on every exit from this scope; declared
	// before any DiskFile over its content so files close before the delete
	struct TempDirGuard
	{
		CString Dir;
		~TempDirGuard()
		{
			CString errmsg;
			FileSystem::DeleteDirectoryWithContent(Dir, errmsg);
		}
	} tempDirGuard{CString(*tempDir)};

	if (m_postInfo)
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		m_postInfo->SetProgressLabel(BString<1024>(
			"Decompressing duplicate %s", *donor.InfoName));
	}

	int64 totalBytes = 0;
	if (!MaterializeDonorSet(donorNzb, donorMembers, donorSet, tempDir, totalBytes))
	{
		PrintMessage(Message::mkInfo,
			"Skipping decompression of %s of duplicate %s: donor not materializable (stopped, incomplete, over the size cap or disk error)",
			setName, *donor.InfoName);
		return;
	}

	BString<1024> archivePath("%s%c%s", *tempDir, PATH_SEPARATOR, mainVolume);
	BString<1024> extractDir("%s%cextracted", *tempDir, PATH_SEPARATOR);

	// the donor's own password unlocks its encrypted archive: passed to the
	// extractor, NEVER logged. MakeExtractor validates the configured tool
	Unpack::ExtractorPtr extractor = Unpack::MakeExtractor(fs::path(*archivePath),
		fs::path(*extractDir),
		donor.Password.Empty() ? std::string() : std::string(*donor.Password),
		Unpack::OverwriteMode::Skip);
	if (!extractor || !extractor->Extract())
	{
		PrintMessage(Message::mkInfo,
			"Skipping decompression of %s of duplicate %s: %s",
			setName, *donor.InfoName,
			extractor ? "extraction failed" : "no extraction tool configured");
		return;
	}

	std::string innerPath = DupeStreamRepair::SelectExtractedInner(extractDir,
		repairSet.Map->GetInnerSize(), repairSet.Map->GetInnerName());
	if (innerPath.empty())
	{
		PrintMessage(Message::mkInfo,
			"Skipping decompression of %s of duplicate %s: no extracted file matches inner file %s",
			setName, *donor.InfoName, repairSet.Map->GetInnerName());
		return;
	}

	DiskFile innerFile;
	if (!innerFile.Open(innerPath.c_str(), DiskFile::omRead))
	{
		PrintMessage(Message::mkInfo,
			"Skipping decompression of %s of duplicate %s: could not open the extracted file",
			setName, *donor.InfoName);
		return;
	}

	int64 innerSize = FileSystem::FileSize(innerPath.c_str());
	bool encrypted = repairSet.Map->GetEncrypted();

	bool verified = encrypted ?
		VerifyDonorInnerFileEncrypted(repairSet, innerFile, innerSize, targetFiles) :
		VerifyDonorInnerFile(repairSet, innerFile, innerSize, targetFiles);
	if (!verified)
	{
		// mkInfo on purpose: a rejected donor is the safety net firing -
		// it must be visible in the log
		PrintMessage(Message::mkInfo,
			"Skipping decompression of %s of duplicate %s for %s: content identity not confirmed",
			setName, *donor.InfoName, repairSet.Map->GetInnerName());
		innerFile.Close();
		return;
	}

	if (encrypted)
	{
		PatchFromDonorInnerFileEncrypted(repairSet, innerFile, innerSize, targetFiles,
			targets, memberTargets, setMembers, donor.InfoName);
	}
	else
	{
		PatchFromDonorInnerFile(repairSet, innerFile, targetFiles, targets,
			memberTargets, setMembers, donor.InfoName);
	}
	innerFile.Close();
}

/* fetches every article of every member of `set` and writes the decoded bytes
 * at their declared offsets into tempDir/<member basename>. A member that
 * misses some of its own articles still materializes what it has (extraction
 * then typically fails and degrades to a skip). false = do not extract:
 * stopped, an article-less member, over MaxDecompressBytes, or a disk error.
 * Runs UNLOCKED - fetches and disk I/O never hold the queue lock. */
bool StreamRepairController::MaterializeDonorSet(NzbInfo* donorNzb,
	const std::vector<SetMember>& donorMembers, const MemberSet& set,
	const char* tempDir, int64& totalBytes)
{
	// index-aligned with donorMembers (DonorSetSources::BuildMembers walks
	// the same list in the same order)
	std::vector<FileInfo*> files;
	for (FileInfo* fileInfo : donorNzb->GetFileList())
	{
		files.push_back(fileInfo);
	}

	// per-attempt (whole donor set) accounting: totalBytes bounds the decoded
	// bytes actually written; totalExtent bounds the on-disk file EXTENTS - N
	// members each declaring a huge size (one tiny high-offset article) would
	// otherwise seek-write up to N x MaxDecompressBytes of zero-fill on a
	// non-sparse filesystem before we ever try to extract
	int64 totalExtent = 0;

	for (int memberIndex : set.Members)
	{
		if (memberIndex < 0 || memberIndex >= (int)files.size())
		{
			return false;
		}
		FileInfo* donorFile = files[memberIndex];
		if (donorFile->GetArticles()->empty())
		{
			return false;	// nothing to materialize for this volume
		}

		BString<1024> path("%s%c%s", tempDir, PATH_SEPARATOR,
			donorMembers[memberIndex].Name.c_str());
		DiskFile file;
		if (!file.Open(path, DiskFile::omWrite))
		{
			PrintMessage(Message::mkWarning,
				"Could not create %s for donor materialization: %s",
				*path, *FileSystem::GetLastErrorMessage());
			return false;
		}

		int64 memberExtent = 0;	// this member's highest write end (its real size)
		for (std::unique_ptr<ArticleInfo>& article : *donorFile->GetArticles())
		{
			if (IsStopped())
			{
				return false;
			}
			ArticleFetcher::FetchedArticle fetched = m_fetcher.Fetch(
				article->GetMessageId(), *donorFile->GetGroups());
			if (!fetched.Success || fetched.Data.empty() || fetched.Offset < 0 ||
				fetched.Offset + (int64)fetched.Data.size() > fetched.FileSize)
			{
				continue;	// the donor misses this article - the extractor decides
			}
			int64 writeEnd = fetched.Offset + (int64)fetched.Data.size();
			// the caps bound BOTH the accumulated decoded bytes AND the
			// accumulated file extents across ALL members: prior members by
			// their real materialized extent, this member by its declared size
			// (a stable per-member upper bound that bails before its download)
			if (totalBytes + (int64)fetched.Data.size() > DupeStreamRepair::MaxDecompressBytes ||
				totalExtent + fetched.FileSize > DupeStreamRepair::MaxDecompressBytes)
			{
				return false;
			}
			file.Seek(fetched.Offset);
			if (file.Position() != fetched.Offset ||
				file.Write(fetched.Data.data(), fetched.Data.size()) !=
					(int64)fetched.Data.size())
			{
				PrintMessage(Message::mkWarning,
					"Could not write to %s during donor materialization: %s",
					*path, *FileSystem::GetLastErrorMessage());
				return false;
			}
			totalBytes += fetched.Data.size();
			memberExtent = std::max(memberExtent, writeEnd);
		}
		totalExtent += memberExtent;
		file.Close();
	}
	return true;
}

/* the M4 identity probe: the extracted donor file must byte-match the target's
 * OWN downloaded bytes around the holes (window and floor semantics shared
 * with M2 via BuildProbeWindows/PlainCompareFloor). PLAINTEXT targets only
 * (v1) - composing extracted plaintext with the encrypted-target write path is
 * future work. Nothing is written before this passes; a wrong or corrupt
 * extraction fails the compare; par2 stays the final verifier. */
bool StreamRepairController::VerifyDonorInnerFile(RepairSetData& repairSet,
	DiskFile& donorInner, int64 donorInnerSize, TargetSetFiles& targetFiles)
{
	ContentMap& targetMap = *repairSet.Map;
	if (targetMap.GetEncrypted() || donorInnerSize != targetMap.GetInnerSize())
	{
		return false;
	}

	std::vector<StreamRange> windows = BuildProbeWindows(repairSet);
	int64 required = PlainCompareFloor(repairSet, windows);
	if (required < 0)
	{
		return false;	// identity unknowable - par2 owns this set
	}

	DiskContentSource donorSource(donorInner, donorInnerSize);
	int64 totalCompared = 0;
	bool sawVariedData = false;
	std::vector<char> donorData;

	for (const StreamRange& window : windows)
	{
		if (IsStopped())
		{
			return false;
		}
		if (totalCompared >= required && sawVariedData)
		{
			break;
		}
		donorData.resize(window.Size);
		if (!donorSource.Read(window.Offset, donorData.data(), window.Size))
		{
			continue;	// unreadable extraction output: inconclusive, less evidence
		}
		for (const MemberRange& piece : targetMap.MapFromInner(window))
		{
			StreamRangeList innerPos = targetMap.MapToInner(piece.MemberIndex, piece.Range);
			DiskFile* file = targetFiles.GetFile(piece.MemberIndex);
			if (innerPos.size() != 1 || !file)
			{
				return false;
			}
			const char* pieceData = donorData.data() + (innerPos[0].Offset - window.Offset);
			if (!CompareToFile(*file, piece.Range.Offset, pieceData, piece.Range.Size))
			{
				return false;	// same inner size but different bytes - wrong donor
			}
			totalCompared += piece.Range.Size;
			for (int64 i = 1; i < piece.Range.Size && !sawVariedData; i++)
			{
				sawVariedData = pieceData[i] != pieceData[0];
			}
		}
	}

	return totalCompared >= required && sawVariedData;
}

/* patches every inner hole with bytes read from the extracted donor file.
 * All writes go through WriteInnerRange, whose containment guards (only
 * inside captured holes of known repair targets) stay authoritative and
 * which already subtracts the member-space target holes - only the
 * INNER-space holes are subtracted here (never twice). */
int64 StreamRepairController::PatchFromDonorInnerFile(RepairSetData& repairSet,
	DiskFile& donorInner, TargetSetFiles& targetFiles,
	std::vector<RepairTarget>& targets, const std::vector<int>& memberTargets,
	const std::vector<SetMember>& setMembers, const char* donorName)
{
	ContentMap& targetMap = *repairSet.Map;
	DiskContentSource donorSource(donorInner, targetMap.GetInnerSize());

	int64 written = 0;
	int holesFilled = 0;
	std::vector<char> donorData;

	StreamRangeList holes = repairSet.InnerHoles;	// iterate a stable copy
	for (const StreamRange& hole : holes)
	{
		int64 pos = hole.Offset;
		while (pos < hole.End() && !IsStopped())
		{
			int64 chunk = std::min<int64>(hole.End() - pos, 4 * 1024 * 1024);
			donorData.resize(chunk);
			if (!donorSource.Read(pos, donorData.data(), chunk))
			{
				break;	// unreadable extraction output - the rest stays for par2
			}
			int64 wrote = WriteInnerRange(targetMap, targetFiles, targets,
				memberTargets, setMembers, {pos, chunk}, donorData.data());
			if (wrote < 0)
			{
				return written;
			}
			written += wrote;
			DupeStreamRepair::SubtractCovered(repairSet.InnerHoles, {pos, chunk});
			pos += chunk;
		}
		if (pos >= hole.End())
		{
			holesFilled++;
		}
	}

	if (written > 0)
	{
		m_recoveredHoles += holesFilled;
		m_recoveredBytes += written;
		PrintMessage(Message::mkInfo,
			"Recovered %.1f MB of %s from duplicate %s (decompressed)",
			written / 1024.0 / 1024.0, targetMap.GetInnerName(), donorName);
	}
	return written;
}

/* the encrypted-target counterpart of VerifyDonorInnerFile: the extracted
 * plaintext is re-encrypted under the target's stream context and its
 * ciphertext byte-compared against the target's disk (the M3 verify core),
 * so a wrong extraction or the wrong archive is rejected before any write. */
bool StreamRepairController::VerifyDonorInnerFileEncrypted(RepairSetData& repairSet,
	DiskFile& donorInner, int64 donorInnerSize, TargetSetFiles& targetFiles)
{
	ContentMap& targetMap = *repairSet.Map;
	if (!targetMap.GetEncrypted() || donorInnerSize != targetMap.GetInnerSize())
	{
		return false;
	}

	DiskContentSource plaintext(donorInner, donorInnerSize);
	std::vector<StreamRange> windows = BuildProbeWindows(repairSet);
	return VerifyEncryptedFromSource(repairSet, plaintext, targetFiles, windows);
}

/* the encrypted-target counterpart of PatchFromDonorInnerFile: the extracted
 * plaintext is re-encrypted under the target's stream context and the
 * ciphertext written into the holes (the M3 patch core). The recovered stat
 * counts filled holes, matching the plaintext decompression path. */
int64 StreamRepairController::PatchFromDonorInnerFileEncrypted(RepairSetData& repairSet,
	DiskFile& donorInner, int64 donorInnerSize, TargetSetFiles& targetFiles,
	std::vector<RepairTarget>& targets, const std::vector<int>& memberTargets,
	const std::vector<SetMember>& setMembers, const char* donorName)
{
	ContentMap& targetMap = *repairSet.Map;
	DiskContentSource plaintext(donorInner, donorInnerSize);

	// count holes that go fully empty across the patch (the encrypted core
	// shrinks repairSet.InnerHoles in place, so snapshot the pre-patch holes)
	StreamRangeList before = repairSet.InnerHoles;
	int64 written = PatchEncryptedFromSource(repairSet, plaintext, targetFiles,
		targets, memberTargets, setMembers, donorName);

	if (written > 0)
	{
		int holesFilled = 0;
		for (const StreamRange& hole : before)
		{
			if (!DupeStreamRepair::IntersectsAny(hole, repairSet.InnerHoles))
			{
				holesFilled++;
			}
		}
		m_recoveredHoles += holesFilled;
		m_recoveredBytes += written;
		PrintMessage(Message::mkInfo,
			"Recovered %.1f MB of %s from duplicate %s (decompressed)",
			written / 1024.0 / 1024.0, targetMap.GetInnerName(), donorName);
	}
	return written;
}

void StreamRepairController::RepairCompleted()
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	NzbInfo* nzbInfo = m_postInfo->GetNzbInfo();

	if (m_recoveredArticles > 0 || m_recoveredBytes > 0 || m_recoveredHoles > 0)
	{
		nzbInfo->SetDupeRecoveredArticles(nzbInfo->GetDupeRecoveredArticles() + m_recoveredArticles);
		nzbInfo->SetDupeRecoveredBytes(nzbInfo->GetDupeRecoveredBytes() + m_recoveredBytes);
		nzbInfo->SetDupeRecoveredHoles(nzbInfo->GetDupeRecoveredHoles() + m_recoveredHoles);
	}

	// restore byte-based health in ENCODED (bytes=) units - the units
	// m_currentFailedSize is kept in. For each FULLY-repaired file we credit
	// its exact encoded FailedSize (accumulated in ReportRemainingHoles),
	// which precisely reverses that file's contribution to m_currentFailedSize
	// (DownloadInfo.cpp:675) and, for a par2 file, m_parCurrentFailedSize
	// (:687). So a fully-repaired par-less release reaches m_currentFailedSize
	// == 0 -> CalcHealth() 1000 -> completes as SUCCESS; a partially-repaired
	// file credits nothing, leaving m_currentFailedSize > 0 -> health < 1000,
	// so the release still parks (never a false SUCCESS - crediting per
	// fully-repaired file, not per decoded byte, is the correctness and safety
	// argument). Clamped at 0. When par2 exists SetRequestParCheck below stays
	// authoritative.
	if (m_recoveredFailedSize > 0)
	{
		nzbInfo->SetFailedSize(std::max<int64>(0,
			nzbInfo->GetFailedSize() - m_recoveredFailedSize));
		nzbInfo->SetSuccessSize(nzbInfo->GetSuccessSize() + m_recoveredFailedSize);
		nzbInfo->SetCurrentFailedSize(std::max<int64>(0,
			nzbInfo->GetCurrentFailedSize() - m_recoveredFailedSize));
		nzbInfo->SetCurrentSuccessSize(nzbInfo->GetCurrentSuccessSize() + m_recoveredFailedSize);
		if (m_recoveredFailedParSize > 0)
		{
			nzbInfo->SetParFailedSize(std::max<int64>(0,
				nzbInfo->GetParFailedSize() - m_recoveredFailedParSize));
			nzbInfo->SetParSuccessSize(
				nzbInfo->GetParSuccessSize() + m_recoveredFailedParSize);
			nzbInfo->SetParCurrentFailedSize(std::max<int64>(0,
				nzbInfo->GetParCurrentFailedSize() - m_recoveredFailedParSize));
			nzbInfo->SetParCurrentSuccessSize(
				nzbInfo->GetParCurrentSuccessSize() + m_recoveredFailedParSize);
		}
	}

	// Persist unfinished holes only when the controller is stopping so a
	// restart can resume. During a normal run, unfinished targets are handed to
	// par-repair and must not re-arm this stage indefinitely.
	for (auto it = nzbInfo->GetStreamRepairJobs()->begin();
		it != nzbInfo->GetStreamRepairJobs()->end(); )
	{
		if (!IsStopped())
		{
			it = nzbInfo->GetStreamRepairJobs()->erase(it);
			continue;
		}
		bool matched = false;
		for (const RepairTarget& target : m_targets)
		{
			if (target.FileId == it->GetFileId())
			{
				matched = true;
				if (target.Holes.empty())
				{
					it = nzbInfo->GetStreamRepairJobs()->erase(it);
				}
				else
				{
					it->SetHoles(target.Holes);
					++it;
				}
				break;
			}
		}
		if (!matched)
		{
			// A job without a matching target is retained conservatively.
			++it;
		}
	}

	// whatever was written - and whatever is still missing - goes through
	// par-check as final verification (a no-op when no par2 files exist)
	if (m_recoveredArticles > 0 || m_holesRemain)
	{
		m_postInfo->SetRequestParCheck(true);
	}

	// Persist repaired counters and any remaining holes before leaving the
	// stage, so a graceful stop cannot lose the current repair state.
	downloadQueue->Save();

	m_postInfo->SetWorking(false);
}

void StreamRepairController::AddMessage(Message::EKind kind, const char* text)
{
	if (m_liveMode)
	{
		// no PostInfo in a live pass: find the collection by id (it may have
		// been deleted mid-pass, then the message is dropped). Callers must
		// not hold the DownloadQueue lock when printing in live mode.
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(m_nzbId);
		if (nzbInfo)
		{
			nzbInfo->AddMessage(kind, text);
		}
		return;
	}

	m_postInfo->GetNzbInfo()->AddMessage(kind, text);
}
