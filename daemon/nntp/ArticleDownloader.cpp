/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#include "nzbget.h"
#include "ArticleDownloader.h"
#include "ArticleWriter.h"
#include "Decoder.h"
#include "Log.h"
#include "Options.h"
#include "WorkState.h"
#include "ServerPool.h"
#include "StatMeter.h"
#include "Util.h"

ArticleDownloader::ArticleDownloader()
	: m_fileInfo(nullptr)
	, m_articleInfo(nullptr)
	, m_writingStarted(false)
{
	debug("Creating ArticleDownloader");

	SetLastUpdateTimeNow();
}

ArticleDownloader::~ArticleDownloader()
{
	debug("Destroying ArticleDownloader");

#ifndef DISABLE_TLS
	OpenSSL::StopSSLThread();
#endif
}

void ArticleDownloader::SetInfoName(const char* infoName)
{
	m_infoName = infoName;
	m_articleWriter.SetInfoName(m_infoName);
}

/*
 * How server management (for one particular article) works:
	- there is a list of failed servers which is initially empty;
	- level is initially 0;

	<loop>
		- request a connection from server pool for current level;
		  Exception: this step is skipped for the very first download attempt, because a
		  level-0 connection is initially passed from queue manager;
		- try to download from server;
		- if connection to server cannot be established or download fails due to interrupted connection,
		  try again (as many times as needed without limit) the same server until connection is OK;
		- if download fails with error "Not-Found" (article or group not found) or with CRC error,
		  add the server to failed server list;
		- if download fails with general failure error (article incomplete, other unknown error
		  codes), try the same server again as many times as defined by option <ArticleRetries>;
		  if all attempts fail, add the server to failed server list;
		- if all servers from current level were tried, increase level;
		- if all servers from all levels were tried, break the loop with failure status.
	<end-loop>
*/
void ArticleDownloader::Run()
{
	debug("Entering ArticleDownloader-loop");

	SetStatus(adRunning);

	m_articleWriter.SetFileInfo(m_fileInfo);
	m_articleWriter.SetArticleInfo(m_articleInfo);
	m_articleWriter.Prepare();

	EStatus status = adFailed;
	int retries = g_Options->GetArticleRetries() > 0 ? g_Options->GetArticleRetries() : 1;
	int remainedRetries = retries;
	ServerPool::RawServerList failedServers;
	failedServers.reserve(g_ServerPool->GetServers()->size());
	NewsServer* wantServer = nullptr;
	NewsServer* lastServer = nullptr;
	int level = 0;
	int serverConfigGeneration = g_ServerPool->GetGeneration();
	bool force = m_fileInfo->GetNzbInfo()->GetForcePriority();

	while (!IsStopped())
	{
		status = adFailed;

		SetStatus(adWaiting);
		while (!m_connection && !(IsStopped() || serverConfigGeneration != g_ServerPool->GetGeneration()))
		{
			m_connection = g_ServerPool->GetConnection(level, wantServer, &failedServers);
			Util::Sleep(5);
		}
		SetLastUpdateTimeNow();
		SetStatus(adRunning);

		if (IsStopped() || ((g_WorkState->GetPauseDownload() || g_WorkState->GetQuotaReached()) && !force) ||
			(g_WorkState->GetDownloadHeld() && !m_fileInfo->GetExtraPriority()) ||
			serverConfigGeneration != g_ServerPool->GetGeneration())
		{
			status = adRetry;
			break;
		}

		lastServer = m_connection->GetNewsServer();
		level = lastServer->GetNormLevel();

		m_connection->SetSuppressErrors(false);

#ifndef DISABLE_TLS
		m_connection->SetCertVerifLevel(lastServer->GetCertVerificationLevel());
#endif

		m_connectionName.Format("%s (%s)",
			m_connection->GetNewsServer()->GetName(), m_connection->GetHost());

		// check server retention
		bool retentionFailure = m_connection->GetNewsServer()->GetRetention() > 0 &&
			(Util::CurrentTime() - m_fileInfo->GetTime()) / 86400 > m_connection->GetNewsServer()->GetRetention();
		if (retentionFailure)
		{
			detail("Article %s @ %s failed: out of server retention (file age: %i, configured retention: %i)",
				*m_infoName, *m_connectionName,
				(int)(Util::CurrentTime() - m_fileInfo->GetTime()) / 86400,
				m_connection->GetNewsServer()->GetRetention());
			status = adFailed;
			FreeConnection(true);
		}

		if (m_connection && !IsStopped())
		{
			detail("Downloading %s @ %s", *m_infoName, *m_connectionName);
		}

		// test connection
		bool connected = m_connection && m_connection->Connect();
		// a server that refuses the login for good (502) can't serve this article: it
		// counts as tried. Waited for, a server with a broken account stalled every
		// download that had failed on the other servers of its level
		bool authRejected = !connected && m_connection && m_connection->GetAuthRejected();
		if (connected && !IsStopped())
		{
			NewsServer* newsServer = m_connection->GetNewsServer();

			// Download article
			status = Download();

			if (status == adFinished || status == adFailed || status == adNotFound || status == adCrcError)
			{
				int serverId = newsServer->GetId();
				int success = status == adFinished ? 1 : 0;
				int failed = status == adFinished ? 0 : 1;
				m_serverStats.StatOp(serverId, success, failed, ServerStatList::soSet);
				ServerVolume::Stats stats;
				stats.bytes = 0;
				stats.articles.failed = failed;
				stats.articles.success = success;
				g_StatMeter->AddServerStats(stats, serverId);
			}
		}

		if (m_connection)
		{
			AddServerStats();
		}

		if (m_contentRejected)
		{
			// the abandoned body still occupies the connection, so it must not
			// serve another request
			FreeConnection(false);
			status = adFailed;
			if (m_articleInfo->GetDupeFallbackRound() > 0)
			{
				// a duplicate's article that doesn't fit this file: every server
				// serves the same content, so it fails here (the next duplicate
				// source, if any, is tried by the queue coordinator)
				break;
			}
			// the release's own article with malformed headers: that server's copy
			// is bad, another server may have a good one - this server is done
			m_contentRejected = false;
			remainedRetries = 0;
		}

		if (!connected && m_connection)
		{
			detail("Article %s @ %s failed: could not establish connection", *m_infoName, *m_connectionName);
		}

		if (status == adConnectError)
		{
			connected = false;
			status = adFailed;
		}

		if (connected && status == adFailed)
		{
			remainedRetries--;
		}

		bool optionalBlocked = false;
		if (!connected && m_connection && !IsStopped())
		{
			g_ServerPool->BlockServer(lastServer);
			optionalBlocked = lastServer->GetOptional();
		}

		wantServer = nullptr;
		if (connected && status == adFailed && remainedRetries > 0 && !retentionFailure)
		{
			wantServer = lastServer;
		}
		else
		{
			FreeConnection(status == adFinished || status == adNotFound);
		}

		if (status == adFinished || status == adFatalError)
		{
			break;
		}

		if (IsStopped() || ((g_WorkState->GetPauseDownload() || g_WorkState->GetQuotaReached()) && !force) ||
			(g_WorkState->GetDownloadHeld() && !m_fileInfo->GetExtraPriority()) ||
			serverConfigGeneration != g_ServerPool->GetGeneration())
		{
			status = adRetry;
			break;
		}

		if (!wantServer && m_fileInfo->GetNzbInfo()->HasDesiredServer())
		{
			status = adFailed;
			break;
		}

		if (!wantServer && (connected || retentionFailure || optionalBlocked || authRejected))
		{
			if (!optionalBlocked)
			{
				failedServers.push_back(lastServer);
			}

			// if all servers from current level were tried, increase level
			// if all servers from all levels were tried, break the loop with failure status

			bool allServersOnLevelFailed = true;
			for (NewsServer* candidateServer : g_ServerPool->GetServers())
			{
				if (candidateServer->GetNormLevel() == level)
				{
					bool serverFailed = !candidateServer->GetActive() || candidateServer->GetMaxConnections() == 0 ||
						(candidateServer->GetOptional() && g_ServerPool->IsServerBlocked(candidateServer));
					if (!serverFailed)
					{
						for (NewsServer* ignoreServer : failedServers)
						{
							if (ignoreServer == candidateServer ||
								(ignoreServer->GetGroup() > 0 && ignoreServer->GetGroup() == candidateServer->GetGroup() &&
								 ignoreServer->GetNormLevel() == candidateServer->GetNormLevel()))
							{
								serverFailed = true;
								break;
							}
						}
					}
					if (!serverFailed)
					{
						allServersOnLevelFailed = false;
						break;
					}
				}
			}

			if (allServersOnLevelFailed)
			{
				if (level < g_ServerPool->GetMaxNormLevel())
				{
					detail("Article %s @ all level %i servers failed, increasing level", *m_infoName, level);
					level++;
				}
				else
				{
					detail("Article %s @ all servers failed", *m_infoName);
					status = adFailed;
					break;
				}
			}

			remainedRetries = retries;
		}
	}

	FreeConnection(status == adFinished);

	if (m_articleWriter.GetDuplicate())
	{
		status = adFinished;
	}

	if (status != adFinished && status != adRetry && status != adFatalError)
	{
		status = adFailed;
	}

	if (IsStopped())
	{
		detail("Download %s cancelled", *m_infoName);
		status = adRetry;
	}

	if (status == adFailed)
	{
		detail("Download %s failed", *m_infoName);
	}

	SetStatus(status);
	Notify(nullptr);

	debug("Exiting ArticleDownloader-loop");
}

ArticleDownloader::EStatus ArticleDownloader::Download()
{
	const char* response = nullptr;
	EStatus status = adRunning;
	m_writingStarted = false;
	m_localWriteError = false;
	m_contentRejected = false;
	m_articleInfo->SetCrc(0);

	if (m_contentAnalyzer)
	{
		m_contentAnalyzer->Reset();
	}

	if (m_connection->GetNewsServer()->GetJoinGroup())
	{
		// change group
		for (CString& group : m_fileInfo->GetGroups())
		{
			response = m_connection->JoinGroup(group);
			if (response && !strncmp(response, "2", 1))
			{
				break;
			}
		}

		status = CheckResponse(response, "could not join group");
		if (status != adFinished)
		{
			return status;
		}
	}

	// retrieve article
	response = m_connection->Request(BString<1024>("%s %s\r\n",
		g_Options->GetRawArticle() ? "ARTICLE" : "BODY", m_articleInfo->GetMessageId()));

	status = CheckResponse(response, "could not fetch article");
	if (status != adFinished)
	{
		return status;
	}

	m_decoder.Clear();
	m_decoder.SetCrcCheck(g_Options->GetCrcCheck());
	m_decoder.SetRawMode(g_Options->GetRawArticle());

	status = adRunning;
	// (128 bytes past what is read into it: the decoder may write up to one uuencoded
	// line's worth more than it was given)
	int readChunk = g_Options->GetArticleReadChunkSize();
	CharBuffer lineBuf(readChunk + 128);

	while (!IsStopped() && !m_decoder.GetEof())
	{
		// throttle the bandwidth
		while (!IsStopped() && (g_WorkState->GetSpeedLimit() > 0.0f) &&
			(g_StatMeter->CalcCurrentDownloadSpeed() > g_WorkState->GetSpeedLimit() ||
			g_StatMeter->CalcMomentaryDownloadSpeed() > g_WorkState->GetSpeedLimit()))
		{
			SetLastUpdateTimeNow();
			Util::Sleep(10);
		}

		char* buffer;
		int len;
		m_connection->ReadBuffer(&buffer, &len);
		if (len == 0)
		{
			len = m_connection->TryRecv(lineBuf, readChunk);
			buffer = lineBuf;
		}

		// have we encountered a timeout?
		if (len <= 0)
		{
			if (!IsStopped())
			{
				detail("Article %s @ %s failed: Unexpected end of article", *m_infoName, *m_connectionName);
			}
			// the rest of this body may still arrive: the connection isn't used again (it
			// was, and late body bytes were read as the reply to the next request - at
			// worst another article's body decoded into this file)
			m_connection->Disconnect();
			status = adFailed;
			break;
		}

		g_StatMeter->AddSpeedReading(len);
		time_t oldTime = m_lastUpdateTime;
		SetLastUpdateTimeNow();
		if (oldTime != m_lastUpdateTime)
		{
			AddServerStats();
		}

		// decode article data
		len = m_decoder.DecodeBuffer(buffer, len);

		// write to output file
		if (len > 0 && !Write(buffer, len))
		{
			if (m_localWriteError)
			{
				status = adFatalError;
			}
			else
			{
				// the article's content was rejected before its body was fully
				// read: the rest of the body is still pending on the connection
				m_contentRejected = true;
				status = adFailed;
			}
			break;
		}
	}

	if (IsStopped())
	{
		status = adFailed;
	}

	if (status == adRunning)
	{
		FreeConnection(true);
		status = DecodeCheck();
	}

	if (m_writingStarted)
	{
		m_articleWriter.Finish(status == adFinished);
	}

	if (status == adFinished)
	{
		detail("Successfully downloaded %s", *m_infoName);
	}

	return status;
}

ArticleDownloader::EStatus ArticleDownloader::CheckResponse(const char* response, const char* comment)
{
	if (!response)
	{
		if (!IsStopped())
		{
			detail("Article %s @ %s failed, %s: Connection closed by remote host",
				*m_infoName, *m_connectionName, comment);
		}
		return adConnectError;
	}
	else if (m_connection->GetAuthError() || !strncmp(response, "400", 3) || !strncmp(response, "499", 3))
	{
		detail("Article %s @ %s failed, %s: %s", *m_infoName, *m_connectionName, comment, response);
		return adConnectError;
	}
	else if (!strncmp(response, "41", 2) || !strncmp(response, "42", 2) || !strncmp(response, "43", 2) ||
		!strncmp(response, "451", 3))
	{
		// 451 is how some providers (super.newsgroupdirect.com) say "no such
		// article": treat it like 430, not as an unknown error that is retried
		// on the same server after <ArticleInterval>
		detail("Article %s @ %s failed, %s: %s", *m_infoName, *m_connectionName, comment, response);
		return adNotFound;
	}
	else if (!strncmp(response, "2", 1))
	{
		// OK
		return adFinished;
	}
	else
	{
		// unknown error, no special handling
		detail("Article %s @ %s failed, %s: %s", *m_infoName, *m_connectionName, comment, response);
		return adFailed;
	}
}

bool ArticleDownloader::Write(char* buffer, int len)
{
	const char* articleFilename = nullptr;
	int64 articleFileSize = 0;
	int64 articleOffset = 0;
	int articleSize = 0;

	if (!m_writingStarted)
	{
		if (!g_Options->GetRawArticle())
		{
			articleFilename = m_decoder.GetArticleFilename();
			if (m_decoder.GetFormat() == Decoder::efYenc)
			{
				if (m_decoder.GetBeginPos() == 0 || m_decoder.GetEndPos() == 0)
				{
					return false;
				}
				articleFileSize = m_decoder.GetSize();
				articleOffset = m_decoder.GetBeginPos() - 1;
				// the range in 64 bits, inside the file size the article declares: cut
				// to an int, a huge range could pass as a small one (F24), and an offset
				// far past the file had the file filled with zeros up to it (F23)
				int64 rangeSize = m_decoder.GetEndPos() - m_decoder.GetBeginPos() + 1;
				if (articleOffset < 0 || rangeSize <= 0 || rangeSize > 1024*1024*1024 ||
					(articleFileSize > 0 && articleOffset + rangeSize > articleFileSize))
				{
					warn("Malformed article %s: range %lli-%lli out of range (file size %lli)", *m_infoName,
						(long long)m_decoder.GetBeginPos(), (long long)m_decoder.GetEndPos(), (long long)articleFileSize);
					return false;
				}
				articleSize = (int)rangeSize;
				m_decodedFileSize = articleFileSize;
				int64 expectedFileSize = m_fileInfo->GetDecodedFileSize();
				if (m_articleInfo->GetDupeFallbackRound() > 0 && expectedFileSize > 0 &&
					articleFileSize != expectedFileSize)
				{
					detail("Discarding article %s from duplicate: file size mismatch (%lli vs %lli)",
						*m_infoName, (long long)articleFileSize, (long long)expectedFileSize);
					return false;
				}
				// a substituted article must decode into exactly the byte range the
				// target article occupies; the expected range was pinned from finished
				// neighbour articles at substitution time (-1 = not known yet). This
				// rejects donors with drifted decoded boundaries before any bytes are
				// written (a mis-placed article would gap-fill and/or overwrite a
				// neighbour's decoded bytes).
				int64 dupeExpectedOffset = m_articleInfo->GetDupeExpectedOffset();
				if (m_articleInfo->GetDupeFallbackRound() > 0 && dupeExpectedOffset >= 0 &&
					articleOffset != dupeExpectedOffset)
				{
					detail("Discarding article %s from duplicate: decoded offset mismatch (%lli vs %lli)",
						*m_infoName, (long long)articleOffset, (long long)dupeExpectedOffset);
					return false;
				}
				int64 dupeExpectedEnd = m_articleInfo->GetDupeExpectedEnd();
				if (m_articleInfo->GetDupeFallbackRound() > 0 && dupeExpectedEnd >= 0 &&
					articleOffset + articleSize != dupeExpectedEnd)
				{
					detail("Discarding article %s from duplicate: decoded end mismatch (%lli vs %lli)",
						*m_infoName, (long long)(articleOffset + articleSize), (long long)dupeExpectedEnd);
					return false;
				}
			}
		}

		if (!m_articleWriter.Start(m_decoder.GetFormat(), articleFilename, articleFileSize, articleOffset, articleSize))
		{
			// both a write failure and a duplicate-existing-file result end the
			// download of this article on the spot (a duplicate is then reported
			// as finished by Run)
			m_localWriteError = true;
			return false;
		}
		m_writingStarted = true;
	}

	bool ok = m_articleWriter.Write(buffer, len);
	if (!ok)
	{
		m_localWriteError = true;
	}

	if (m_contentAnalyzer)
	{
		m_contentAnalyzer->Append(buffer, len);
	}

	return ok;
}

ArticleDownloader::EStatus ArticleDownloader::DecodeCheck()
{
	if (!g_Options->GetRawArticle())
	{
		Decoder::EStatus status = m_decoder.Check();

		// an article decoding to more bytes than its range holds: the bytes past it were
		// dropped, but its size counted them, and the cache padded the segment with
		// memory never written - into the file, past its end or over the next article
		if (status == Decoder::dsFinished && m_decoder.GetFormat() == Decoder::efYenc &&
			!g_Options->GetRawArticle() && m_articleWriter.GetRangeExceeded())
		{
			detail("Decoding %s failed: more data than its range", *m_infoName);
			return adFailed;
		}

		if (status == Decoder::dsFinished)
		{
			if (m_decoder.GetArticleFilename())
			{
				m_articleFilename = m_decoder.GetArticleFilename();
			}

			if (m_decoder.GetFormat() == Decoder::efYenc)
			{
				m_articleInfo->SetCrc(g_Options->GetCrcCheck() ?
					m_decoder.GetCalculatedCrc() : m_decoder.GetExpectedCrc());
			}

			return adFinished;
		}
		else if (status == Decoder::dsCrcError)
		{
			detail("Decoding %s failed: CRC-Error", *m_infoName);
			return adCrcError;
		}
		else if (status == Decoder::dsArticleIncomplete)
		{
			detail("Decoding %s failed: article incomplete", *m_infoName);
			return adFailed;
		}
		else if (status == Decoder::dsInvalidSize)
		{
			detail("Decoding %s failed: size mismatch", *m_infoName);
			return adFailed;
		}
		else if (status == Decoder::dsNoBinaryData)
		{
			detail("Decoding %s failed: no binary data found", *m_infoName);
			return adFailed;
		}
		else
		{
			detail("Decoding %s failed", *m_infoName);
			return adFailed;
		}
	}
	else
	{
		return adFinished;
	}
}

void ArticleDownloader::SetLastUpdateTimeNow()
{
	m_lastUpdateTime = Util::CurrentTime();
}

void ArticleDownloader::LogDebugInfo()
{
	info("      Download: status=%i, LastUpdateTime=%s, InfoName=%s", m_status,
		 *Util::FormatTime(m_lastUpdateTime.load()), *m_infoName);
}

void ArticleDownloader::Stop()
{
	debug("Trying to stop ArticleDownloader");
	Thread::Stop();
	Guard guard(m_connectionMutex);
	if (m_connection)
	{
		m_connection->SetSuppressErrors(true);
		m_connection->Cancel();
	}
	debug("ArticleDownloader stopped successfully");
}

void ArticleDownloader::FreeConnection(bool keepConnected)
{
	if (m_connection)
	{
		debug("Releasing connection");
		Guard guard(m_connectionMutex);
		if (!keepConnected || m_connection->GetStatus() == Connection::csCancelled)
		{
			m_connection->Disconnect();
		}
		AddServerStats();
		g_ServerPool->FreeConnection(m_connection, true);
		m_connection = nullptr;
	}
}

void ArticleDownloader::AddServerStats()
{
	int serverId = m_connection->GetNewsServer()->GetId();
	int bytesRead = m_connection->FetchTotalBytesRead();
	ServerVolume::Stats stats;
	stats.bytes = Util::SafeIntCast<int, uint32>(bytesRead);
	stats.articles.failed = 0;
	stats.articles.success = 0;
	g_StatMeter->AddServerStats(stats, serverId);
	m_downloadedSize += bytesRead;
}
