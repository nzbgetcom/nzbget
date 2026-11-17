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

#include "Async.h"

using namespace async;

BOOST_AUTO_TEST_SUITE(AsyncTest)

BOOST_AUTO_TEST_CASE(ExecutorPool_ZeroThreads_Throw)
{
	BOOST_CHECK_THROW(ExecutorPool{0}, std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(ExecutorPool_NextRotatesShards)
{
	ExecutorPool pool{3};
	auto* a = &asio::query(pool.Next(), asio::execution::context);
	auto* b = &asio::query(pool.Next(), asio::execution::context);
	auto* c = &asio::query(pool.Next(), asio::execution::context);
	BOOST_CHECK(a != b);
	BOOST_CHECK(b != c);
	BOOST_CHECK(a != c);
	BOOST_CHECK(&asio::query(pool.Next(), asio::execution::context) == a);
}

BOOST_AUTO_TEST_CASE(ExecutorPool_PostedWorkRuns)
{
	ExecutorPool pool{2};
	std::promise<void> done;
	auto fut = done.get_future();

	asio::post(pool.Next(), [&done] { done.set_value(); });

	BOOST_REQUIRE(fut.wait_for(std::chrono::seconds{2}) == std::future_status::ready);
}

BOOST_AUTO_TEST_CASE(ExecutorPool_StopIsIdempotentAndJoins)
{
	ExecutorPool pool{2};
	pool.Stop();
	BOOST_CHECK_NO_THROW(pool.Stop());
}

BOOST_AUTO_TEST_SUITE_END()