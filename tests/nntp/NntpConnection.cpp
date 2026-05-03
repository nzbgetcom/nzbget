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
#include "NntpConnection.h"
#include "Connection.h"
#include "NewsServer.h"

#include <boost/test/unit_test.hpp>
#include <future>
#include <thread>
#include <chrono>
#include <cstring>

#ifndef WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

static int FindFreeTcpPort()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        return 0;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0)
    {
        closesocket(sock);
        return 0;
    }

    socklen_t len = sizeof(addr);
    if (getsockname(sock, (struct sockaddr*)&addr, &len) != 0)
    {
        closesocket(sock);
        return 0;
    }

    int port = ntohs(addr.sin_port);
    closesocket(sock);
    return port;
}

static void RunPipeliningServer(int port, std::promise<bool>& started)
{
    Connection listener("127.0.0.1", port, false);
    listener.SetIPVersion(Connection::ipV4);
    if (!listener.Bind())
    {
        started.set_value(false);
        return;
    }

    started.set_value(true);
    std::unique_ptr<Connection> clientConn = listener.Accept();
    if (!clientConn)
    {
        return;
    }

    clientConn->WriteLine("200 NZBGet test server ready\r\n");

    char line[256];
    int bytesRead = 0;

    if (!clientConn->ReadLine(line, sizeof(line), &bytesRead))
    {
        return;
    }
    BOOST_CHECK_EQUAL(std::string(line), "BODY <msg1>\r\n");
    clientConn->WriteLine("222 Article 1 retrieved\r\n");

    if (!clientConn->ReadLine(line, sizeof(line), &bytesRead))
    {
        return;
    }
    BOOST_CHECK_EQUAL(std::string(line), "BODY <msg2>\r\n");
    clientConn->WriteLine("222 Article 2 retrieved\r\n");
}

BOOST_AUTO_TEST_SUITE(NNTPTest)

BOOST_AUTO_TEST_CASE(NntpConnectionPipeliningTest)
{
    Connection::Init();

    int port = FindFreeTcpPort();
    BOOST_REQUIRE(port > 0);

    std::promise<bool> started;
    std::future<bool> startedFuture = started.get_future();
    std::thread serverThread(RunPipeliningServer, port, std::ref(started));

    BOOST_REQUIRE(startedFuture.get());

    NewsServer newsServer(1, true, "test", "127.0.0.1", port, 4,
        "", "", false, false, "", 1, 2, 0, 0, 0, false, 0);

    NntpConnection client(&newsServer);
    client.SetIPVersion(Connection::ipV4);
    BOOST_REQUIRE(client.Connect());

    const char* firstResponse = client.Request("BODY <msg1>\r\n");
    BOOST_REQUIRE(firstResponse);
    BOOST_CHECK_EQUAL(strncmp(firstResponse, "222", 3), 0);

    BOOST_CHECK(client.SendRequest("BODY <msg2>\r\n"));

    const char* secondResponse = client.ReadResponseLine("BODY <msg2>\r\n");
    BOOST_REQUIRE(secondResponse);
    BOOST_CHECK_EQUAL(strncmp(secondResponse, "222", 3), 0);

    client.Disconnect();
    if (serverThread.joinable())
    {
        serverThread.join();
    }

    Connection::Final();
}

BOOST_AUTO_TEST_SUITE_END()
