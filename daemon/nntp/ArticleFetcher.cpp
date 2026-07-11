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
#include "ArticleFetcher.h"
#include "NntpConnection.h"
#include "NewsServer.h"
#include "ServerPool.h"
#include "Decoder.h"
#include "Log.h"
#include "Options.h"
#include "StatMeter.h"
#include "Util.h"
#include "WorkState.h"

bool ArticleFetchLimits::AddRawBytes(int64 bytes)
{
	if (bytes < 0 || bytes > MaxRawBytes - m_rawBytes)
	{
		return false;
	}
	m_rawBytes += bytes;
	return true;
}

bool ArticleFetchLimits::HeaderWithinLimit(bool declaredRangeKnown) const
{
	return declaredRangeKnown || m_rawBytes <= MaxHeaderBytes;
}

bool ArticleFetchLimits::AddDecodedBytes(int64 bytes, int64 begin, int64 end,
	int64 fileSize)
{
	if (bytes < 0 || begin <= 0 || end < begin || fileSize <= 0 || end > fileSize)
	{
		return false;
	}

	int64 declaredBytes = end - begin + 1;
	if (declaredBytes > MaxDecodedBytes || bytes > declaredBytes - m_decodedBytes)
	{
		return false;
	}
	m_decodedBytes += bytes;
	return true;
}

ArticleFetcher::FetchedArticle ArticleFetcher::Fetch(const char* messageId,
	const std::vector<CString>& groups)
{
	FetchedArticle result;
	ServerPool::RawServerList failedServers;

	int level = 0;
	while (level <= g_ServerPool->GetMaxNormLevel() && !m_stopped)
	{
		while (g_WorkState->GetQuotaReached() && !m_stopped)
		{
			Util::Sleep(100);
		}
		if (m_stopped)
		{
			break;
		}
		// count servers still worth trying on this level; without this check
		// a fully failed level would spin the acquire loop until its timeout
		int eligible = 0;
		for (NewsServer* server : g_ServerPool->GetServers())
		{
			if (server->GetActive() && server->GetNormLevel() == level &&
				std::find(failedServers.begin(), failedServers.end(), server) == failedServers.end())
			{
				eligible++;
			}
		}
		if (eligible == 0)
		{
			level++;
			continue;
		}

		// the pool is non-blocking: poll until a connection frees up, bounded
		// so a saturated pool cannot stall the repair stage indefinitely
		NntpConnection* connection = nullptr;
		time_t waitStart = Util::CurrentTime();
		while (!connection && !m_stopped &&
			Util::CurrentTime() - waitStart <= g_Options->GetArticleTimeout())
		{
			if (g_WorkState->GetQuotaReached())
			{
				Util::Sleep(100);
				waitStart = Util::CurrentTime();
				continue;
			}
			connection = g_ServerPool->GetConnection(level, nullptr, &failedServers);
			if (!connection)
			{
				Util::Sleep(5);
			}
		}

		if (!connection)
		{
			level++;
			continue;
		}

		bool stopped = false;
		{
			Guard guard(m_connectionMutex);
			m_connection = connection;
			stopped = m_stopped;
		}
		if (stopped)
		{
			ReleaseConnection(connection, true);
			break;
		}

		result = FetchFromConnection(connection, messageId, groups);

		NewsServer* server = connection->GetNewsServer();
		ReleaseConnection(connection, true);

		if (result.Success)
		{
			return result;
		}
		if (result.Retry)
		{
			while (g_WorkState->GetQuotaReached() && !m_stopped)
			{
				Util::Sleep(100);
			}
			continue;
		}

		// this server could not supply the article; try the remaining servers
		// of this level, then the next level
		failedServers.push_back(server);
	}

	return result;
}

void ArticleFetcher::Stop()
{
	m_stopped = true;
	Guard guard(m_connectionMutex);
	if (m_connection)
	{
		m_connection->SetSuppressErrors(true);
		m_connection->Cancel();
	}
}

void ArticleFetcher::ReleaseConnection(NntpConnection* connection, bool keepConnected)
{
	Guard guard(m_connectionMutex);
	AddServerStats(connection);
	if (!keepConnected || connection->GetStatus() == Connection::csCancelled)
	{
		connection->Disconnect();
	}
	if (m_connection == connection)
	{
		m_connection = nullptr;
	}
	g_ServerPool->FreeConnection(connection, true);
}

void ArticleFetcher::AddServerStats(NntpConnection* connection)
{
	int bytesRead = connection->FetchTotalBytesRead();
	if (bytesRead <= 0)
	{
		return;
	}

	ServerVolume::Stats stats;
	stats.bytes = Util::SafeIntCast<int, uint32>(bytesRead);
	stats.articles.failed = 0;
	stats.articles.success = 0;
	g_StatMeter->AddServerStats(stats, connection->GetNewsServer()->GetId());
}

ArticleFetcher::FetchedArticle ArticleFetcher::FetchFromConnection(NntpConnection* connection,
	const char* messageId, const std::vector<CString>& groups)
{
	FetchedArticle result;
	if (m_stopped)
	{
		return result;
	}

	if (!connection->Connect())
	{
		detail("Stream repair: could not connect to %s", connection->GetNewsServer()->GetName());
		return result;
	}

	if (connection->GetNewsServer()->GetJoinGroup())
	{
		const char* response = nullptr;
		for (const CString& group : groups)
		{
			response = connection->JoinGroup(group);
			if (response && !strncmp(response, "2", 1))
			{
				break;
			}
		}
		if (!response || strncmp(response, "2", 1))
		{
			detail("Stream repair: could not join groups on %s for %s",
				connection->GetNewsServer()->GetName(), messageId);
			return result;
		}
	}

	const char* response = connection->Request(BString<1024>("BODY %s\r\n", messageId));
	if (!response || strncmp(response, "2", 1))
	{
		detail("Stream repair: article %s not available on %s",
			messageId, connection->GetNewsServer()->GetName());
		return result;
	}

	Decoder decoder;
	decoder.Clear();
	decoder.SetCrcCheck(g_Options->GetCrcCheck());
	decoder.SetRawMode(false);

	CharBuffer recvBuf(g_Options->GetArticleReadChunkSize());
	ArticleFetchLimits limits;

	while (!m_stopped && !decoder.GetEof())
	{
		if (g_WorkState->GetQuotaReached())
		{
			// A BODY response cannot be parked safely for an arbitrary quota
			// pause. Drop this partial response and retry the same source after
			// quota resumes; do not classify it as a source failure.
			connection->Disconnect();
			result.Retry = true;
			return result;
		}
		while (!m_stopped && !g_WorkState->GetQuotaReached() &&
			g_WorkState->GetSpeedLimit() > 0 &&
			(g_StatMeter->CalcCurrentDownloadSpeed() > g_WorkState->GetSpeedLimit() ||
			g_StatMeter->CalcMomentaryDownloadSpeed() > g_WorkState->GetSpeedLimit()))
		{
			Util::Sleep(10);
		}
		if (g_WorkState->GetQuotaReached())
		{
			connection->Disconnect();
			result.Retry = true;
			return result;
		}
		if (m_stopped)
		{
			break;
		}

		char* buffer;
		int len;
		connection->ReadBuffer(&buffer, &len);
		if (len == 0)
		{
			len = connection->TryRecv(recvBuf, recvBuf.Size());
			buffer = recvBuf;
		}

		if (len <= 0)
		{
			// timeout or connection closed mid-article: the connection state
			// is unusable for further commands
			detail("Stream repair: connection to %s lost while fetching %s",
				connection->GetNewsServer()->GetName(), messageId);
			connection->Disconnect();
			return result;
		}

		g_StatMeter->AddSpeedReading(len);
		if (!limits.AddRawBytes(len))
		{
			detail("Stream repair: article %s from %s exceeded the wire-size limit",
				messageId, connection->GetNewsServer()->GetName());
			connection->Disconnect();
			return result;
		}

		int decodedLen = decoder.DecodeBuffer(buffer, len);
		bool declaredRangeKnown = decoder.GetBeginPos() > 0 &&
			decoder.GetEndPos() >= decoder.GetBeginPos() && decoder.GetSize() > 0;
		if (!limits.HeaderWithinLimit(declaredRangeKnown))
		{
			detail("Stream repair: article %s from %s exceeded the yEnc-header limit",
				messageId, connection->GetNewsServer()->GetName());
			connection->Disconnect();
			return result;
		}
		if (declaredRangeKnown && !limits.AddDecodedBytes(decodedLen,
			decoder.GetBeginPos(), decoder.GetEndPos(), decoder.GetSize()))
		{
			detail("Stream repair: article %s from %s exceeded its declared decoded range",
				messageId, connection->GetNewsServer()->GetName());
			connection->Disconnect();
			return result;
		}
		if (decodedLen > 0)
		{
			result.Data.insert(result.Data.end(), buffer, buffer + decodedLen);
		}
	}

	if (m_stopped)
	{
		// stopped mid-body: the response was not fully drained, so the
		// connection cannot be reused by the next pool consumer
		connection->Disconnect();
		return result;
	}

	if (decoder.Check() != Decoder::dsFinished ||
		decoder.GetFormat() != Decoder::efYenc ||
		decoder.GetBeginPos() <= 0 || decoder.GetEndPos() <= 0 ||
		result.Data.empty())
	{
		detail("Stream repair: article %s from %s failed decoding",
			messageId, connection->GetNewsServer()->GetName());
		return result;
	}

	// yEnc part positions are 1-based inclusive; the decoded byte count must
	// match the declared part range or the article cannot be trusted
	result.Offset = decoder.GetBeginPos() - 1;
	result.FileSize = decoder.GetSize();
	result.Success = (int64)result.Data.size() ==
		decoder.GetEndPos() - decoder.GetBeginPos() + 1;

	return result;
}
