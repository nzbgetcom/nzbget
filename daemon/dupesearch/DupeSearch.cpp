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

#include <chrono>
#include <fstream>
#include <sstream>
#include "DupeSearch.h"
#include "ReleaseName.h"
#include "FileSystem.h"
#include "Log.h"
#include "Options.h"
#include "Util.h"

DupeSearch* g_DupeSearch = nullptr;

namespace
{

std::string LowerKey(const std::string& key)
{
	std::string out = key;
	for (char& ch : out)
	{
		ch = (char)tolower((unsigned char)ch);
		if (ch == '\t' || ch == '\n' || ch == '\r')
		{
			ch = ' ';
		}
	}
	return out;
}

// the key a download is searched under: its own, or one made from its name
std::string EffectiveKey(NzbInfo* nzbInfo)
{
	if (!Util::EmptyStr(nzbInfo->GetDupeKey()))
	{
		return nzbInfo->GetDupeKey();
	}
	return DupeSearch::MakeDupeKey(nzbInfo->GetName());
}

}

std::string DupeSearch::MakeDupeKey(const std::string& name)
{
	return "dupes:" + ReleaseName::Normalize(name);
}

bool DupeSearch::IsDonor(NzbInfo* nzbInfo)
{
	return nzbInfo->GetParameters()->Find(DonorParam) || nzbInfo->GetParameters()->Find(AliveParam);
}

DupeSearch::DupeSearch()
{
	m_observer.m_owner = this;
	DownloadQueue::Guard()->Attach(&m_observer);
}

DupeSearch::~DupeSearch()
{
	debug("Destroying DupeSearch");
}

void DupeSearch::Stop()
{
	Thread::Stop();
	std::lock_guard<std::mutex> guard(m_mutex);
	m_cond.notify_all();
}

void DupeSearch::DownloadQueueUpdate(void* aspect)
{
	DownloadQueue::Aspect* queueAspect = (DownloadQueue::Aspect*)aspect;
	if (queueAspect->action == DownloadQueue::eaNzbAdded && queueAspect->nzbInfo &&
		g_Options->GetDupeSearch() && !IsStopped())
	{
		Schedule(queueAspect->nzbInfo->GetId(), Util::CurrentTime() + g_Options->GetDupeSearchDelay());
	}
}

void DupeSearch::Schedule(int nzbId, time_t due)
{
	std::lock_guard<std::mutex> guard(m_mutex);
	m_pending[nzbId] = due;
	m_cond.notify_all();
}

// an event isn't fired for a download that was queued before a restart
void DupeSearch::ScanQueue()
{
	std::vector<int> ids;
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			if (nzbInfo->GetKind() == NzbInfo::nkNzb)
			{
				ids.push_back(nzbInfo->GetId());
			}
		}
	}
	for (int id : ids)
	{
		Schedule(id, Util::CurrentTime() + g_Options->GetDupeSearchDelay());
	}
}

void DupeSearch::Run()
{
	debug("Entering DupeSearch-loop");

	while (!DownloadQueue::IsLoaded() && !IsStopped())
	{
		Util::Sleep(20);
	}
	if (IsStopped())
	{
		return;
	}

	LoadState();
	if (g_Options->GetDupeSearch())
	{
		ScanQueue();
	}

	while (!IsStopped())
	{
		std::vector<int> due;
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_cond.wait_for(lock, std::chrono::milliseconds(500));
			time_t now = Util::CurrentTime();
			for (auto it = m_pending.begin(); it != m_pending.end();)
			{
				if (it->second <= now)
				{
					due.push_back(it->first);
					it = m_pending.erase(it);
				}
				else
				{
					++it;
				}
			}
		}

		for (int nzbId : due)
		{
			if (IsStopped())
			{
				break;
			}
			Job job;
			bool search;
			{
				GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
				search = Prepare(downloadQueue, nzbId, job);
			}
			if (search)
			{
				Search(job);
			}
		}
	}

	debug("Exiting DupeSearch-loop");
}

/*
 * Decides whether a download is searched now. Runs under the queue lock.
 */
bool DupeSearch::Prepare(DownloadQueue* downloadQueue, int nzbId, Job& job)
{
	if (!g_Options->GetDupeSearch() || !g_Options->GetDupeCheck())
	{
		return false;
	}

	// gone from the queue: a backup that the duplicate check filed in history,
	// or a download that was deleted
	NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(nzbId);
	if (!nzbInfo || nzbInfo->GetKind() != NzbInfo::nkNzb)
	{
		return false;
	}

	// only before post-processing
	if (nzbInfo->GetDeleting() || nzbInfo->GetDeleteStatus() != NzbInfo::dsNone || nzbInfo->GetPostInfo())
	{
		return false;
	}

	// the score decides which of several duplicates is the main one; a
	// download that bypasses duplicate handling can't be managed
	if (nzbInfo->GetDupeMode() != dmScore)
	{
		return false;
	}

	// a duplicate this search (or the proxy) queued; a promotion from history
	// after a failover: its key was searched for the pick
	if (IsDonor(nzbInfo) || nzbInfo->GetDupeHint() == NzbInfo::dhRedownloadAuto)
	{
		return false;
	}

	std::string key = EffectiveKey(nzbInfo);
	std::string lowerKey = LowerKey(key);

	// the top-scored item of its key only (a submitter's backups, scored a
	// little lower, are filed in history shortly after they are added)
	for (NzbInfo* other : downloadQueue->GetQueue())
	{
		if (other != nzbInfo && other->GetKind() == NzbInfo::nkNzb && !other->GetDeleting() &&
			other->GetDeleteStatus() == NzbInfo::dsNone && LowerKey(EffectiveKey(other)) == lowerKey &&
			other->GetDupeScore() > nzbInfo->GetDupeScore())
		{
			return false;
		}
	}

	int pickScore = std::max(nzbInfo->GetDupeScore(), BasePickScore);
	time_t now = Util::CurrentTime();
	{
		std::lock_guard<std::mutex> guard(m_mutex);
		auto it = m_searched.find(lowerKey);
		if (it != m_searched.end() && now - it->second.time < SearchWindowSec && it->second.score >= pickScore)
		{
			return false;
		}
		m_searched[lowerKey] = { pickScore, now };
	}
	SaveState();

	// a managed pick: it has a key and a score that leave room below it for
	// duplicates (they score pick - 1000 + 2..90)
	bool changed = false;
	if (Util::EmptyStr(nzbInfo->GetDupeKey()))
	{
		nzbInfo->SetDupeKey(key.c_str());
		changed = true;
	}
	if (nzbInfo->GetDupeScore() < BasePickScore)
	{
		nzbInfo->SetDupeScore(BasePickScore);
		changed = true;
	}
	if (changed)
	{
		downloadQueue->SaveChanged();
	}

	job.nzbId = nzbId;
	job.name = nzbInfo->GetName();
	job.dupeKey = key;
	job.category = nzbInfo->GetCategory();
	job.score = pickScore;
	nzbInfo->PrintMessage(Message::mkInfo, "DupeSearch: searching duplicates of %s (key %s)",
		nzbInfo->GetName(), key.c_str());
	return true;
}

void DupeSearch::Search(const Job& job)
{
	debug("DupeSearch: search of %s", job.name.c_str());
}

std::string DupeSearch::StatePath()
{
	return std::string(g_Options->GetQueueDir()) + PATH_SEPARATOR + "dupesearch";
}

void DupeSearch::LoadState()
{
	std::ifstream file(StatePath());
	std::string line;
	time_t now = Util::CurrentTime();
	std::lock_guard<std::mutex> guard(m_mutex);
	while (std::getline(file, line))
	{
		std::stringstream stream(line);
		std::string key, score, time;
		if (std::getline(stream, key, '\t') && std::getline(stream, score, '\t') && std::getline(stream, time, '\t'))
		{
			Searched searched;
			searched.score = atoi(score.c_str());
			searched.time = (time_t)atoll(time.c_str());
			if (now - searched.time < StateTtlSec)
			{
				m_searched[key] = searched;
			}
		}
	}
}

void DupeSearch::SaveState()
{
	std::string path = StatePath();
	std::string temp = path + ".new";
	{
		std::lock_guard<std::mutex> guard(m_mutex);
		std::ofstream file(temp, std::ios::trunc);
		time_t now = Util::CurrentTime();
		for (const auto& entry : m_searched)
		{
			if (now - entry.second.time < StateTtlSec)
			{
				file << entry.first << '\t' << entry.second.score << '\t' << (long long)entry.second.time << "\t\n";
			}
		}
		if (!file.good())
		{
			warn("Could not save the DupeSearch state to %s", temp.c_str());
			return;
		}
	}
	FileSystem::MoveFile(temp.c_str(), path.c_str());
}
