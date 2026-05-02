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


#include <deque>

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

static ArticleInfo* ReserveNextPipelinedArticle(FileInfo* fileInfo, ArticleInfo* currentArticle)
{
	if (!fileInfo || !currentArticle)
	{
		return nullptr;
	}

	ArticleInfo* candidate = nullptr;
	int currentPart = currentArticle->GetPartNumber();

	GuardedDownloadQueue guard = DownloadQueue::Guard();
	for (auto& articlePtr : *fileInfo->GetArticles())
	{
		ArticleInfo* article = articlePtr.get();
		if (!article || article->GetStatus() != ArticleInfo::aiUndefined)
		{
			continue;
		}

		if (article->GetPartNumber() > currentPart)
		{
			if (!candidate || article->GetPartNumber() < candidate->GetPartNumber())
			{
				candidate = article;
			}
		}
	}

	if (!candidate)
	{
		for (auto& articlePtr : *fileInfo->GetArticles())
		{
			ArticleInfo* article = articlePtr.get();
			if (!article || article->GetStatus() != ArticleInfo::aiUndefined)
			{
				continue;
			}
			if (!candidate || article->GetPartNumber() < candidate->GetPartNumber())
			{
				candidate = article;
			}
		}
	}

	if (candidate)
	{
		candidate->SetStatus(ArticleInfo::aiRunning);
	}

	return candidate;
}

static void RestorePipelinedArticle(ArticleInfo* article)
{
	if (!article)
	{
		return;
	}

	GuardedDownloadQueue guard = DownloadQueue::Guard();
	if (article->GetStatus() == ArticleInfo::aiRunning)
	{
		article->SetStatus(ArticleInfo::aiUndefined);
	}
}

static void UpdateArticleCompletion(FileInfo* fileInfo, NzbInfo* nzbInfo, ArticleInfo* articleInfo, bool success)
{
	if (!fileInfo || !nzbInfo || !articleInfo)
	{
		return;
	}

	if (success)
	{
		articleInfo->SetStatus(ArticleInfo::aiFinished);
		fileInfo->SetSuccessSize(fileInfo->GetSuccessSize() + articleInfo->GetSize());
		nzbInfo->SetCurrentSuccessSize(nzbInfo->GetCurrentSuccessSize() + articleInfo->GetSize());
		nzbInfo->SetParCurrentSuccessSize(nzbInfo->GetParCurrentSuccessSize() + (fileInfo->GetParFile() ? articleInfo->GetSize() : 0));
		fileInfo->SetSuccessArticles(fileInfo->GetSuccessArticles() + 1);
		nzbInfo->SetCurrentSuccessArticles(nzbInfo->GetCurrentSuccessArticles() + 1);
	}
	else
	{
		articleInfo->SetStatus(ArticleInfo::aiFailed);
		fileInfo->SetFailedSize(fileInfo->GetFailedSize() + articleInfo->GetSize());
		nzbInfo->SetCurrentFailedSize(nzbInfo->GetCurrentFailedSize() + articleInfo->GetSize());
		nzbInfo->SetParCurrentFailedSize(nzbInfo->GetParCurrentFailedSize() + (fileInfo->GetParFile() ? articleInfo->GetSize() : 0));
		fileInfo->SetFailedArticles(fileInfo->GetFailedArticles() + 1);
	}
	fileInfo->SetCompletedArticles(fileInfo->GetCompletedArticles() + 1);
}

ArticleDownloader::ArticleDownloader()
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
			(g_WorkState->GetTempPauseDownload() && !m_fileInfo->GetExtraPriority()) ||
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
		if (connected && !IsStopped())
		{
			NewsServer* newsServer = m_connection->GetNewsServer();

			// Download article
			status = Download();

			if (status == adFinished || status == adFailed || status == adNotFound || status == adCrcError)
			{
				for (ServerStat& serverStat : m_serverStats)
				{
					ServerVolume::Stats stats;
					stats.bytes = 0;
					stats.articles.failed = serverStat.GetFailedArticles();
					stats.articles.success = serverStat.GetSuccessArticles();
					g_StatMeter->AddServerStats(stats, serverStat.GetServerId());
				}
			}
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
			(g_WorkState->GetTempPauseDownload() && !m_fileInfo->GetExtraPriority()) ||
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

		if (!wantServer && (connected || retentionFailure || optionalBlocked))
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

	if (status != adFinished && status != adRetry)
	{
		status = adFailed;
	}

	if (IsStopped() && status != adFinished)
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
	const char* articleResponse = nullptr;
	EStatus status = adRunning;
	m_writingStarted = false;
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
			articleResponse = m_connection->JoinGroup(group);
			if (articleResponse && !strncmp(articleResponse, "2", 1))
			{
				break;
			}
		}

		status = CheckResponse(articleResponse, "could not join group");
		if (status != adFinished)
		{
			return status;
		}
	}

	struct PipelineEntry
	{
		ArticleInfo* article;
		CString request;
		PipelineEntry(ArticleInfo* article_, CString&& request_) noexcept : article(article_), request(std::move(request_)) {}
	};

	std::deque<PipelineEntry> pipeline;
	ArticleInfo* currentArticle = m_articleInfo;
	ArticleInfo* originalArticle = m_articleInfo;
	CString request;
	bool firstResponseReady = true;

	auto RestorePipeline = [&pipeline]() {
		for (size_t i = 1; i < pipeline.size(); ++i)
		{
			RestorePipelinedArticle(pipeline[i].article);
		}
		pipeline.clear();
	};

	auto FillPipeline = [&]() {
		int pipelineDepth = m_connection->GetNewsServer()->GetPipelineDepth();
		if (pipelineDepth < 1)
		{
			pipelineDepth = 1;
		}
		while ((int)pipeline.size() < pipelineDepth)
		{
			ArticleInfo* tailArticle = pipeline.empty() ? currentArticle : pipeline.back().article;
			ArticleInfo* nextArticle = ReserveNextPipelinedArticle(m_fileInfo, tailArticle);
			if (!nextArticle)
			{
				break;
			}

			CString nextRequest;
			nextRequest.Format("%s %s\r\n",
				g_Options->GetRawArticle() ? "ARTICLE" : "BODY", nextArticle->GetMessageId());
			if (!m_connection->SendRequest(nextRequest))
			{
				AddServerStats();
				RestorePipelinedArticle(nextArticle);
				break;
			}

			pipeline.emplace_back(nextArticle, std::move(nextRequest));
		}
	};

	request.Format("%s %s\r\n",
		g_Options->GetRawArticle() ? "ARTICLE" : "BODY", currentArticle->GetMessageId());
	articleResponse = m_connection->Request(request);
	status = CheckResponse(articleResponse, "could not fetch article");
	if (status != adFinished)
	{
		return status;
	}

	pipeline.emplace_back(currentArticle, std::move(request));
	FillPipeline();

	while (!IsStopped() && !pipeline.empty())
	{
		currentArticle = pipeline.front().article;
		if (currentArticle != originalArticle)
		{
			m_articleWriter.SetArticleInfo(currentArticle);
			m_articleInfo = currentArticle;
			if (m_contentAnalyzer)
			{
				m_contentAnalyzer.reset();
			}
		}

		currentArticle->SetCrc(0);

		if (!firstResponseReady)
		{
			articleResponse = m_connection->ReadResponseLine(pipeline.front().request);
		}
		firstResponseReady = false;

		status = CheckResponse(articleResponse, "could not fetch article");
		if (status != adFinished)
		{
			AddServerStats();
			RestorePipeline();
			break;
		}

		m_decoder.Clear();
		m_decoder.SetCrcCheck(g_Options->GetCrcCheck());
		m_decoder.SetRawMode(g_Options->GetRawArticle());

		status = adRunning;
		CharBuffer lineBuf(g_Options->GetArticleReadChunkSize());

		while (!IsStopped() && !m_decoder.GetEof())
		{
			while (!IsStopped() && (g_WorkState->GetSpeedLimit() > 0.0f) &&
				(g_StatMeter->CalcCurrentDownloadSpeed() > g_WorkState->GetSpeedLimit() ||
				g_StatMeter->CalcMomentaryDownloadSpeed() > g_WorkState->GetSpeedLimit()))
			{
				SetLastUpdateTimeNow();
				Util::Sleep(10);
			}

			int bytesRead = 0;
			char* buffer = m_connection->ReadLine(lineBuf, lineBuf.Size(), &bytesRead);
			if (!buffer || bytesRead <= 0)
			{
				if (!IsStopped())
				{
					detail("Article %s @ %s failed: Unexpected end of article", *m_infoName, *m_connectionName);
				}
				status = adFailed;
				break;
			}

			g_StatMeter->AddSpeedReading(bytesRead);
			SetLastUpdateTimeNow();
			AddServerStats();

			int len = m_decoder.DecodeBuffer(buffer, bytesRead);
			if (len > 0 && !Write(buffer, len))
			{
				status = adFatalError;
				break;
			}
		}

		if (IsStopped())
		{
			status = adFailed;
		}

		if (status == adRunning)
		{
			status = DecodeCheck();
		}

		if (m_writingStarted)
		{
			m_articleWriter.Finish(status == adFinished);
			m_writingStarted = false;
		}

		if (status == adFinished)
		{
			if (m_infoName.Empty())
			{
				SetInfoName(currentArticle->GetResultFilename());
			}
			detail("Successfully downloaded %s", *m_infoName);
			UpdateArticleCompletion(m_fileInfo, m_fileInfo->GetNzbInfo(), currentArticle, true);
			int serverId = m_connection->GetNewsServer()->GetId();
			m_serverStats.StatOp(serverId, 1, 0, ServerStatList::soAdd);
		}
		else
		{
			UpdateArticleCompletion(m_fileInfo, m_fileInfo->GetNzbInfo(), currentArticle, false);
			int serverId = m_connection->GetNewsServer()->GetId();
			m_serverStats.StatOp(serverId, 0, 1, ServerStatList::soAdd);
			AddServerStats();
			RestorePipeline();
			break;
		}

		pipeline.pop_front();
		if (pipeline.empty())
		{
			break;
		}

		FillPipeline();
		currentArticle = pipeline.front().article;
		m_articleInfo = currentArticle;
		BString<1024> infoName("%s%c%s [%i/%i]", m_fileInfo->GetNzbInfo()->GetName(), PATH_SEPARATOR,
			m_fileInfo->GetFilename(), currentArticle->GetPartNumber(), (int)m_fileInfo->GetArticles()->size());
		SetInfoName(infoName);
	}

	if (IsStopped())
	{
		status = adFailed;
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
	else if (!strncmp(response, "41", 2) || !strncmp(response, "42", 2) || !strncmp(response, "43", 2))
	{
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
				articleSize = (int)(m_decoder.GetEndPos() - m_decoder.GetBeginPos() + 1);
				if (articleSize <= 0 || articleSize > 1024*1024*1024)
				{
					warn("Malformed article %s: size %i out of range", *m_infoName, articleSize);
					return false;
				}
			}
		}

		if (!m_articleWriter.Start(m_decoder.GetFormat(), articleFilename, articleFileSize, articleOffset, articleSize))
		{
			return false;
		}
		m_writingStarted = true;
	}

	bool ok = m_articleWriter.Write(buffer, len);

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
