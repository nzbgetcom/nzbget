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


#include "nzbget.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <chrono>
#include <mutex>
#include "Fleet.h"
#include "DonorHealth.h"
#include "DupeCoordinator.h"
#include "DupeSearch.h"
#include "HttpGet.h"
#include "NntpHealthServer.h"
#include "NzbReader.h"
#include "Posting.h"
#include "DupeUtil.h"
#include "ReleaseName.h"
#include "Scanner.h"
#include "DownloadInfo.h"
#include "Options.h"
#include "Log.h"
#include "WorkState.h"

namespace
{
	// the largest nzb-file a member may be (as for the duplicate search's fetches)
	constexpr size_t MaxNzbBytes = 64 * 1024 * 1024;
	// postings checked at once at most (each news server limits its own connections)
	constexpr int MaxParallel = 16;
	// articles sampled of each posting at most (after the probe)
	constexpr int FleetSampleMax = 200;
	// time kept from the limit for ranking and adding the members
	constexpr long long FinishReserveMs = 2000;

	struct Candidate
	{
		size_t index;			// in the request
		NzbSummary summary;
		DonorHealth::Health health;
		bool checked = false;
		int alive = -1;
		bool dead = false;
		int sameAs = -1;		// index of the member whose posting this one is
		int existingId = 0;		// the item of the key already in nzbget whose posting this one is
		std::string error;
	};

	// one fleet of a key at a time (F2: a client's retry raced the first call)
	std::mutex g_keyLocksMutex;
	std::map<std::string, std::shared_ptr<std::timed_mutex>> g_keyLocks;

	std::shared_ptr<std::timed_mutex> KeyLock(const std::string& key)
	{
		std::lock_guard<std::mutex> guard(g_keyLocksMutex);
		std::shared_ptr<std::timed_mutex>& lock = g_keyLocks[DupeUtil::Lower(key)];
		if (!lock)
		{
			lock = std::make_shared<std::timed_mutex>();
		}
		return lock;
	}
}

Fleet::Result Fleet::Append(Request request)
{
	Result result;
	request.timeoutSec = std::max(5, std::min(request.timeoutSec, (int)MaxTimeoutSec));
	if (request.members.empty())
	{
		result.reason = "NO_MEMBERS";
		return result;
	}
	// no key given: the one a single append would get (F1-d)
	if (request.dupeKey.empty())
	{
		request.dupeKey = DupeSearch::MakeDupeKey(request.members[0].name);
	}
	// the whole call - the wait for the key, fetches and checks - ends by the time
	// limit (F1-b, F4)
	long long deadlineMs = DonorHealth::NowMs() + request.timeoutSec * 1000LL;
	// a second fleet of the key waits for the first, then sees what it added (F2);
	// not past the time limit (F4: it waited out the first, then checked anew)
	std::shared_ptr<std::timed_mutex> keyLock = KeyLock(request.dupeKey);
	std::unique_lock<std::timed_mutex> keyGuard(*keyLock, std::defer_lock);
	if (!keyGuard.try_lock_for(std::chrono::milliseconds(std::max(0LL,
		deadlineMs - DonorHealth::NowMs() - FinishReserveMs))))
	{
		result.reason = "KEY_BUSY";
		result.complete = false;
		return result;
	}

	// read every member: an nzb-file sent, or fetched from its url
	std::vector<Candidate> candidates(request.members.size());
	for (size_t i = 0; i < request.members.size(); i++)
	{
		Member& member = request.members[i];
		Candidate& candidate = candidates[i];
		candidate.index = i;
		if (!member.url.empty())
		{
			HttpGet::Reply reply = HttpGet::Fetch(member.url, "fleet member " + member.name, MaxNzbBytes,
				deadlineMs - FinishReserveMs);
			if (!reply.ok)
			{
				candidate.error = reply.status ? "the nzb-file could not be fetched (HTTP " + std::to_string(reply.status) + ")" :
					std::string("the nzb-file could not be fetched (connection failed)");
				continue;
			}
			member.data = std::move(reply.body);
		}
		if (!NzbReader::Parse(member.data, candidate.summary) || candidate.summary.messageIds.empty())
		{
			candidate.error = "the nzb-file is unreadable";
			continue;
		}
		// a copy of a posting already in the fleet is checked once, with it
		for (size_t j = 0; j < i; j++)
		{
			if (candidates[j].error.empty() && candidates[j].sameAs < 0 &&
				Posting::SamePosting(candidates[j].summary.messageIds, candidate.summary.messageIds))
			{
				candidate.sameAs = (int)j;
				break;
			}
		}
	}

	// what the key holds already: a download running, and the nzb-files of all its items
	int top = DupeSearch::BasePickScore;
	int runningId = 0;
	int runningScore = 0;
	int lowest = 0;		// the lowest score the key holds
	bool any = false;
	std::vector<std::pair<int, std::string>> existing;	// nzb id, its nzb-file
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		auto consider = [&](NzbInfo* nzbInfo, bool queued)
			{
				if (nzbInfo->GetKind() != NzbInfo::nkNzb || strcasecmp(nzbInfo->GetDupeKey(), request.dupeKey.c_str()))
				{
					return;
				}
				top = std::max(top, nzbInfo->GetDupeScore() + 1000);
				lowest = any ? std::min(lowest, nzbInfo->GetDupeScore()) : nzbInfo->GetDupeScore();
				any = true;
				if (queued && !nzbInfo->GetDeleting() && nzbInfo->GetDeleteStatus() == NzbInfo::dsNone &&
					(!runningId || nzbInfo->GetDupeScore() > runningScore))
				{
					runningId = nzbInfo->GetId();
					runningScore = nzbInfo->GetDupeScore();
				}
				if (!Util::EmptyStr(nzbInfo->GetQueuedFilename()) && !strchr(nzbInfo->GetQueuedFilename(), '|'))
				{
					existing.emplace_back(nzbInfo->GetId(), nzbInfo->GetQueuedFilename());
				}
			};
		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			consider(nzbInfo, true);
		}
		for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
		{
			if (historyInfo->GetKind() == HistoryInfo::hkNzb)
			{
				consider(historyInfo->GetNzbInfo(), false);
			}
		}
	}
	// a member whose posting the key holds already isn't added again (F2: a resent
	// fleet's copy of the whole posting was filed as a copy, and a worse one queued)
	for (const auto& item : existing)
	{
		NzbSummary summary;
		if (!NzbReader::Parse(DupeUtil::ReadAll(item.second), summary))
		{
			continue;
		}
		for (Candidate& candidate : candidates)
		{
			if (candidate.error.empty() && candidate.sameAs < 0 && !candidate.existingId &&
				Posting::SamePosting(summary.messageIds, candidate.summary.messageIds))
			{
				candidate.existingId = item.first;
			}
		}
	}
	// the health of each posting, on the configured servers, within the time limit
	std::vector<DonorHealth::Posting> postings;
	for (Candidate& candidate : candidates)
	{
		// a posting the key holds already needs no check: it isn't added again
		if (candidate.error.empty() && candidate.sameAs < 0 && !candidate.existingId)
		{
			postings.push_back({ std::to_string(candidate.index), candidate.summary.messageIds,
				std::make_shared<std::vector<std::string>>(candidate.summary.groups) });
		}
	}
	DonorHealth::ServerList servers = NntpHealthServer::Servers();
	if (!servers.empty() && !postings.empty())
	{
		DonorHealth::Options options;
		options.sample.percent = g_Options->GetDupeHealthPercent();
		options.sample.minimum = g_Options->GetDupeHealthMin();
		// ranking a few copies needs less than measuring a donor: the sample of each
		// posting is capped (F7: 5% of a 100,000-article posting took about 8 s each)
		options.sample.maximum = std::min(g_Options->GetDupeHealthMax(), FleetSampleMax);
		options.sample.maxBody = g_Options->GetDupeBodyChecks();
		// every posting at once, each with the time left: the check ends by the deadline
		// (one after another, each with its own budget, it took 57 s for a 45 s limit)
		long long checkStart = DonorHealth::NowMs();
		options.budgetMs = (int)std::max(1000LL, deadlineMs - checkStart - FinishReserveMs);
		std::mutex mutex;
		// downloads hold off meanwhile: a download keeps taking the connections back
		// after each article, and the check got none (F7: with a download running,
		// every check used its whole time limit and measured nothing)
		g_WorkState->HoldDownloadForFleet(true);
		DonorHealth::CheckPostings(servers, postings, options, true,
			std::min((int)postings.size(), MaxParallel),
			[&](const std::string& key, const DonorHealth::Health& health)
			{
				std::lock_guard<std::mutex> guard(mutex);
				Candidate& candidate = candidates[(size_t)atoi(key.c_str())];
				candidate.health = health;
				candidate.checked = true;
			});
		g_WorkState->HoldDownloadForFleet(false);
		// a check the deadline cut short knows less than it could
		result.complete = DonorHealth::NowMs() - checkStart < options.budgetMs;
	}
	for (Candidate& candidate : candidates)
	{
		if (!candidate.error.empty() || candidate.sameAs >= 0)
		{
			continue;
		}
		// cut short with nothing found present, a posting is unknown, not dead: its
		// unsettled articles only count as missing at the limit (F7)
		bool unknown = !candidate.checked || (!result.complete && candidate.health.present == 0);
		double alive = unknown ? -1 : candidate.health.Alive();
		candidate.alive = alive < 0 ? -1 : (int)std::lround(100 * alive);
		candidate.dead = !unknown && (DonorHealth::DeadProbe(candidate.health) ||
			(alive >= 0 && alive * 100 < g_Options->GetDupeMinAlive() &&
			 candidate.health.missing >= DonorHealth::MinKnown));
		result.complete &= candidate.checked && candidate.health.Answered() >= DonorHealth::MinKnown;
	}
	for (Candidate& candidate : candidates)
	{
		if (candidate.sameAs >= 0)
		{
			candidate.alive = candidates[candidate.sameAs].alive;
			candidate.dead = candidates[candidate.sameAs].dead;
		}
	}

	// the wholest first; of equal health the size most of the fleet has (the
	// release's own packaging), then the order the client sent them in
	std::vector<long long> sizes;
	for (Candidate& candidate : candidates)
	{
		if (candidate.error.empty())
		{
			sizes.push_back(candidate.summary.totalBytes);
		}
	}
	std::sort(sizes.begin(), sizes.end());
	long long typical = sizes.empty() ? 0 : sizes[sizes.size() / 2];
	auto distance = [typical](const Candidate& c) { return std::llabs(c.summary.totalBytes - typical); };
	auto tier = [](const Candidate& c) { return !c.error.empty() ? 3 : c.dead ? 2 : c.alive < 0 ? 1 : 0; };
	std::vector<Candidate*> order;
	for (Candidate& candidate : candidates)
	{
		if (candidate.sameAs < 0)
		{
			order.push_back(&candidate);
		}
	}
	std::stable_sort(order.begin(), order.end(), [&](const Candidate* a, const Candidate* b)
		{
			if (tier(*a) != tier(*b)) return tier(*a) < tier(*b);
			if (a->alive != b->alive) return a->alive > b->alive;
			return distance(*a) < distance(*b);
		});
	// a same posting follows the one it is a copy of
	std::vector<Candidate*> ranked;
	for (Candidate* candidate : order)
	{
		ranked.push_back(candidate);
		for (Candidate& copy : candidates)
		{
			if (copy.sameAs == (int)candidate->index)
			{
				ranked.push_back(&copy);
			}
		}
	}

	// nzbget shuts down: adding now would wait for the stopped scanner for good (as
	// in B37); the client sends the fleet again after the restart
	if (DonorHealth::Stopping() || HttpGet::Stopped())
	{
		result.reason = "SHUTDOWN";
		result.complete = false;
		return result;
	}

	// the release downloaded under another key (another client's), its files on
	// disk: nothing of the fleet downloads (F1-c); the same release by name, as
	// strictly as the duplicate search compares them
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
		{
			NzbInfo* item = historyInfo->GetKind() == HistoryInfo::hkNzb ? historyInfo->GetNzbInfo() : nullptr;
			if (!item || item->GetDeleteStatus() != NzbInfo::dsNone || !item->IsDupeSuccess() ||
				item->GetMarkStatus() == NzbInfo::ksBad || !ReleaseName::Readable(item->GetName()))
			{
				continue;
			}
			bool same = std::any_of(ranked.begin(), ranked.end(), [&](const Candidate* c)
				{
					const std::string& name = request.members[c->index].name;
					return c->error.empty() && ReleaseName::Readable(name) &&
						ReleaseName::SameRelease(name, item->GetName());
				});
			if (same && DupeCoordinator::FilesOnDisk(item))
			{
				result.reason = "ALREADY_DOWNLOADED";
				result.chosen = 0;
				int rank = 0;
				for (Candidate* candidate : ranked)
				{
					Entry entry;
					entry.name = request.members[candidate->index].name;
					entry.rank = ++rank;
					entry.alive = candidate->alive;
					entry.status = candidate->error.empty() ? "SKIPPED" : "ERROR";
					entry.reason = candidate->error.empty() ? "downloaded as " + std::string(item->GetName()) : candidate->error;
					result.members.push_back(entry);
				}
				info("Fleet for %s: %s is downloaded already", request.dupeKey.c_str(), item->GetName());
				return result;
			}
		}
	}

	// a download of the key running: the fleet's members become its backups, scored
	// below everything the key holds (F5: just below the running download, they tied
	// with the backups an earlier fleet left); otherwise above everything the key
	// holds, so the duplicate check queues the best and keeps the others as backups,
	// tried in rank order
	int base = runningId ? lowest : top;

	bool anyAlive = std::any_of(order.begin(), order.end(),
		[&](const Candidate* c) { return tier(*c) <= 1; });
	std::vector<int> added(candidates.size(), 0);	// member index -> its nzb id
	int rank = 0;
	for (Candidate* candidate : ranked)
	{
		Member& member = request.members[candidate->index];
		Entry entry;
		entry.member = (int)candidate->index;
		entry.name = member.name;
		entry.rank = ++rank;
		entry.alive = candidate->alive;
		if (!candidate->error.empty())
		{
			entry.status = "ERROR";
			entry.reason = candidate->error;
		}
		else if (candidate->sameAs >= 0 || candidate->existingId)
		{
			// its posting is in the fleet or in nzbget already: one copy of it is enough
			entry.status = "SAME_POSTING";
			entry.sameAs = candidate->existingId ? candidate->existingId : added[candidate->sameAs];
			// the twin's rank, also when it wasn't added (dead: F8)
			for (const Entry& other : result.members)
			{
				if (!candidate->existingId && other.member == candidate->sameAs)
				{
					entry.sameAsRank = other.rank;
				}
			}
		}
		else if (!anyAlive)
		{
			entry.status = "DEAD";
		}
		else
		{
			NzbParameterList parameters;
			if (candidate->alive >= 0)
			{
				parameters.SetParameter(DupeSearch::AliveParam, candidate->dead ? "0" :
					std::to_string(candidate->alive).c_str());
			}
			int nzbId = 0;
			std::string name = member.name;
			if (name.size() < 4 || strcasecmp(name.c_str() + name.size() - 4, ".nzb"))
			{
				name += ".nzb";
			}
			Scanner::EAddStatus status = g_Scanner->AddExternalFile(name.c_str(), request.category.c_str(),
				false, request.priority, request.dupeKey.c_str(), base - rank, dmScore, &parameters,
				false, false, nullptr, nullptr, member.data.data(), (int)member.data.size(), &nzbId);
			entry.nzbId = status == Scanner::asSuccess ? nzbId : 0;
			added[candidate->index] = entry.nzbId;
			entry.status = entry.nzbId == 0 ? "ERROR" : candidate->dead ? "DEAD" : "BACKUP";
			if (entry.nzbId == 0)
			{
				entry.reason = "nzbget could not add the nzb-file";
			}
		}
		result.members.push_back(entry);
	}

	// which one downloads: the download already running, or the one the duplicate
	// check queued (the best, unless the release is on disk already)
	bool onDisk = false;
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		if (runningId && downloadQueue->GetQueue()->Find(runningId))
		{
			result.chosen = runningId;
			result.reason = "ALREADY_QUEUED";
		}
		for (Entry& entry : result.members)
		{
			if (!runningId && !result.chosen && entry.nzbId > 0 && downloadQueue->GetQueue()->Find(entry.nzbId))
			{
				entry.status = "QUEUED";
				result.chosen = entry.nzbId;
			}
		}
		// the duplicate check filed a member as a copy of another item (the same
		// nzb-file under another key): reported so, not as a backup
		for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
		{
			if (historyInfo->GetKind() != HistoryInfo::hkNzb ||
				historyInfo->GetNzbInfo()->GetDeleteStatus() != NzbInfo::dsCopy)
			{
				continue;
			}
			for (Entry& entry : result.members)
			{
				if (entry.nzbId == historyInfo->GetNzbInfo()->GetId())
				{
					entry.status = "COPY";
					entry.reason = "the same nzb-file as an item nzbget has already";
				}
			}
		}
		NzbInfo probe;
		probe.SetDupeKey(request.dupeKey.c_str());
		onDisk = DupeCoordinator::DownloadedOnDisk(downloadQueue, &probe) != nullptr;
	}
	if (result.chosen == 0 && result.reason.empty())
	{
		bool anyUsable = std::any_of(candidates.begin(), candidates.end(),
			[](const Candidate& c) { return c.error.empty(); });
		// INCOMPLETE only when a member wasn't measured at all (F6); one measured
		// below the floor is dead, though the check of the rest ran to the limit (F10)
		bool anyUnknown = std::any_of(order.begin(), order.end(),
			[&](const Candidate* c) { return c->error.empty() && c->alive < 0; });
		result.reason = !anyUsable ? "NO_USABLE_MEMBERS" : !anyAlive ? (anyUnknown ? "INCOMPLETE" : "ALL_DEAD") :
			onDisk ? "ALREADY_DOWNLOADED" : "NOT_QUEUED";
	}
	info("Fleet of %i nzb-file(s) for %s: %s%s", (int)request.members.size(), request.dupeKey.c_str(),
		result.chosen ? ("queued #" + std::to_string(result.chosen)).c_str() : result.reason.c_str(),
		result.complete ? "" : " (not every posting checked in time)");
	return result;
}
