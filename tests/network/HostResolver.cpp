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

#include <thread>

#include "daemon/network/HostResolver.h"

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

tcp::endpoint Ep(const char* address, unsigned short port = 563)
{
	return tcp::endpoint(asio::ip::make_address(address), port);
}

}

BOOST_AUTO_TEST_CASE(HostResolver_LiteralIPv4NeedsNoDns)
{
	asio::io_context ctx;
	auto result = RunBlocking(ctx, ResolveHost(ctx.get_executor(), "127.0.0.1", 119, IPVersion::Auto, 5s));
	BOOST_REQUIRE(result.has_value());
	BOOST_REQUIRE_EQUAL(result.value().size(), 1u);
	BOOST_CHECK(result.value().front() == Ep("127.0.0.1", 119));
}

BOOST_AUTO_TEST_CASE(HostResolver_LiteralIPv6)
{
	asio::io_context ctx;
	auto result = RunBlocking(ctx, ResolveHost(ctx.get_executor(), "::1", 119, IPVersion::Auto, 5s));
	BOOST_REQUIRE(result.has_value());
	BOOST_REQUIRE_EQUAL(result.value().size(), 1u);
	BOOST_CHECK(result.value().front() == Ep("::1", 119));
}

BOOST_AUTO_TEST_CASE(HostResolver_LiteralOfOtherFamilyFails)
{
	asio::io_context ctx;
	auto result = RunBlocking(ctx, ResolveHost(ctx.get_executor(), "127.0.0.1", 119, IPVersion::V6, 5s));
	BOOST_REQUIRE(result.has_error());
	BOOST_CHECK(result.error() == Error::ResolveFailed);
}

BOOST_AUTO_TEST_CASE(HostResolver_OrderEndpoints_AutoInterleavesIPv6First)
{
	Endpoints resolved{Ep("10.0.0.1"), Ep("10.0.0.2"), Ep("::1")};

	auto ordered = OrderEndpoints(resolved, IPVersion::Auto);
	BOOST_CHECK((ordered == Endpoints{Ep("::1"), Ep("10.0.0.1"), Ep("10.0.0.2")}));
}

BOOST_AUTO_TEST_CASE(HostResolver_OrderEndpoints_SingleFamilyKeepsOnlyThatFamily)
{
	Endpoints resolved{Ep("10.0.0.1"), Ep("::1"), Ep("10.0.0.2")};

	BOOST_CHECK((OrderEndpoints(resolved, IPVersion::V4) == Endpoints{Ep("10.0.0.1"), Ep("10.0.0.2")}));
	BOOST_CHECK((OrderEndpoints(resolved, IPVersion::V6) == Endpoints{Ep("::1")}));
}

BOOST_AUTO_TEST_SUITE_END()
