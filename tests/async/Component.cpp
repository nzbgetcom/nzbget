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

#include <string_view>

#include "Async.h"

using namespace std::chrono_literals;

namespace
{

class TestComponent final : public ::async::Component
{
public:
	void Stop() noexcept override { ++stops; }

	void Stop(::async::Clock::time_point deadline) noexcept override
	{
		Stop();
		lastDeadline = deadline;
	}

	int stops = 0;
	::async::Clock::time_point lastDeadline{};
};

class ImmediateOnlyComponent final : public ::async::Component
{
public:
	void Stop() noexcept override { ++stops; }

	int stops = 0;
};

} // namespace

namespace async
{
template<>
struct ComponentTraits<TestComponent>
{
	static constexpr ComponentId Id = ComponentId::Network;
	static constexpr std::string_view Name = "test";
};
} // namespace async

BOOST_AUTO_TEST_SUITE(AsyncTest)

BOOST_AUTO_TEST_CASE(ComponentTraits_MapTypeToIdAndName)
{
	static_assert(::async::ComponentType<TestComponent>);
	static_assert(!::async::ComponentType<ImmediateOnlyComponent>);
	static_assert(!::async::ComponentType<int>);
	BOOST_CHECK(::async::ComponentTraits<TestComponent>::Id == ::async::ComponentId::Network);
	BOOST_CHECK_EQUAL(std::string(::async::ComponentTraits<TestComponent>::Name), "test");
	BOOST_CHECK(::async::ComponentId::Network < ::async::ComponentId::Count);
}

BOOST_AUTO_TEST_CASE(Stop_DeadlineOverloadIsForwardedToTheComponent)
{
	TestComponent component;
	::async::Component& base = component;
	const auto deadline = ::async::Clock::now() + 5s;

	base.Stop(deadline);

	BOOST_CHECK_EQUAL(component.stops, 1);
	BOOST_CHECK(component.lastDeadline == deadline);
}

BOOST_AUTO_TEST_CASE(Stop_DeadlineOverloadFallsBackToImmediateStop)
{
	ImmediateOnlyComponent component;
	::async::Component& base = component;

	base.Stop(::async::Clock::now() + 5s);

	BOOST_CHECK_EQUAL(component.stops, 1);
}

BOOST_AUTO_TEST_SUITE_END()
