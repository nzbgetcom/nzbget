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
#include "DupeProbe.h"
#include "DownloadInfo.h"
#include "QueueCoordinator.h"
#include "ServerPool.h"
#include "NewsServer.h"
#include "NntpConnection.h"
#include "Log.h"
#include "Util.h"

namespace
{
	// how long a server gets to hand out a connection (downloads keep them busy)
	constexpr int ConnectionWaitSec = 10;
	// the whole probe
	constexpr int ProbeLimitSec = 90;

	// registry of running probes: shutdown cancels them and waits
	Mutex g_probeMutex;
	std::set<DupeProbe*> g_probes;
	std::atomic<int> g_probeCount{0};
	// set by StopAll: no probe starts after it (it would outlive the server pool)
	bool g_probesStopping = false;
}

std::vector<size_t> DupeProbe::SampleIndexes(size_t total, int count)
{
	std::vector<size_t> indexes;
	if (total == 0 || count <= 0)
	{
		return indexes;
	}

	size_t wanted = std::min(total, (size_t)count);
	for (size_t i = 0; i < wanted; i++)
	{
		indexes.push_back(((2 * i + 1) * total) / (2 * wanted));
	}
	return indexes;
}

DupeProbe::EAnswer DupeProbe::Classify(const char* response)
{
	if (!response)
	{
		return daUnknown;
	}
	if (!strncmp(response, "2", 1))
	{
		return daExists;
	}
	// 400 and 499 are connection problems; the other 41x-43x replies say
	// there is no such article, as does 451
	if (!strncmp(response, "400", 3) || !strncmp(response, "499", 3))
	{
		return daUnknown;
	}
	if (!strncmp(response, "41", 2) || !strncmp(response, "42", 2) || !strncmp(response, "43", 2) ||
		!strncmp(response, "451", 3))
	{
		return daMissing;
	}
	return daUnknown;
}

bool DupeProbe::IsDead(int existing, int definitiveServers, int activeServers)
{
	return existing < MinAliveSamples && definitiveServers > 0 &&
		definitiveServers >= std::min(MinMissingServers, activeServers);
}

void DupeProbe::Start(int nzbId, std::vector<Sample> samples, int recoveredAtStart)
{
	DupeProbe* probe = new DupeProbe(nzbId, std::move(samples));
	probe->m_recoveredAtStart = recoveredAtStart;
	if (!probe->Register())
	{
		delete probe;
		return;
	}
	probe->SetAutoDestroy(true);
	probe->Thread::Start();
}

DupeProbe::Verdict DupeProbe::Check(std::vector<Sample> samples, int limitSec)
{
	DupeProbe probe(0, std::move(samples));
	if (!probe.Register())
	{
		Verdict stopping;
		stopping.Finished = false;
		return stopping;
	}
	Verdict verdict = probe.Measure(limitSec);
	probe.Unregister();
	return verdict;
}

void DupeProbe::Reset()
{
	Guard guard(g_probeMutex);
	g_probesStopping = false;
}

bool DupeProbe::Probing(int nzbId)
{
	Guard guard(g_probeMutex);
	return std::any_of(g_probes.begin(), g_probes.end(),
		[nzbId](DupeProbe* probe) { return probe->m_nzbId == nzbId && !probe->m_countAll; });
}

bool DupeProbe::Register()
{
	Guard guard(g_probeMutex);
	if (g_probesStopping)
	{
		return false;
	}
	g_probes.insert(this);
	g_probeCount++;
	return true;
}

void DupeProbe::Unregister()
{
	Guard guard(g_probeMutex);
	g_probes.erase(this);
	g_probeCount--;
}

std::vector<DupeProbe::Sample> DupeProbe::SamplesOf(const std::vector<FileInfo*>& files,
	const std::function<void(FileInfo*)>& loadArticles)
{
	size_t total = 0;
	for (FileInfo* fileInfo : files)
	{
		total += fileInfo->GetTotalArticles();
	}

	std::vector<Sample> samples;
	for (size_t position : SampleIndexes(total, SampleCount))
	{
		size_t offset = 0;
		for (FileInfo* fileInfo : files)
		{
			size_t count = fileInfo->GetTotalArticles();
			if (position >= offset + count)
			{
				offset += count;
				continue;
			}

			if (fileInfo->GetArticles()->empty() && loadArticles)
			{
				loadArticles(fileInfo);
			}
			size_t index = position - offset;
			if (index < fileInfo->GetArticles()->size())
			{
				Sample sample;
				sample.MessageId = fileInfo->GetArticles()->at(index)->GetMessageId();
				sample.Groups = std::make_shared<std::vector<CString>>();
				for (const CString& group : *fileInfo->GetGroups())
				{
					sample.Groups->emplace_back(*group);
				}
				samples.push_back(std::move(sample));
			}
			break;
		}
	}
	return samples;
}

void DupeProbe::StopAll()
{
	Guard guard(g_probeMutex);
	g_probesStopping = true;
	for (DupeProbe* probe : g_probes)
	{
		probe->Thread::Stop();
		probe->Cancel();
	}
}

void DupeProbe::WaitAll()
{
	// without a time limit, like the download threads (QueueCoordinator::WaitJobs):
	// a probe that outlived the wait would use the server pool after it was freed.
	// StopAll cancelled their connections, so they end quickly
	while (g_probeCount > 0)
	{
		Util::Sleep(20);
	}
}

// called with g_probeMutex held
void DupeProbe::Cancel()
{
	if (m_connection)
	{
		m_connection->SetSuppressErrors(true);
		m_connection->Cancel();
	}
}

bool DupeProbe::ProbeServer(int serverId, std::set<int>& probed, ServerResult& result)
{
	NewsServer* wanted = g_ServerPool->GetServerById(serverId);
	if (!wanted)
	{
		return false;
	}

	NntpConnection* connection = nullptr;
	time_t waitStart = Util::CurrentTime();
	while (!connection && !IsStopped() && Util::CurrentTime() - waitStart <= ConnectionWaitSec)
	{
		connection = g_ServerPool->GetConnection(wanted->GetNormLevel(), wanted, nullptr);
		if (!connection)
		{
			Util::Sleep(50);
		}
	}
	if (!connection)
	{
		return false;
	}

	// a connection of the same server group may have been handed out instead
	NewsServer* server = connection->GetNewsServer();
	bool fresh = probed.insert(server->GetId()).second;
	bool probedServer = false;
	if (fresh)
	{
		{
			Guard guard(g_probeMutex);
			m_connection = connection;
		}

		int remaining = (int)m_samples.size();
		for (size_t index = 0; index < m_samples.size(); index++)
		{
			const Sample& sample = m_samples[index];
			if (IsStopped())
			{
				break;
			}
			remaining--;
			if (m_found[index])
			{
				continue;	// another server has it
			}

			if (!connection->Connect())
			{
				result.Unknown += remaining + 1;
				break;
			}

			if (server->GetJoinGroup())
			{
				const char* joined = nullptr;
				for (const CString& group : *sample.Groups)
				{
					joined = connection->JoinGroup(group);
					if (joined && !strncmp(joined, "2", 1))
					{
						break;
					}
				}
				if (!joined || strncmp(joined, "2", 1))
				{
					result.Unknown += remaining + 1;
					if (!joined)
					{
						connection->Disconnect();
					}
					break;
				}
			}

			const char* response = connection->Request(BString<1024>("STAT %s\r\n", *sample.MessageId));
			if (!response)
			{
				connection->Disconnect();
				result.Unknown += remaining + 1;
				break;
			}

			switch (Classify(response))
			{
				case daExists:
					result.Exists++;
					m_found[index] = 1;
					if (m_foundOn.empty())
					{
						m_foundOn = std::string(server->GetName()) + " (" + *sample.MessageId + ")";
					}
					break;
				case daMissing:
					result.Missing++;
					break;
				case daUnknown:
					result.Unknown++;
					break;
			}

			if (!m_countAll && std::count(m_found.begin(), m_found.end(), 1) >= MinAliveSamples)
			{
				break;	// alive: no need to ask further
			}
		}
		probedServer = true;
	}

	{
		Guard guard(g_probeMutex);
		m_connection = nullptr;
	}
	if (IsStopped() || connection->GetStatus() == Connection::csCancelled)
	{
		connection->Disconnect();
	}
	g_ServerPool->FreeConnection(connection, true);
	return probedServer;
}

DupeProbe::Verdict DupeProbe::Measure(int limitSec)
{
	// the servers to ask: every active one, one per server group
	std::vector<std::pair<int, int>> order;	// level, id
	std::set<int> groups;
	Verdict verdict;
	for (NewsServer* server : g_ServerPool->GetServers())
	{
		if (!server->GetActive())
		{
			continue;
		}
		if (server->GetGroup() > 0 && !groups.insert(server->GetGroup()).second)
		{
			continue;
		}
		verdict.ActiveServers++;
		order.emplace_back(server->GetNormLevel(), server->GetId());
	}
	std::sort(order.begin(), order.end());

	std::set<int> probed;
	time_t start = Util::CurrentTime();

	for (const std::pair<int, int>& entry : order)
	{
		if (IsStopped() || Util::CurrentTime() - start > limitSec)
		{
			verdict.Finished = false;
			break;
		}

		ServerResult result;
		if (!ProbeServer(entry.second, probed, result))
		{
			// not reached (no connection, not asked): no evidence either way
			continue;
		}
		verdict.ReachedServers++;

		verdict.Existing = (int)std::count(m_found.begin(), m_found.end(), 1);
		verdict.FoundOn = m_foundOn;
		if (m_countAll)
		{
			// every sample is asked of every server until each was found somewhere
			if (verdict.Existing == (int)m_samples.size())
			{
				break;
			}
			continue;
		}
		if (verdict.Existing >= MinAliveSamples)
		{
			break;
		}
		if (result.Unknown == 0 && result.Exists + result.Missing > 0)
		{
			verdict.MissingServers++;
		}
	}
	if (IsStopped())
	{
		// stopped while asking the last server: its answers are incomplete
		verdict.Finished = false;
	}
	return verdict;
}

void DupeProbe::StartRecheck(int nzbId, std::vector<Sample> samples)
{
	DupeProbe* probe = new DupeProbe(nzbId, std::move(samples));
	probe->m_countAll = true;
	probe->m_found.assign(probe->m_samples.size(), 0);
	if (!probe->Register())
	{
		delete probe;
		return;
	}
	probe->SetAutoDestroy(true);
	probe->Thread::Start();
}

void DupeProbe::Recheck()
{
	Verdict verdict = Measure(RecheckLimitSec);
	if (IsStopped() || !verdict.Finished)
	{
		return;
	}

	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
	for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
	{
		if (historyInfo->GetKind() != HistoryInfo::hkNzb || historyInfo->GetNzbInfo()->GetId() != m_nzbId)
		{
			continue;
		}
		NzbInfo* nzbInfo = historyInfo->GetNzbInfo();
		int sampled = (int)m_samples.size();
		bool retry = verdict.Existing * 2 >= sampled;
		nzbInfo->PrintMessage(Message::mkInfo, "%i of %i failed articles exist on the servers%s",
			verdict.Existing, sampled, retry ? "; retrying them" : "; not retrying");
		if (retry)
		{
			IdList ids = { historyInfo->GetId() };
			downloadQueue->EditList(&ids, nullptr, DownloadQueue::mmId, DownloadQueue::eaHistoryRetryFailed, nullptr);
		}
		break;
	}
}

void DupeProbe::Run()
{
	if (m_countAll)
	{
		Recheck();
		Unregister();
		return;
	}

	Verdict verdict = Measure(ProbeLimitSec);
	if (IsStopped())
	{
		// shutting down: never park a download now
		Unregister();
		return;
	}

	int sampled = (int)m_samples.size();
	if (verdict.Dead())
	{
		Note(Message::mkInfo, "Dupe probe: %i of %i sampled articles exist (%i of %i servers answered "
			"definitively)%s%s: the posting is dead", verdict.Existing, sampled, verdict.MissingServers,
			verdict.ActiveServers, verdict.FoundOn.empty() ? "" : ", found on ", verdict.FoundOn.c_str());
		Abandon(verdict.MissingServers, verdict.ActiveServers);
	}
	else if (verdict.Existing >= MinAliveSamples)
	{
		// the probe stops asking once MinAliveSamples were found: the rest weren't asked
		Note(Message::mkInfo, "Dupe probe: %i of %i sampled articles found (it stops at %i), the first on %s: "
			"not abandoning it", verdict.Existing, sampled, MinAliveSamples, verdict.FoundOn.c_str());
	}
	else if (verdict.Finished && verdict.ReachedServers == 0)
	{
		// no server could be asked (every connection busy): no verdict at all, so
		// the download may be probed again when its next article starts
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(m_nzbId);
		if (nzbInfo)
		{
			nzbInfo->SetDeadPickProbed(false);
			// (under the queue lock already: not through Note, the lock isn't recursive)
			nzbInfo->PrintMessage(Message::mkDetail, "Dupe probe: no server could be asked, will probe again");
		}
	}
	else if (verdict.Finished)
	{
		Note(Message::mkInfo, "Dupe probe: no verdict (%i of %i sampled articles exist, %i of %i servers "
			"answered definitively)", verdict.Existing, sampled, verdict.MissingServers, verdict.ActiveServers);
	}

	Unregister();
}

void DupeProbe::Note(Message::EKind kind, const char* format, ...)
{
	char text[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(text, sizeof(text), format, args);
	va_end(args);

	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
	NzbInfo* nzbInfo = downloadQueue->GetQueue()->Find(m_nzbId);
	if (nzbInfo)
	{
		nzbInfo->PrintMessage(kind, "%s", text);
	}
	else
	{
		detail("Dupe probe of download %i: %s", m_nzbId, text);
	}
}

void DupeProbe::Abandon(int missingServers, int activeServers)
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
	g_QueueCoordinator->FailOverDeadPick(downloadQueue, m_nzbId, (int)m_samples.size(),
		missingServers, activeServers, m_recoveredAtStart);
}
