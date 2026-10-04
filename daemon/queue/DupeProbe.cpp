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

bool DupeProbe::IsDead(int existing, int missingServers, int activeServers)
{
	return existing == 0 && missingServers > 0 &&
		missingServers >= std::min(MinMissingServers, activeServers);
}

void DupeProbe::Start(int nzbId, std::vector<Sample> samples)
{
	DupeProbe* probe = new DupeProbe(nzbId, std::move(samples));
	probe->SetAutoDestroy(true);
	{
		Guard guard(g_probeMutex);
		g_probes.insert(probe);
		g_probeCount++;
	}
	probe->Thread::Start();
}

void DupeProbe::StopAll()
{
	Guard guard(g_probeMutex);
	for (DupeProbe* probe : g_probes)
	{
		probe->Thread::Stop();
		probe->Cancel();
	}
}

void DupeProbe::WaitAll()
{
	for (int waited = 0; g_probeCount > 0 && waited < 500; waited++)
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
		for (const Sample& sample : m_samples)
		{
			if (IsStopped())
			{
				break;
			}
			remaining--;

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
					break;
				case daMissing:
					result.Missing++;
					break;
				case daUnknown:
					result.Unknown++;
					break;
			}

			if (result.Exists > 0)
			{
				break;
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

void DupeProbe::Run()
{
	// the servers to ask: every active one, one per server group
	std::vector<std::pair<int, int>> order;	// level, id
	std::set<int> groups;
	int activeServers = 0;
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
		activeServers++;
		order.emplace_back(server->GetNormLevel(), server->GetId());
	}
	std::sort(order.begin(), order.end());

	int existing = 0;
	int missingServers = 0;
	std::set<int> probed;
	time_t start = Util::CurrentTime();
	bool finished = true;

	for (const std::pair<int, int>& entry : order)
	{
		if (IsStopped() || Util::CurrentTime() - start > ProbeLimitSec)
		{
			finished = false;
			break;
		}

		ServerResult result;
		if (!ProbeServer(entry.second, probed, result))
		{
			// not reached (no connection, not asked): no evidence either way
			continue;
		}

		existing += result.Exists;
		if (existing > 0)
		{
			break;
		}
		if (result.Missing == (int)m_samples.size())
		{
			missingServers++;
		}
	}

	if (finished && IsDead(existing, missingServers, activeServers))
	{
		Abandon(missingServers, activeServers);
	}
	else if (existing > 0)
	{
		detail("Dupe probe of download %i: an article exists, not abandoning it", m_nzbId);
	}
	else if (finished)
	{
		detail("Dupe probe of download %i: no verdict (%i of %i servers answered definitively)",
			m_nzbId, missingServers, activeServers);
	}

	Guard guard(g_probeMutex);
	g_probes.erase(this);
	g_probeCount--;
}

void DupeProbe::Abandon(int missingServers, int activeServers)
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
	g_QueueCoordinator->FailOverDeadPick(downloadQueue, m_nzbId, (int)m_samples.size(),
		missingServers, activeServers);
}
