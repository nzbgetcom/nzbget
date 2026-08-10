/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2024-2026 Denis <denis@nzbget.com>
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


#ifndef SERVERPOOL_H
#define SERVERPOOL_H

#include "Log.h"
#include "Container.h"
#include "Thread.h"
#include "NewsServer.h"
#include "NntpConnection.h"

inline constexpr int CONNECTION_HOLD_SECONDS = 60;

class ServerPool : public Debuggable
{
public:
	typedef std::vector<NewsServer*> RawServerList;

	void SetTimeout(int timeout) { m_timeout = timeout; }
	void SetRetryInterval(int retryInterval) { m_retryInterval = retryInterval; }
	void AddServer(std::unique_ptr<NewsServer> newsServer);
	void InitConnections();
	int GetMaxNormLevel() { return m_maxNormLevel; }
	Servers* GetServers() { return &m_servers; } // Only for read access (no lockings)
	NewsServer* GetServerById(int id);
	NntpConnection* GetConnection(int level, NewsServer* wantServer, RawServerList* ignoreServers);
	void FreeConnection(NntpConnection* connection, bool failed);
	void CloseUnusedConnections();
	void Changed();
	int GetGeneration() { return m_generation; }
	// True if the server has at least one pooled connection that is not currently on
	// cooldown. In-use connections still count (they free up soon); this answers "can
	// this optional server actually serve a connection right now", used to decide
	// whether to escalate to the next level instead of waiting.
	bool ServerHasUsableConnection(NewsServer* newsServer);

protected:
	virtual void LogDebugInfo();

private:
	class PooledConnection : public NntpConnection
	{
	public:
		using NntpConnection::NntpConnection;
		bool GetInUse() { return m_inUse; }
		void SetInUse(bool inUse) { m_inUse = inUse; }
		void SetIndex(int index) { m_index = index; }
		time_t GetFreeTime() { return m_freeTime; }
		void SetFreeTimeNow();
		void SetCooldown(int retryIntervalSec);
		bool IsOnCooldown();
	private:
		bool m_inUse = false;
		time_t m_freeTime = 0;
		time_t m_cooldownUntil = 0;
		int m_consecutiveFailures = 0;
		int m_index = 0;
	};

	typedef std::vector<int> Levels;
	typedef std::vector<std::unique_ptr<PooledConnection>> Connections;

	Servers m_servers;
	RawServerList m_sortedServers;
	Connections m_connections;
	Levels m_levels;
	int m_maxNormLevel = 0;
	Mutex m_connectionsMutex;
	int m_timeout = 60;
	int m_retryInterval = 0;
	int m_generation = 0;

	void NormalizeLevels();
	NntpConnection* LockedGetConnection(int level, NewsServer* wantServer, RawServerList* ignoreServers);
	bool LockedServerHasUsableConnection(NewsServer* newsServer); // assumes m_connectionsMutex held
};

extern ServerPool* g_ServerPool;

#endif
