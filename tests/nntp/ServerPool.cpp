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

#include <boost/test/unit_test.hpp>
#include "ServerPool.h"
#include "Options.h"
#include "Util.h"

BOOST_AUTO_TEST_SUITE(NNTPTest)

void AddTestServer(ServerPool* pool, int id, bool active, int level, bool optional, int group, int connections)
{
	pool->AddServer(std::make_unique<NewsServer>(id, active, nullptr, "", 119, 0,
		"", "", false, false, nullptr, connections, 0, level, group, optional, Options::cvStrict));
}

NntpConnection* WaitForConnection(ServerPool* pool, int level, time_t timeoutSec)
{
	time_t deadline = Util::CurrentTime() + timeoutSec;
	NntpConnection* connection;
	while ((connection = pool->GetConnection(level, nullptr, nullptr)) == nullptr &&
		Util::CurrentTime() < deadline)
	{
		Util::Sleep(10);
	}
	return connection;
}

void TestCooldownOnFailure(int /*group*/)
{
	ServerPool pool;
	AddTestServer(&pool, 1, true, 0, false, 0, 2);
	AddTestServer(&pool, 2, true, 0, false, 0, 2);
	pool.InitConnections();
	pool.SetRetryInterval(60);

	NntpConnection* con1 = pool.GetConnection(0, nullptr, nullptr);
	BOOST_REQUIRE(con1 != nullptr);

	// Free with used=false → sets cooldown
	pool.FreeConnection(con1, false);

	// con1's connection is on cooldown, should NOT be returned
	NntpConnection* con2 = pool.GetConnection(0, nullptr, nullptr);
	BOOST_CHECK(con2 != nullptr);
	BOOST_CHECK(con2 != con1);

	// Free with used=true → resets cooldown and failures
	pool.FreeConnection(con2, true);

	// con2 should now be available for reuse immediately
	NntpConnection* con3 = pool.GetConnection(0, nullptr, nullptr);
	BOOST_CHECK(con3 != nullptr);

	if (con3) pool.FreeConnection(con3, true);
}


void TestBlockDoesNotGateConnections(int /*group*/)
{
	ServerPool pool;
	AddTestServer(&pool, 1, true, 0, false, 0, 2);
	AddTestServer(&pool, 2, true, 1, false, 0, 1);
	pool.InitConnections();
	pool.SetRetryInterval(60);

	NewsServer* serv1 = pool.GetServers()->at(0).get();
	pool.BlockServer(serv1);

	// BlockServer alone does NOT gate connections (only per-connection cooldown does)
	NntpConnection* con1 = pool.GetConnection(0, nullptr, nullptr);
	BOOST_CHECK(con1 != nullptr);
	BOOST_CHECK(con1->GetNewsServer() == serv1);

	if (con1) pool.FreeConnection(con1, true);
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

	pool.FreeConnection(con1, true);
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

BOOST_AUTO_TEST_CASE(CooldownOnFailureTest)
{
	TestCooldownOnFailure(0);
}

BOOST_AUTO_TEST_CASE(CooldownOnFailureGroupedTest)
{
	TestCooldownOnFailure(1);
}

BOOST_AUTO_TEST_CASE(BlockDoesNotGateConnectionsTest)
{
	TestBlockDoesNotGateConnections(0);
}

BOOST_AUTO_TEST_CASE(SpeedTestServerSelection)
{
	ServerPool pool;
	AddTestServer(&pool, 1, true, 1, false, 0, 1);
	AddTestServer(&pool, 2, true, 2, false, 0, 1);
	pool.InitConnections();

	NewsServer* serv1 = pool.GetServers()->at(0).get();

	NntpConnection* con1 = pool.GetConnection(0, serv1, nullptr);
	BOOST_CHECK(con1 != nullptr);
	BOOST_CHECK(con1->GetNewsServer() == serv1);

	// Server 1 connection consumed; asking for it again returns nullptr.
	NntpConnection* con2 = pool.GetConnection(0, serv1, nullptr);
	BOOST_CHECK(con2 == nullptr);
}


BOOST_AUTO_TEST_CASE(VerifyNormLevelCorrectness)
{
	ServerPool pool;
	AddTestServer(&pool, 1, true, 5, false, 0, 1);
	pool.InitConnections();

	NewsServer* server = pool.GetServers()->at(0).get();

	BOOST_CHECK_EQUAL(server->GetNormLevel(), 0);

	NntpConnection* con = pool.GetConnection(server->GetNormLevel(), server, nullptr);
	BOOST_CHECK(con != nullptr);

	if (con) pool.FreeConnection(con, false);
}

BOOST_AUTO_TEST_CASE(ProgressiveBackoffTest)
{
	ServerPool pool;
	AddTestServer(&pool, 1, true, 0, false, 0, 1);
	pool.InitConnections();
	pool.SetRetryInterval(1);

	// First failure puts the connection on a base cooldown (1s)...
	NntpConnection* con = pool.GetConnection(0, nullptr, nullptr);
	BOOST_REQUIRE(con != nullptr);
	pool.FreeConnection(con, false);
	BOOST_CHECK(pool.GetConnection(0, nullptr, nullptr) == nullptr);

	// ...which expires after the base window (1s).
	con = WaitForConnection(&pool, 0, 2);
	BOOST_REQUIRE(con != nullptr);

	// A second consecutive failure escalates the cooldown (to 2s). At 1s the
	// base 1s window would have cleared, but the connection must STILL be
	// blocked; that is what distinguishes escalation from a flat cooldown.
	pool.FreeConnection(con, false);
	BOOST_CHECK(pool.GetConnection(0, nullptr, nullptr) == nullptr);
	con = WaitForConnection(&pool, 0, 1);
	BOOST_CHECK(con == nullptr);

	// It comes back once the escalated (2s) window expires.
	con = WaitForConnection(&pool, 0, 2);
	BOOST_REQUIRE(con != nullptr);
	pool.FreeConnection(con, true);
}

BOOST_AUTO_TEST_CASE(ResetOnSuccessTest)
{
	ServerPool pool;
	AddTestServer(&pool, 1, true, 0, false, 0, 1);
	pool.InitConnections();
	pool.SetRetryInterval(1);

	// Failure → base cooldown (1s).
	NntpConnection* con = pool.GetConnection(0, nullptr, nullptr);
	BOOST_REQUIRE(con != nullptr);
	pool.FreeConnection(con, false);
	con = WaitForConnection(&pool, 0, 2);
	BOOST_REQUIRE(con != nullptr);

	// A successful use resets the failure counter, so the following failure
	// only gets the base cooldown (1s), not an escalated one (2s).
	pool.FreeConnection(con, true);
	con = pool.GetConnection(0, nullptr, nullptr);
	BOOST_REQUIRE(con != nullptr);
	pool.FreeConnection(con, false);
	BOOST_CHECK(pool.GetConnection(0, nullptr, nullptr) == nullptr);

	// Because the counter was reset, the connection returns within the base
	// 1s window (an unreset 2s cooldown would still be blocked at 1s).
	con = WaitForConnection(&pool, 0, 1);
	BOOST_CHECK(con != nullptr);
	if (con) pool.FreeConnection(con, true);
}

BOOST_AUTO_TEST_CASE(OptionalServerEscalationOnCooldownTest)
{
	ServerPool pool;
	// All optional on level 0, non-optional on level 1
	AddTestServer(&pool, 1, true, 0, true, 0, 2);
	AddTestServer(&pool, 2, true, 0, true, 0, 2);
	AddTestServer(&pool, 3, true, 1, false, 0, 2);
	pool.InitConnections();
	pool.SetRetryInterval(5);

	NewsServer* serv1 = pool.GetServers()->at(0).get();
	NewsServer* serv2 = pool.GetServers()->at(1).get();

	for (int pass = 0; pass < 2; pass++)
	{
		for (int i = 0; i < 4; i++)
		{
			NntpConnection *con = pool.GetConnection(0, nullptr, nullptr);
			if (!con) break;
			pool.FreeConnection(con, false);
		}
	}

	// All level-0 servers blocked and on cooldown → escalate to level 1
	pool.BlockServer(serv1);
	pool.BlockServer(serv2);

	NntpConnection* con = pool.GetConnection(0, nullptr, nullptr);
	BOOST_CHECK(con != nullptr);
	if (con)
	{
		BOOST_CHECK_MESSAGE(con->GetNewsServer()->GetLevel() == 1,
			"Got level " << con->GetNewsServer()->GetLevel() << ", expected level 1");
		pool.FreeConnection(con, true);
	}
}

BOOST_AUTO_TEST_CASE(MixedCooldownNoEscalationTest)
{
	ServerPool pool;
	// One optional + one non-optional on level 0, non-optional on level 1
	AddTestServer(&pool, 1, true, 0, true, 0, 2);
	AddTestServer(&pool, 2, true, 0, false, 0, 2);
	AddTestServer(&pool, 3, true, 1, false, 0, 2);
	pool.InitConnections();
	pool.SetRetryInterval(60);

	NewsServer* serv1 = pool.GetServers()->at(0).get();

	// Block the optional server
	pool.BlockServer(serv1);

	NntpConnection* con1 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con2 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con3 = pool.GetConnection(0, nullptr, nullptr);
	NntpConnection* con4 = pool.GetConnection(0, nullptr, nullptr);
	BOOST_REQUIRE(con1);
	BOOST_REQUIRE(con2);
	BOOST_REQUIRE(con3);
	BOOST_REQUIRE(con4);

	// Free one level-0 connection; it returns immediately, no escalation needed.
	pool.FreeConnection(con4, true);
	NntpConnection* con5 = pool.GetConnection(0, nullptr, nullptr);
	BOOST_CHECK(con5 != nullptr);

	if (con1) pool.FreeConnection(con1, true);
	if (con2) pool.FreeConnection(con2, true);
	if (con3) pool.FreeConnection(con3, true);
	if (con5) pool.FreeConnection(con5, true);
}

BOOST_AUTO_TEST_SUITE_END()
