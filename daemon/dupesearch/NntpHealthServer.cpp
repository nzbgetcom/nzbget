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
#include <map>
#include <set>
#include "NntpHealthServer.h"
#include "ArticleFetcher.h"
#include "HttpGet.h"
#include "NewsServer.h"
#include "NntpConnection.h"
#include "ServerPool.h"
#include "Util.h"

namespace
{

// a message-id as NNTP commands take it: in angle brackets (RFC 3977 3.6); nzb-files
// list it without them
std::string Bracketed(const std::string& messageId)
{
	if (!messageId.empty() && messageId.front() == '<' && messageId.back() == '>')
	{
		return messageId;
	}
	return "<" + messageId + ">";
}

}

using Answer = DonorHealth::Answer;

int NntpHealthServer::CapFor(int connections)
{
	return std::max(1, std::min(MaxCap, connections / 2));
}

DonorHealth::ServerList NntpHealthServer::Servers()
{
	static std::mutex mutex;
	static std::map<int, std::shared_ptr<NntpHealthServer>> known;

	std::lock_guard<std::mutex> guard(mutex);
	DonorHealth::ServerList list;
	std::set<int> groups;
	for (NewsServer* server : g_ServerPool->GetServers())
	{
		if (!server->GetActive() || server->GetMaxConnections() == 0)
		{
			continue;
		}
		if (server->GetGroup() > 0 && !groups.insert(server->GetGroup()).second)
		{
			continue;
		}
		std::shared_ptr<NntpHealthServer>& entry = known[server->GetId()];
		if (!entry || entry->Cap() != CapFor(server->GetMaxConnections()))
		{
			entry = std::make_shared<NntpHealthServer>(server->GetId(), CapFor(server->GetMaxConnections()));
		}
		list.push_back(entry);
	}
	return list;
}

std::vector<Answer> NntpHealthServer::Exchange(const std::vector<DonorHealth::Request>& batch)
{
	NewsServer* server = g_ServerPool->GetServerById(m_serverId);
	if (!server)
	{
		throw DonorHealth::ExchangeError();
	}

	// a connection of the pool: it stays the server's, only borrowed
	NntpConnection* connection = nullptr;
	time_t waitStart = Util::CurrentTime();
	while (!connection && !HttpGet::Stopped() && Util::CurrentTime() - waitStart <= ConnectionWaitSec)
	{
		connection = g_ServerPool->GetConnection(server->GetNormLevel(), server, nullptr);
		if (!connection)
		{
			Util::Sleep(20);
		}
	}
	if (!connection)
	{
		throw DonorHealth::ExchangeError();
	}

	struct Release
	{
		NntpConnection* connection;
		bool broken = false;
		~Release()
		{
			if (broken)
			{
				connection->Disconnect();
			}
			g_ServerPool->FreeConnection(connection, true);
		}
	} release{ connection };

	if (!connection->Connect())
	{
		release.broken = true;
		throw DonorHealth::ExchangeError();
	}

	if (server->GetJoinGroup() && batch.front().groups)
	{
		const char* joined = nullptr;
		for (const std::string& group : *batch.front().groups)
		{
			joined = connection->JoinGroup(group.c_str());
			if (joined && !strncmp(joined, "2", 1))
			{
				break;
			}
		}
		if (!joined || strncmp(joined, "2", 1))
		{
			release.broken = !joined;
			throw DonorHealth::ExchangeError();
		}
	}

	// all STATs in one write, then their replies in order (RFC 3977 section 3.5)
	std::string commands;
	for (const DonorHealth::Request& request : batch)
	{
		commands += "STAT " + Bracketed(request.messageId) + "\r\n";
	}
	if (!connection->Send(commands.c_str(), (int)commands.size()))
	{
		release.broken = true;
		throw DonorHealth::ExchangeError();
	}

	// 223 present; 430 definitively missing; any other reply (some providers
	// say 451 for a missing article) is an answer on a healthy connection that
	// proves nothing
	std::vector<Answer> answers;
	for (size_t i = 0; i < batch.size(); i++)
	{
		char line[1024];
		int length = 0;
		if (!connection->ReadLine(line, sizeof(line), &length))
		{
			release.broken = true;
			DonorHealth::ExchangeError error;
			error.answers = answers;
			throw error;
		}
		answers.push_back(!strncmp(line, "223", 3) ? Answer::Present : !strncmp(line, "430", 3) ? Answer::Missing : Answer::Error);
	}

	// BODY for the articles that have a gate: one server at a time, verified
	for (size_t i = 0; i < batch.size(); i++)
	{
		if (!batch[i].gate || answers[i] != Answer::Present)
		{
			continue;
		}
		try
		{
			answers[i] = BodyOnce(batch[i].gate, [&]() -> Answer
				{
					std::vector<CString> groups;
					if (batch[i].groups)
					{
						for (const std::string& group : *batch[i].groups)
						{
							groups.emplace_back(group.c_str());
						}
					}
					ArticleFetcher fetcher;
					ArticleFetcher::FetchedArticle article = fetcher.FetchFromConnection(connection, Bracketed(batch[i].messageId).c_str(), groups);
					if (article.Success)
					{
						return Answer::Present;
					}
					if (article.Transient)
					{
						// the connection broke mid-body
						throw DonorHealth::ExchangeError();
					}
					return Answer::BodyBad;
				});
		}
		catch (DonorHealth::ExchangeError&)
		{
			// the STAT answers already read stay; the unread bodies are unknown
			release.broken = true;
			DonorHealth::ExchangeError error;
			error.answers = answers;
			error.answers[i] = Answer::Error;
			for (size_t j = i + 1; j < batch.size(); j++)
			{
				if (batch[j].gate && answers[j] == Answer::Present)
				{
					error.answers[j] = Answer::Error;
				}
			}
			throw error;
		}
	}
	return answers;
}
