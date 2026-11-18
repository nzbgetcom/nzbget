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
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_future.hpp>

#include <array>
#include <chrono>
#include <thread>

#include "daemon/network/DnsCache.h"
#include "daemon/network/http/HttpClient.h"
#include "daemon/network/TlsContexts.h"
#include "daemon/async/ExecutorPool.h"

using namespace network;
using namespace network::http;
using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(NetworkTest)

namespace
{

template <typename Awaitable>
auto RunBlocking(asio::io_context& ctx, Awaitable&& awaitable)
{
	ctx.restart();
	auto fut = asio::co_spawn(ctx, std::forward<Awaitable>(awaitable), asio::use_future);
	std::thread stopper{[&ctx, &fut] { fut.wait(); ctx.stop(); }};
	ctx.run();
	stopper.join();
	return fut.get();
}

/// Serves one scripted raw reply per accepted connection, in order, then closes it.
Awaitable<void> Serve(tcp::acceptor& acceptor, std::vector<std::string> replies, std::string& lastRequest)
{
	for (const auto& reply : replies)
	{
		auto socket = co_await acceptor.async_accept(asio::use_awaitable);
		std::string request;
		co_await asio::async_read_until(socket, asio::dynamic_buffer(request), "\r\n\r\n", asio::use_awaitable);
		lastRequest = request;
		co_await asio::async_write(socket, asio::buffer(reply), asio::use_awaitable);
		socket.close();
	}
}

/// Accepts one connection and says nothing until the peer hangs up.
Awaitable<void> Stall(tcp::acceptor& acceptor)
{
	auto socket = co_await acceptor.async_accept(asio::use_awaitable);
	std::array<char, 1024> sink;
	boost::system::error_code ec;
	while (!ec)
	{
		co_await socket.async_read_some(asio::buffer(sink), asio::redirect_error(asio::use_awaitable, ec));
	}
}

struct Fixture
{
	asio::io_context ctx;
	tcp::acceptor acceptor{ctx, tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0)};
	std::shared_ptr<TlsContexts> tls = std::make_shared<TlsContexts>(TlsContexts::Config{.certCheck = false});
	std::shared_ptr<DnsCache> dns = std::make_shared<DnsCache>();
	std::string lastRequest;

	std::string Url(const std::string& path = "/") const
	{
		return "http://127.0.0.1:" + std::to_string(acceptor.local_endpoint().port()) + path;
	}

	Result<HttpResponse> Get(std::vector<std::string> replies, const std::string& path = "/",
		std::chrono::milliseconds timeout = 5s)
	{
		auto client = std::make_shared<HttpClient>(ctx.get_executor(), tls, dns);
		asio::co_spawn(ctx, Serve(acceptor, std::move(replies), lastRequest), asio::detached);
		return RunBlocking(ctx, client->Get(Url(path), timeout));
	}
};

std::string Reply(const std::string& status, const std::string& headers, const std::string& body)
{
	return "HTTP/1.1 " + status + "\r\n" + headers + "\r\n" + body;
}

bool HasHeader(const HttpResponse& response, const std::string& name)
{
	return std::ranges::any_of(response.headers, [&](const auto& h) { return h.first == name; });
}

}

BOOST_FIXTURE_TEST_CASE(HttpClient_GetReturnsBodyFromLocalServer, Fixture)
{
	auto r = Get({Reply("200 OK", "Content-Length: 5\r\nX-Test: yes\r\n", "hello")}, "/path?q=1");
	BOOST_REQUIRE(r.has_value());
	BOOST_CHECK_EQUAL(r->status, 200u);
	BOOST_CHECK_EQUAL(r->body, "hello");
	BOOST_CHECK_EQUAL(r->localAddress, "127.0.0.1");
	BOOST_CHECK(lastRequest.starts_with("GET /path?q=1 HTTP/1.1\r\n"));
	BOOST_CHECK(lastRequest.find("Host: 127.0.0.1:") != std::string::npos);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_ChunkedBodyDecodes, Fixture)
{
	auto r = Get({Reply("200 OK", "Transfer-Encoding: chunked\r\n", "3\r\nabc\r\n2\r\nde\r\n0\r\n\r\n")});
	BOOST_REQUIRE(r.has_value());
	BOOST_CHECK_EQUAL(r->body, "abcde");
	BOOST_CHECK(!HasHeader(*r, "Transfer-Encoding"));
}

BOOST_FIXTURE_TEST_CASE(HttpClient_ChunkedTrailerLineOverLimitFails, Fixture)
{
	auto r = Get({Reply("200 OK", "Transfer-Encoding: chunked\r\n", "0\r\nX: " + std::string(9000, 'a') + "\r\n\r\n")});
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::ProtocolViolation);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_BodyDelimitedByClose, Fixture)
{
	auto r = Get({Reply("200 OK", "", "until close")});
	BOOST_REQUIRE(r.has_value());
	BOOST_CHECK_EQUAL(r->body, "until close");
	BOOST_CHECK_EQUAL(r->localAddress, "127.0.0.1");
}

BOOST_FIXTURE_TEST_CASE(HttpClient_NotFoundIsAResponseNotAnError, Fixture)
{
	auto r = Get({Reply("404 Not Found", "Content-Length: 0\r\n", "")});
	BOOST_REQUIRE(r.has_value());
	BOOST_CHECK_EQUAL(r->status, 404u);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_FollowsRelativeRedirect, Fixture)
{
	auto r = Get({Reply("302 Found", "Location: /final\r\nContent-Length: 0\r\n", ""),
		Reply("200 OK", "Content-Length: 2\r\n", "ok")});
	BOOST_REQUIRE(r.has_value());
	BOOST_CHECK_EQUAL(r->body, "ok");
	BOOST_CHECK_EQUAL(r->url.target, "/final");
}

BOOST_FIXTURE_TEST_CASE(HttpClient_RedirectLoopCapped, Fixture)
{
	const auto loop = Reply("302 Found", "Location: /again\r\nContent-Length: 0\r\n", "");
	auto r = Get({loop, loop, loop, loop, loop, loop}, "/");
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::HttpUnexpectedResponse);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_RedirectWithoutLocationFails, Fixture)
{
	auto r = Get({Reply("301 Moved", "Content-Length: 0\r\n", "")});
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::HttpUnexpectedResponse);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_BodyOverLimitFails, Fixture)
{
	auto r = Get({Reply("200 OK", "Content-Length: 67108865\r\n", "")});
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::MessageTooLarge);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_TruncatedBodyFails, Fixture)
{
	auto r = Get({Reply("200 OK", "Content-Length: 10\r\n", "short")});
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::ConnectionLost);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_GarbageResponseFails, Fixture)
{
	auto r = Get({"not http at all\r\n\r\n"});
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::ProtocolViolation);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_ChunkedWithExtensionAndTrailers, Fixture)
{
	auto r = Get({Reply("200 OK", "Transfer-Encoding: chunked\r\n", "3;ext=1\r\nabc\r\n0\r\nX-Trailer: 1\r\n\r\n")});
	BOOST_REQUIRE(r.has_value());
	BOOST_CHECK_EQUAL(r->body, "abc");
}

BOOST_FIXTURE_TEST_CASE(HttpClient_ChunkedTrailersTooLargeFails, Fixture)
{
	std::string trailers;
	for (int i = 0; i < 12000; ++i)
	{
		trailers += "X: y\r\n"; // 72 KB, above the 64 KB header block limit
	}
	auto r = Get({Reply("200 OK", "Transfer-Encoding: chunked\r\n", "0\r\n" + trailers + "\r\n")});
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::MessageTooLarge);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_SwitchingProtocolsIsFinal, Fixture)
{
	auto r = Get({Reply("101 Switching Protocols", "Upgrade: websocket\r\nConnection: Upgrade\r\n", "")});
	BOOST_REQUIRE(r.has_value());
	BOOST_CHECK_EQUAL(r->status, 101u);
	BOOST_CHECK(r->body.empty());
}

BOOST_FIXTURE_TEST_CASE(HttpClient_WhitespaceBeforeColonFails, Fixture)
{
	auto r = Get({Reply("200 OK", "X-Bad : 1\r\nContent-Length: 0\r\n", "")});
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::ProtocolViolation);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_NonTokenHeaderNameFails, Fixture)
{
	auto r = Get({Reply("200 OK", "Bad Name: 1\r\nContent-Length: 0\r\n", "")});
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::ProtocolViolation);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_ControlCharInHeaderValueFails, Fixture)
{
	auto r = Get({Reply("200 OK", "X-Test: a\x01" "b\r\nContent-Length: 0\r\n", "")});
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::ProtocolViolation);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_HeaderAccessorIsCaseInsensitiveAndKeepsTab, Fixture)
{
	auto r = Get({Reply("200 OK", "X-Test: a\tb\r\nContent-Length: 0\r\n", "")});
	BOOST_REQUIRE(r.has_value());
	BOOST_REQUIRE(r->Header("x-test") != nullptr);
	BOOST_CHECK_EQUAL(*r->Header("X-TEST"), "a\tb");
	BOOST_CHECK(r->Header("X-Missing") == nullptr);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_BadChunkSizeFails, Fixture)
{
	auto r = Get({Reply("200 OK", "Transfer-Encoding: chunked\r\n", "zz\r\nabc\r\n0\r\n\r\n")});
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::ProtocolViolation);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_NoContentHasNoBody, Fixture)
{
	auto r = Get({Reply("204 No Content", "", "")});
	BOOST_REQUIRE(r.has_value());
	BOOST_CHECK_EQUAL(r->status, 204u);
	BOOST_CHECK(r->body.empty());
}

BOOST_FIXTURE_TEST_CASE(HttpClient_SkipsInterimResponse, Fixture)
{
	auto r = Get({"HTTP/1.1 100 Continue\r\n\r\n" + Reply("200 OK", "Content-Length: 2\r\n", "ok")});
	BOOST_REQUIRE(r.has_value());
	BOOST_CHECK_EQUAL(r->status, 200u);
	BOOST_CHECK_EQUAL(r->body, "ok");
}

BOOST_FIXTURE_TEST_CASE(HttpClient_HeaderNamesAreCaseInsensitive, Fixture)
{
	auto r = Get({Reply("302 Found", "lOcAtIoN: /final\r\ncontent-length: 0\r\n", ""),
		Reply("200 OK", "Content-Length: 2\r\n", "ok")});
	BOOST_REQUIRE(r.has_value());
	BOOST_CHECK_EQUAL(r->body, "ok");
}

BOOST_FIXTURE_TEST_CASE(HttpClient_ContentLengthWithTransferEncodingFails, Fixture)
{
	auto r = Get({Reply("200 OK", "Content-Length: 5\r\nTransfer-Encoding: chunked\r\n", "0\r\n\r\n")});
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::ProtocolViolation);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_ConflictingContentLengthRejected, Fixture)
{
	auto r = Get({Reply("200 OK", "Content-Length: 2\r\nContent-Length: 3\r\n", "abc")});
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::ProtocolViolation);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_RepeatedEqualContentLengthIsAccepted, Fixture)
{
	auto r = Get({Reply("200 OK", "Content-Length: 2\r\nContent-Length: 2\r\n", "ok")});
	BOOST_REQUIRE(r.has_value());
	BOOST_CHECK_EQUAL(r->body, "ok");
}

BOOST_FIXTURE_TEST_CASE(HttpClient_OversizedHeaderRejected, Fixture)
{
	auto r = Get({"HTTP/1.1 200 OK\r\nX: " + std::string(70000, 'a')});
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::MessageTooLarge);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_UnknownContentEncodingFails, Fixture)
{
	auto r = Get({Reply("200 OK", "Content-Encoding: br\r\nContent-Length: 2\r\n", "ok")});
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::HttpUnexpectedResponse);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_SlowServerTimesOutWithinDeadline, Fixture)
{
	auto client = std::make_shared<HttpClient>(ctx.get_executor(), tls, dns);
	asio::co_spawn(ctx, Stall(acceptor), asio::detached);

	const auto start = std::chrono::steady_clock::now();
	auto r = RunBlocking(ctx, client->Get(Url(), 300ms));
	const auto elapsed = std::chrono::steady_clock::now() - start;
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::Timeout);
	BOOST_CHECK_LE(elapsed, 400ms);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_SyncGetReturnsBody, Fixture)
{
	async::ExecutorPool pool{1};
	auto client = std::make_shared<HttpClient>(pool.Next(), tls, dns);
	asio::co_spawn(ctx, Serve(acceptor, {Reply("200 OK", "Content-Length: 2\r\n", "ok")}, lastRequest),
		asio::detached);

	std::thread serverThread{[this] { ctx.run(); }};
	auto r = client->SyncGet(Url(), 5s);
	ctx.stop();
	serverThread.join();

	BOOST_REQUIRE(r.has_value());
	BOOST_CHECK_EQUAL(r->body, "ok");
}

BOOST_FIXTURE_TEST_CASE(HttpClient_SyncGetIsCanceledPromptly, Fixture)
{
	async::ExecutorPool pool{1};
	auto client = std::make_shared<HttpClient>(pool.Next(), tls, dns);
	asio::co_spawn(ctx, Stall(acceptor), asio::detached);

	std::thread serverThread{[this] { ctx.run(); }};
	Result<HttpResponse> result = Error::LocalFailure;
	std::thread fetchThread{[&] { result = client->SyncGet(Url(), 30s); }};

	std::this_thread::sleep_for(200ms);
	const auto start = std::chrono::steady_clock::now();
	client->CancelAll();
	fetchThread.join();
	const auto elapsed = std::chrono::steady_clock::now() - start;
	ctx.stop();
	serverThread.join();

	BOOST_REQUIRE(result.has_error());
	BOOST_CHECK(result.error() == Error::Canceled);
	// Generous: thread sanitizer and loaded hosts stretch scheduling; a missed
	// cancellation would instead sit on the 30 s request timeout.
	BOOST_CHECK_LT(elapsed, 5s);
}

BOOST_FIXTURE_TEST_CASE(HttpClient_RejectsBadUrlWithoutNetwork, Fixture)
{
	auto client = std::make_shared<HttpClient>(ctx.get_executor(), tls, dns);
	auto r = RunBlocking(ctx, client->Get("ftp://x/", 1s));
	BOOST_REQUIRE(r.has_error());
}

BOOST_AUTO_TEST_SUITE_END()
