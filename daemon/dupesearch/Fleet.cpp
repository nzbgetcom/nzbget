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
#include <mutex>
#include "Fleet.h"
#include "DonorHealth.h"
#include "DupeCoordinator.h"
#include "DupeSearch.h"
#include "HttpGet.h"
#include "NntpHealthServer.h"
#include "NzbReader.h"
#include "Posting.h"
#include "Scanner.h"
#include "DownloadInfo.h"
#include "Options.h"
#include "Log.h"

namespace
{
	// the largest nzb-file a member may be (as for the duplicate search's fetches)
	constexpr size_t MaxNzbBytes = 64 * 1024 * 1024;
	// postings checked at once (as the duplicate search does)
	constexpr int FleetParallel = 4;

	struct Candidate
	{
		size_t index;			// in the request
		NzbSummary summary;
		DonorHealth::Health health;
		bool checked = false;
		int alive = -1;
		bool dead = false;
		int sameAs = -1;		// index of the member whose posting this one is
		std::string error;
	};
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

	// read every member: an nzb-file sent, or fetched from its url
	std::vector<Candidate> candidates(request.members.size());
	for (size_t i = 0; i < request.members.size(); i++)
	{
		Member& member = request.members[i];
		Candidate& candidate = candidates[i];
		candidate.index = i;
		if (!member.url.empty())
		{
			HttpGet::Reply reply = HttpGet::Fetch(member.url, "fleet member " + member.name, MaxNzbBytes);
			if (!reply.ok)
			{
				candidate.error = "the nzb-file could not be fetched (HTTP " + std::to_string(reply.status) + ")";
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

	// the health of each posting, on the configured servers, within the time limit
	std::vector<DonorHealth::Posting> postings;
	for (Candidate& candidate : candidates)
	{
		if (candidate.error.empty() && candidate.sameAs < 0)
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
		options.sample.maximum = g_Options->GetDupeHealthMax();
		options.sample.maxBody = g_Options->GetDupeBodyChecks();
		options.budgetMs = request.timeoutSec * 1000;
		std::mutex mutex;
		DonorHealth::CheckPostings(servers, postings, options, true, FleetParallel,
			[&](const std::string& key, const DonorHealth::Health& health)
			{
				std::lock_guard<std::mutex> guard(mutex);
				Candidate& candidate = candidates[(size_t)atoi(key.c_str())];
				candidate.health = health;
				candidate.checked = true;
			});
	}
	for (Candidate& candidate : candidates)
	{
		if (!candidate.error.empty() || candidate.sameAs >= 0)
		{
			continue;
		}
		double alive = candidate.checked ? candidate.health.Alive() : -1;
		candidate.alive = alive < 0 ? -1 : (int)std::lround(100 * alive);
		candidate.dead = candidate.checked && (DonorHealth::DeadProbe(candidate.health) ||
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

	// scores above everything the key holds, in rank order, so the duplicate check
	// queues the best and keeps the others as backups, tried in this order
	int top = DupeSearch::BasePickScore;
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		auto consider = [&](NzbInfo* nzbInfo)
			{
				if (!strcasecmp(nzbInfo->GetDupeKey(), request.dupeKey.c_str()))
				{
					top = std::max(top, nzbInfo->GetDupeScore() + 1000);
				}
			};
		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			consider(nzbInfo);
		}
		for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
		{
			if (historyInfo->GetKind() == HistoryInfo::hkNzb)
			{
				consider(historyInfo->GetNzbInfo());
			}
		}
	}

	bool anyAlive = std::any_of(order.begin(), order.end(),
		[&](const Candidate* c) { return tier(*c) <= 1; });
	std::vector<int> added(candidates.size(), 0);	// member index -> its nzb id
	int rank = 0;
	for (Candidate* candidate : ranked)
	{
		Member& member = request.members[candidate->index];
		Entry entry;
		entry.name = member.name;
		entry.rank = ++rank;
		entry.alive = candidate->alive;
		if (!candidate->error.empty())
		{
			entry.status = "ERROR";
			entry.reason = candidate->error;
		}
		else if (candidate->sameAs >= 0)
		{
			// its posting is in the fleet already: one copy of it is enough
			entry.status = "SAME_POSTING";
			entry.sameAs = added[candidate->sameAs];
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
				false, request.priority, request.dupeKey.c_str(), top - rank, dmScore, &parameters,
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

	// which one the duplicate check queued: the best, unless the release is on disk already
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		for (Entry& entry : result.members)
		{
			if (entry.nzbId > 0 && downloadQueue->GetQueue()->Find(entry.nzbId))
			{
				entry.status = "QUEUED";
				result.chosen = entry.nzbId;
				break;
			}
		}
	}
	if (result.chosen == 0)
	{
		result.reason = !anyAlive ? "ALL_DEAD" : "ALREADY_DOWNLOADED";
	}
	info("Fleet of %i nzb-file(s) for %s: %s%s", (int)request.members.size(), request.dupeKey.c_str(),
		result.chosen ? ("queued #" + std::to_string(result.chosen)).c_str() : result.reason.c_str(),
		result.complete ? "" : " (not every posting checked in time)");
	return result;
}
