/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2025-2026 Denis <denis@nzbget.com>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"

#include <atomic>
#include <string>
#include <thread>

#include <boost/test/unit_test.hpp>
#include "ServerPool.h"
#include "Options.h"
#include "DiskState.h"
#include "ConnectionIdlePolicy.h"

BOOST_AUTO_TEST_SUITE(NNTPTest)

// Minimal loopback server for driving real pooled NntpConnection instances. It binds
// an ephemeral port, sends a greeting, and acknowledges QUIT before accepting again.
class LoopbackNntpServer
{
public:
	LoopbackNntpServer()
	{
		Connection::Init();
	}

	~LoopbackNntpServer()
	{
		m_stopping = true;
		CloseSocket(m_listenSocket);
		if (m_thread.joinable())
		{
			m_thread.join();
		}
		Connection::Final();
	}

	int Start()
	{
		SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (listenSocket == INVALID_SOCKET)
		{
			return 0;
		}

		int on = 1;
		setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR,
			reinterpret_cast<const char*>(&on), sizeof(on));

		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr.sin_port = 0;

		if (bind(listenSocket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0 ||
			listen(listenSocket, 1) != 0)
		{
			closesocket(listenSocket);
			return 0;
		}

		socklen_t len = sizeof(addr);
		if (getsockname(listenSocket, reinterpret_cast<struct sockaddr*>(&addr), &len) != 0)
		{
			closesocket(listenSocket);
			return 0;
		}

		m_listenSocket = listenSocket;
		m_thread = std::thread(&LoopbackNntpServer::Run, this);
		return ntohs(addr.sin_port);
	}

private:
	static void CloseSocket(std::atomic<SOCKET>& socket)
	{
		SOCKET ownedSocket = socket.exchange(INVALID_SOCKET);
		if (ownedSocket != INVALID_SOCKET)
		{
			shutdown(ownedSocket, SHUT_RDWR);
			closesocket(ownedSocket);
		}
	}

	static void SendLine(SOCKET socket, const char* line)
	{
		std::string data = std::string(line) + "\r\n";
		send(socket, data.c_str(), static_cast<int>(data.size()), 0);
	}

	void Run()
	{
		while (!m_stopping)
		{
			SOCKET listenSocket = m_listenSocket;
			if (listenSocket == INVALID_SOCKET)
			{
				break;
			}

			SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
			if (clientSocket == INVALID_SOCKET)
			{
				break;
			}

			if (m_stopping)
			{
				shutdown(clientSocket, SHUT_RDWR);
				closesocket(clientSocket);
				break;
			}

			SendLine(clientSocket, "200 Welcome");

			char buffer[64];
			if (recv(clientSocket, buffer, sizeof(buffer), 0) > 0)
			{
				SendLine(clientSocket, "205 Connection closing");
			}

			shutdown(clientSocket, SHUT_RDWR);
			closesocket(clientSocket);
		}
	}

	std::atomic<SOCKET> m_listenSocket{INVALID_SOCKET};
	std::atomic<bool> m_stopping{false};
	std::thread m_thread;
};

void AddTestServer(ServerPool* pool, int id, bool active, int level, bool optional, int group, int connections)
{
	pool->AddServer(std::make_unique<NewsServer>(id, active, nullptr, "", 119, 0,
		"", "", false, false, nullptr, connections, 0, level, group, optional, Options::cvStrict));
}

void TestBlockServers(int group)
{
	ServerVolume::VolumeArray arr;
	ServerPool pool;
	AddTestServer(&pool, 1, true, 0, false, group, 2);
	AddTestServer(&pool, 2, true, 0, false, group, 2);
	AddTestServer(&pool, 3, true, 1, false, 0, 2);
	pool.InitConnections();
	pool.SetRetryInterval(60);

	NewsServer* serv1 = pool.GetServers()->at(0).get();
	pool.BlockServer(serv1);

	NntpConnection* con1 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con2 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con3 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con4 = pool.GetConnection(0, nullptr, nullptr);
	BOOST_CHECK(con1 != nullptr);
	BOOST_CHECK(con2 != nullptr);
	BOOST_CHECK(con3 == nullptr);
	BOOST_CHECK(con4 == nullptr);
	BOOST_CHECK(con2->GetNewsServer()->GetLevel() == 0);

	if (con1) pool.FreeConnection(con1, false);
	if (con2) pool.FreeConnection(con2, false);
}


void TestOptionalBlockServers(int group)
{
	ServerPool pool;
	AddTestServer(&pool, 1, true, 0, true, group, 2);
	AddTestServer(&pool, 2, true, 0, true, group, 2);
	AddTestServer(&pool, 3, true, 1, false, 0, 2);
	pool.InitConnections();
	pool.SetRetryInterval(60);

	NewsServer* serv1 = pool.GetServers()->at(0).get();
	NewsServer* serv2 = pool.GetServers()->at(1).get();
	pool.BlockServer(serv1);
	pool.BlockServer(serv2);

	NntpConnection* con1 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con2 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con3 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con4 = pool.GetConnection(0, nullptr, nullptr);

	// all servers on level 0 are optional and blocked;
	// we should get a connection from level-1 server (server 3)
	BOOST_CHECK(con1 != nullptr);
	BOOST_CHECK(con2 != nullptr);
	BOOST_CHECK(con3 == nullptr);
	BOOST_CHECK(con4 == nullptr);
	BOOST_CHECK(con1->GetNewsServer()->GetLevel() == 1);
	BOOST_CHECK(con2->GetNewsServer()->GetLevel() == 1);
}

void TestBlockOptionalAndNonOptionalServers(int group)
{
	ServerPool pool;
	AddTestServer(&pool, 1, true, 0, true, group, 2);
	AddTestServer(&pool, 2, true, 0, false, group, 2);
	AddTestServer(&pool, 3, true, 1, false, 0, 2);
	pool.InitConnections();
	pool.SetRetryInterval(60);

	NewsServer* serv1 = pool.GetServers()->at(0).get();
	NewsServer* serv2 = pool.GetServers()->at(1).get();
	pool.BlockServer(serv1);
	pool.BlockServer(serv2);

	NntpConnection* con1 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con2 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con3 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con4 = pool.GetConnection(0, nullptr, nullptr);

	// all servers on level 0 are blocked but one of them is non-optional
	// we should NOT get any connections
	BOOST_CHECK(con1 == nullptr);
	BOOST_CHECK(con2 == nullptr);
	BOOST_CHECK(con3 == nullptr);
	BOOST_CHECK(con4 == nullptr);
}

BOOST_AUTO_TEST_CASE(SimpleLevelsTest)
{
	ServerPool pool;
	AddTestServer(&pool, 1, true, 2, false, 0, 2);
	pool.InitConnections();
	BOOST_CHECK(pool.GetMaxNormLevel() == 0);

	AddTestServer(&pool, 2, true, 10, false, 0, 3);
	pool.InitConnections();
	BOOST_CHECK(pool.GetMaxNormLevel() == 1);

	NntpConnection* con1 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con2 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con3 = pool.GetConnection(0, nullptr, nullptr);
	BOOST_CHECK(con1 != nullptr);
	BOOST_CHECK(con2 != nullptr);
	BOOST_CHECK(con3 == nullptr);

	pool.FreeConnection(con1, false);
	con3 = pool.GetConnection(0, nullptr, nullptr);
	BOOST_CHECK(con3 != nullptr);

	con1 = pool.GetConnection(1, nullptr, nullptr);
	con2 = pool.GetConnection(1, nullptr, nullptr);
	con3 = pool.GetConnection(1, nullptr, nullptr);
	NntpConnection* con4 = pool.GetConnection(1, nullptr, nullptr);
	BOOST_CHECK(con1 != nullptr);
	BOOST_CHECK(con2 != nullptr);
	BOOST_CHECK(con3 != nullptr);
	BOOST_CHECK(con4 == nullptr);
}

BOOST_AUTO_TEST_CASE(WantServerTest)
{
	ServerPool pool;
	AddTestServer(&pool, 1, true, 0, false, 0, 2);
	AddTestServer(&pool, 2, true, 0, false, 0, 1);
	AddTestServer(&pool, 3, true, 1, false, 0, 3);
	pool.InitConnections();

	NewsServer* serv1 = pool.GetServers()->at(0).get();

	NntpConnection* con1 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con2 = pool.GetConnection(0, serv1, nullptr);
	NntpConnection* con3 = pool.GetConnection(0, serv1, nullptr);
	BOOST_CHECK(con1 != nullptr);
	BOOST_CHECK(con2 != nullptr);
	BOOST_CHECK(con3 == nullptr);
}

BOOST_AUTO_TEST_CASE(ActiveOnOffTest)
{
	ServerPool pool;
	AddTestServer(&pool, 1, true, 0, false, 0, 2);
	AddTestServer(&pool, 2, true, 0, false, 0, 1);
	pool.InitConnections();

	NntpConnection* con1 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con2 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con3 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con4 = pool.GetConnection(0, nullptr, nullptr);
	BOOST_TEST(con1 != nullptr);
	BOOST_TEST(con2 != nullptr);
	BOOST_TEST(con3 != nullptr);
	BOOST_TEST(con4 == nullptr);

	pool.FreeConnection(con1, false);
	pool.FreeConnection(con2, false);
	pool.FreeConnection(con3, false);

	BOOST_CHECK(pool.GetGeneration() == 1);

	NewsServer* serv1 = pool.GetServers()->at(0).get();
	serv1->SetActive(false);
	pool.Changed();
	BOOST_CHECK(pool.GetGeneration() == 2);

	con1 = pool.GetConnection(0, nullptr, nullptr);
	con2 = pool.GetConnection(0, nullptr, nullptr);
	con3 = pool.GetConnection(0, nullptr, nullptr);
	BOOST_CHECK(con1 != nullptr);
	BOOST_CHECK(con2 == nullptr);
	BOOST_CHECK(con3 == nullptr);
}

BOOST_AUTO_TEST_CASE(IgnoreServersTest)
{
	ServerPool pool;
	AddTestServer(&pool, 1, true, 0, false, 0, 2);
	AddTestServer(&pool, 2, true, 0, false, 0, 2);
	pool.InitConnections();

	NewsServer* serv1 = pool.GetServers()->at(0).get();
	ServerPool::RawServerList ignoreServers;
	ignoreServers.push_back(serv1);

	NntpConnection* con1 = pool.GetConnection(0, nullptr, &ignoreServers);
	NntpConnection* con2 = pool.GetConnection(0, nullptr, &ignoreServers);
	NntpConnection* con3 = pool.GetConnection(0, nullptr, &ignoreServers);
	NntpConnection* con4 = pool.GetConnection(0, nullptr, &ignoreServers);

	BOOST_CHECK(con1 != nullptr);
	BOOST_CHECK(con2 != nullptr);
	BOOST_CHECK(con3 == nullptr);
	BOOST_CHECK(con4 == nullptr);
}

BOOST_AUTO_TEST_CASE(IgnoreServersGroupedTest)
{
	ServerPool pool;
	AddTestServer(&pool, 1, true, 0, false, 1, 2);
	AddTestServer(&pool, 2, true, 0, false, 1, 2);
	pool.InitConnections();

	NewsServer* serv1 = pool.GetServers()->at(0).get();
	ServerPool::RawServerList ignoreServers;
	ignoreServers.push_back(serv1);

	NntpConnection* con1 = pool.GetConnection(0, nullptr, &ignoreServers);
	NntpConnection* con2 = pool.GetConnection(0, nullptr, &ignoreServers);
	NntpConnection* con3 = pool.GetConnection(0, nullptr, &ignoreServers);
	NntpConnection* con4 = pool.GetConnection(0, nullptr, &ignoreServers);
	BOOST_CHECK(con1 == nullptr);
	BOOST_CHECK(con2 == nullptr);
	BOOST_CHECK(con3 == nullptr);
	BOOST_CHECK(con4 == nullptr);

	AddTestServer(&pool, 3, true, 0, false, 2, 2);
	pool.InitConnections();

	con1 = pool.GetConnection(0, nullptr, &ignoreServers);
	con2 = pool.GetConnection(0, nullptr, &ignoreServers);
	con3 = pool.GetConnection(0, nullptr, &ignoreServers);
	BOOST_CHECK(con1 != nullptr);
	BOOST_CHECK(con2 != nullptr);
	BOOST_CHECK(con3 == nullptr);
}

BOOST_AUTO_TEST_CASE(BlockServersUngroupedTest)
{
	TestBlockServers(0);
}

BOOST_AUTO_TEST_CASE(BlockServersGroupedTest)
{
	TestBlockServers(1);
}

BOOST_AUTO_TEST_CASE(BlockOptionalServersUngroupedTest)
{
	TestOptionalBlockServers(0);
}

BOOST_AUTO_TEST_CASE(BlockOptionalServersGrouped)
{
	TestOptionalBlockServers(1);
}

BOOST_AUTO_TEST_CASE(BlockOptionalAndNonOptionalServersUngroupedTest)
{
	TestBlockOptionalAndNonOptionalServers(0);
}

BOOST_AUTO_TEST_CASE(BlockOptionalAndNonOptionalServersGroupedTest)
{
	TestBlockOptionalAndNonOptionalServers(1);
}

BOOST_AUTO_TEST_CASE(SpeedTestServerSelection)
{
	ServerPool pool;
	// Level 1, connections=1, id=1
	AddTestServer(&pool, 1, true, 1, false, 0, 1);
	// Level 2, connections=1, id=2
	AddTestServer(&pool, 2, true, 2, false, 0, 1);
	pool.InitConnections();

	NewsServer* serv1 = pool.GetServers()->at(0).get();

	// Consume the connection for Server 1
	NntpConnection* con1 = pool.GetConnection(0, serv1, nullptr);
	BOOST_CHECK(con1 != nullptr);
	BOOST_CHECK(con1->GetNewsServer() == serv1);

	// Verify that pool.GetConnection(0, serv1, nullptr) returns nullptr (since Server 1 is busy)
	NntpConnection* con2 = pool.GetConnection(0, serv1, nullptr);
	BOOST_CHECK(con2 == nullptr);
}


BOOST_AUTO_TEST_CASE(VerifyNormLevelCorrectness)
{
	ServerPool pool;
	// Create a server with a high level (5)
	AddTestServer(&pool, 1, true, 5, false, 0, 1);
	pool.InitConnections();

	NewsServer* server = pool.GetServers()->at(0).get();
	
	// Check that it normalized to 0
	BOOST_CHECK_EQUAL(server->GetNormLevel(), 0);

	// Check that the connection is found using NormLevel (0)
	NntpConnection* con = pool.GetConnection(server->GetNormLevel(), server, nullptr);
	BOOST_CHECK(con != nullptr);
	
	if (con) pool.FreeConnection(con, false);
}

BOOST_AUTO_TEST_CASE(IdleLevelWaitsForMostRecentlyFreedConnection)
{
	ConnectionIdlePolicy policy;
	policy.ObserveIdle(10);
	BOOST_CHECK(policy.ShouldClose());

	policy.ObserveIdle(0);
	BOOST_CHECK(!policy.ShouldClose());
}

BOOST_AUTO_TEST_CASE(CloseUnusedConnectionsUsesWholeLevelIdleTime)
{
	LoopbackNntpServer server1;
	LoopbackNntpServer server2;
	int port1 = server1.Start();
	int port2 = server2.Start();
	BOOST_REQUIRE(port1 != 0);
	BOOST_REQUIRE(port2 != 0);

	ServerPool pool;
	pool.AddServer(std::make_unique<NewsServer>(1, true, "test1", "127.0.0.1", port1, 4,
		"", "", false, false, nullptr, 1, 0, 0, 0, false, Options::cvNone));
	pool.AddServer(std::make_unique<NewsServer>(2, true, "test2", "127.0.0.1", port2, 4,
		"", "", false, false, nullptr, 1, 0, 0, 0, false, Options::cvNone));
	pool.InitConnections();

	NewsServer* newsServer1 = pool.GetServers()->at(0).get();
	NewsServer* newsServer2 = pool.GetServers()->at(1).get();
	NntpConnection* oldConnection1 = pool.GetConnection(0, newsServer1, nullptr);
	NntpConnection* oldConnection2 = pool.GetConnection(0, newsServer2, nullptr);
	BOOST_REQUIRE(oldConnection1 != nullptr);
	BOOST_REQUIRE(oldConnection2 != nullptr);
	oldConnection1->SetSuppressErrors(true);
	oldConnection2->SetSuppressErrors(true);
	oldConnection1->SetTimeout(5);
	oldConnection2->SetTimeout(5);
	BOOST_REQUIRE(oldConnection1->Connect());
	BOOST_REQUIRE(oldConnection2->Connect());

	// Preserve the initial epoch timestamps to create an all-old level without sleeping.
	pool.FreeConnection(oldConnection1, false);
	pool.FreeConnection(oldConnection2, false);
	pool.CloseUnusedConnections();

	BOOST_REQUIRE(oldConnection1->GetStatus() == Connection::csDisconnected);
	BOOST_REQUIRE(oldConnection2->GetStatus() == Connection::csDisconnected);

	NntpConnection* oldConnection = pool.GetConnection(0, newsServer1, nullptr);
	NntpConnection* recentlyUsed = pool.GetConnection(0, newsServer2, nullptr);
	BOOST_REQUIRE(oldConnection != nullptr);
	BOOST_REQUIRE(recentlyUsed != nullptr);
	oldConnection->SetSuppressErrors(true);
	recentlyUsed->SetSuppressErrors(true);
	oldConnection->SetTimeout(5);
	recentlyUsed->SetTimeout(5);
	BOOST_REQUIRE(oldConnection->Connect());
	BOOST_REQUIRE(recentlyUsed->Connect());

	// Keep one old timestamp and stamp the other connection as freshly returned.
	pool.FreeConnection(oldConnection, false);
	pool.FreeConnection(recentlyUsed, true);
	BOOST_REQUIRE(oldConnection->GetStatus() == Connection::csConnected);
	BOOST_REQUIRE(recentlyUsed->GetStatus() == Connection::csConnected);

	pool.CloseUnusedConnections();

	BOOST_CHECK(oldConnection->GetStatus() == Connection::csConnected);
	BOOST_CHECK(recentlyUsed->GetStatus() == Connection::csConnected);
}

BOOST_AUTO_TEST_CASE(IdleLevelClosesOnlyAfterEveryConnectionPassesHold)
{
	ConnectionIdlePolicy allIdle;
	allIdle.ObserveIdle(7);
	allIdle.ObserveIdle(6);
	BOOST_CHECK(allIdle.ShouldClose());

	ConnectionIdlePolicy boundary;
	boundary.ObserveIdle(5);
	BOOST_CHECK(!boundary.ShouldClose());

	ConnectionIdlePolicy active;
	active.ObserveIdle(10);
	active.ObserveInUse();
	BOOST_CHECK(!active.ShouldClose());

	ConnectionIdlePolicy empty;
	BOOST_CHECK(!empty.ShouldClose());

	ConnectionIdlePolicy largeDuration;
	largeDuration.ObserveIdle(std::numeric_limits<time_t>::max());
	BOOST_CHECK(largeDuration.ShouldClose());
}

BOOST_AUTO_TEST_SUITE_END()
