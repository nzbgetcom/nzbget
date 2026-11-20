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

#include <boost/test/unit_test.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <chrono>
#include <memory>
#include <thread>

#include "daemon/async/ExecutorPool.h"
#include "daemon/network/DnsCache.h"
#include "daemon/network/http/HttpClient.h"
#include "daemon/network/TlsContexts.h"
#include "daemon/system/SystemInfo.h"

using namespace network;
using namespace network::http;
using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(SystemTest)

namespace
{

network::Awaitable<void> Serve(tcp::acceptor& acceptor, std::string reply, std::string& lastRequest)
{
	auto socket = co_await acceptor.async_accept(asio::use_awaitable);
	co_await asio::async_read_until(socket, asio::dynamic_buffer(lastRequest), "\r\n\r\n", asio::use_awaitable);
	co_await asio::async_write(socket, asio::buffer(reply), asio::use_awaitable);
	boost::system::error_code ec;
	socket.close(ec);
}

struct Fixture
{
	asio::io_context ctx;
	tcp::acceptor acceptor{ctx, tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0)};
	async::ExecutorPool pool{1};
	std::shared_ptr<network::TlsContexts> tls = std::make_shared<network::TlsContexts>(network::TlsContexts::Config{.certCheck = false});
	std::shared_ptr<network::DnsCache> dns = std::make_shared<network::DnsCache>();
	std::shared_ptr<network::http::HttpClient> client = std::make_shared<network::http::HttpClient>(pool.Next(), tls, dns);
	std::string serviceUrl = "http://127.0.0.1:" + std::to_string(acceptor.local_endpoint().port()) + "/";
	std::string lastRequest;
	asio::executor_work_guard<asio::io_context::executor_type> work{asio::make_work_guard(ctx)};
	std::thread serverThread{[this] { ctx.run(); }};

	~Fixture()
	{
		work.reset();
		ctx.stop();
		serverThread.join();
	}

	System::Network Fetch(std::string reply, const std::string& url)
	{
		asio::co_spawn(ctx, Serve(acceptor, std::move(reply), lastRequest), asio::detached);
		return System::FetchNetworkInfo(*client, url);
	}
};

std::string Reply(const std::string& status, const std::string& body)
{
	return "HTTP/1.1 " + status + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

}

BOOST_FIXTURE_TEST_CASE(NetworkInfo_FetchReturnsServiceBodyAndLocalAddress, Fixture)
{
	const auto result = Fetch(Reply("200 OK", "203.0.113.7\n"), serviceUrl);
	BOOST_CHECK_EQUAL(result.publicIP, "203.0.113.7");
	BOOST_CHECK_EQUAL(result.privateIP, "127.0.0.1");
	BOOST_CHECK(lastRequest.starts_with("GET / HTTP/1.1\r\n"));
}

BOOST_FIXTURE_TEST_CASE(NetworkInfo_FetchNon200ReturnsEmpty, Fixture)
{
	const auto result = Fetch(Reply("404 Not Found", "no"), serviceUrl);
	BOOST_CHECK(result.publicIP.empty());
	BOOST_CHECK(result.privateIP.empty());
}

BOOST_FIXTURE_TEST_CASE(NetworkInfo_FetchAfterPoolStopReturnsEmpty, Fixture)
{
	pool.Stop();

	const auto start = std::chrono::steady_clock::now();
	const auto result = System::FetchNetworkInfo(*client, serviceUrl);
	const auto elapsed = std::chrono::steady_clock::now() - start;

	BOOST_CHECK(result.publicIP.empty());
	BOOST_CHECK(result.privateIP.empty());
	BOOST_CHECK_LT(elapsed, 1s);
}

BOOST_AUTO_TEST_SUITE_END()
