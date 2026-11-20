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
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <map>
#include <thread>

#include "daemon/network/Dial.h"

using namespace network;
using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(NetworkTest)

namespace
{

template <typename Awaitable>
auto RunBlocking(asio::io_context& ctx, Awaitable&& awaitable)
{
	auto fut = asio::co_spawn(ctx, std::forward<Awaitable>(awaitable), asio::use_future);
	std::thread stopper{[&ctx, &fut] { fut.wait(); ctx.stop(); }};
	ctx.run();
	stopper.join();
	return fut.get();
}

Options TestOptions()
{
	Options options;
	// Windows retries a refused loopback SYN for ~2 s before reporting it; keep headroom.
	options.connectTimeout = 5s;
	return options;
}

/// A loopback port nobody listens on: connecting is refused immediately.
tcp::endpoint ClosedEndpoint(asio::io_context& ctx)
{
	tcp::acceptor acceptor(ctx, tcp::endpoint(tcp::v4(), 0));
	auto endpoint = acceptor.local_endpoint();
	acceptor.close();
	return tcp::endpoint(asio::ip::make_address("127.0.0.1"), endpoint.port());
}

struct FakeEndpoint
{
	std::chrono::milliseconds latency{0};
	Error error{Error::None};
};

struct FakeLog
{
	std::vector<uint16_t> started;
	std::vector<uint16_t> succeeded;
};

tcp::endpoint Fake(uint16_t port)
{
	return tcp::endpoint(asio::ip::make_address("127.0.0.1"), port);
}

/// Simulated connect: takes @p latency, then fails with the configured error or succeeds.
Awaitable<Result<std::shared_ptr<Connection>>> FakeConnect(Executor executor, tcp::endpoint endpoint,
	const std::map<uint16_t, FakeEndpoint>& behavior, FakeLog& log)
{
	log.started.push_back(endpoint.port());
	const auto& fake = behavior.at(endpoint.port());

	SteadyTimer timer(executor);
	timer.expires_after(fake.latency);
	auto [ec] = co_await timer.async_wait(asio::as_tuple(asio::use_awaitable));
	if (ec)
	{
		co_return Error::Canceled;
	}
	if (fake.error != Error::None)
	{
		co_return fake.error;
	}

	log.succeeded.push_back(endpoint.port());
	co_return std::make_shared<Connection>(executor, Options{}, nullptr);
}

Result<std::shared_ptr<Connection>> Race(asio::io_context& ctx, Endpoints endpoints,
	const std::map<uint16_t, FakeEndpoint>& behavior, FakeLog& log)
{
	return RunBlocking(ctx, detail::DialUsing(ctx.get_executor(), std::move(endpoints),
		[&behavior, &log](Executor executor, tcp::endpoint endpoint)
		{
			return FakeConnect(executor, endpoint, behavior, log);
		}));
}

}

BOOST_AUTO_TEST_CASE(Dial_ConnectsToReachableEndpoint)
{
	asio::io_context ctx;
	tcp::acceptor acceptor(ctx, tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));

	auto result = RunBlocking(ctx, Dial(ctx.get_executor(), {acceptor.local_endpoint()}, TestOptions(), nullptr));
	BOOST_REQUIRE(result.has_value());
	BOOST_CHECK(result.value()->IsConnected());
}

BOOST_AUTO_TEST_CASE(Dial_SkipsRefusedEndpoint)
{
	asio::io_context ctx;
	tcp::acceptor acceptor(ctx, tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));

	auto result = RunBlocking(ctx, Dial(ctx.get_executor(), {ClosedEndpoint(ctx), acceptor.local_endpoint()}, TestOptions(), nullptr));
	BOOST_REQUIRE(result.has_value());
	BOOST_CHECK(result.value()->IsConnected());
}

BOOST_AUTO_TEST_CASE(Dial_AllEndpointsFail_ReturnsConnectFailed)
{
	asio::io_context ctx;

	auto result = RunBlocking(ctx, Dial(ctx.get_executor(), {ClosedEndpoint(ctx)}, TestOptions(), nullptr));
	BOOST_REQUIRE(result.has_error());
	BOOST_CHECK_MESSAGE(result.error() == Error::ConnectFailed,
		"unexpected error: " << network::ToString(result.error())
			<< " (" << static_cast<int>(result.error()) << ")");
}

BOOST_AUTO_TEST_CASE(Dial_NoEndpoints_ReturnsConnectFailed)
{
	asio::io_context ctx;

	auto result = RunBlocking(ctx, Dial(ctx.get_executor(), {}, TestOptions(), nullptr));
	BOOST_REQUIRE(result.has_error());
	BOOST_CHECK(result.error() == Error::ConnectFailed);
}

BOOST_AUTO_TEST_CASE(Dial_SlowFirstEndpoint_SecondWinsAndFirstIsCancelled)
{
	asio::io_context ctx;
	std::map<uint16_t, FakeEndpoint> behavior{{1001, {2000ms, Error::None}}, {1002, {0ms, Error::None}}};
	FakeLog log;

	auto started = std::chrono::steady_clock::now();
	auto result = Race(ctx, {Fake(1001), Fake(1002)}, behavior, log);
	auto elapsed = std::chrono::steady_clock::now() - started;

	BOOST_REQUIRE(result.has_value());
	BOOST_CHECK(log.succeeded == std::vector<uint16_t>{1002});
	BOOST_CHECK_LT(elapsed, 1000ms);
}

BOOST_AUTO_TEST_CASE(Dial_FastFirstEndpoint_SecondIsNeverStarted)
{
	asio::io_context ctx;
	std::map<uint16_t, FakeEndpoint> behavior{{1001, {0ms, Error::None}}, {1002, {0ms, Error::None}}};
	FakeLog log;

	auto result = Race(ctx, {Fake(1001), Fake(1002)}, behavior, log);

	BOOST_REQUIRE(result.has_value());
	BOOST_CHECK(log.started == std::vector<uint16_t>{1001});
}

BOOST_AUTO_TEST_CASE(Dial_FailedFirstEndpoint_SecondIsUsed)
{
	asio::io_context ctx;
	std::map<uint16_t, FakeEndpoint> behavior{{1001, {0ms, Error::ConnectFailed}}, {1002, {0ms, Error::None}}};
	FakeLog log;

	auto result = Race(ctx, {Fake(1001), Fake(1002)}, behavior, log);

	BOOST_REQUIRE(result.has_value());
	BOOST_CHECK(log.succeeded == std::vector<uint16_t>{1002});
}

BOOST_AUTO_TEST_CASE(Dial_TlsFailureIsReportedWhenNothingConnects)
{
	asio::io_context ctx;
	std::map<uint16_t, FakeEndpoint> behavior{{1001, {0ms, Error::TlsVerifyFailed}}, {1002, {0ms, Error::ConnectFailed}}};
	FakeLog log;

	auto result = Race(ctx, {Fake(1001), Fake(1002)}, behavior, log);

	BOOST_REQUIRE(result.has_error());
	BOOST_CHECK(result.error() == Error::TlsVerifyFailed);
}

BOOST_AUTO_TEST_CASE(Dial_AllFakeEndpointsFail_ReturnsConnectFailed)
{
	asio::io_context ctx;
	std::map<uint16_t, FakeEndpoint> behavior{{1001, {0ms, Error::ConnectFailed}}, {1002, {0ms, Error::ConnectFailed}}};
	FakeLog log;

	auto result = Race(ctx, {Fake(1001), Fake(1002)}, behavior, log);

	BOOST_REQUIRE(result.has_error());
	BOOST_CHECK(result.error() == Error::ConnectFailed);
}

BOOST_AUTO_TEST_SUITE_END()
