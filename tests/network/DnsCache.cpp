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

#include "daemon/network/DnsCache.h"

using namespace network;

namespace
{

using Clock = DnsCache::Clock;

Endpoints OneEndpoint(uint16_t port)
{
	return { tcp::endpoint{boost::asio::ip::make_address_v4("127.0.0.1"), port} };
}

}

BOOST_AUTO_TEST_SUITE(NetworkTest)

BOOST_AUTO_TEST_CASE(DnsCache_HitWithinTtl)
{
	DnsCache cache;
	auto now = Clock::now();
	cache.Store("news.example", 119, IPVersion::Auto, OneEndpoint(119), now);
	auto found = cache.Find("news.example", 119, IPVersion::Auto, now + std::chrono::seconds{59});
	BOOST_REQUIRE(found.has_value());
	BOOST_CHECK_EQUAL(found->size(), 1u);
}

BOOST_AUTO_TEST_CASE(DnsCache_PositiveEntryExpiresAfterTtl)
{
	DnsCache cache;
	auto now = Clock::now();
	cache.Store("news.example", 119, IPVersion::Auto, OneEndpoint(119), now);
	BOOST_CHECK(!cache.Find("news.example", 119, IPVersion::Auto, now + DnsCache::POSITIVE_TTL));
}

BOOST_AUTO_TEST_CASE(DnsCache_NegativeEntryExpiresAfterFiveSeconds)
{
	DnsCache cache;
	auto now = Clock::now();
	cache.Store("missing.example", 119, IPVersion::Auto, {}, now);
	auto found = cache.Find("missing.example", 119, IPVersion::Auto, now + std::chrono::seconds{4});
	BOOST_REQUIRE(found.has_value());
	BOOST_CHECK(found->empty());
	BOOST_CHECK(!cache.Find("missing.example", 119, IPVersion::Auto, now + DnsCache::NEGATIVE_TTL));
}

BOOST_AUTO_TEST_CASE(DnsCache_KeyIncludesPortAndVersion)
{
	DnsCache cache;
	auto now = Clock::now();
	cache.Store("news.example", 119, IPVersion::Auto, OneEndpoint(119), now);
	BOOST_CHECK(!cache.Find("news.example", 563, IPVersion::Auto, now));
	BOOST_CHECK(!cache.Find("news.example", 119, IPVersion::V4, now));
	BOOST_CHECK(!cache.Find("other.example", 119, IPVersion::Auto, now));
}

BOOST_AUTO_TEST_CASE(DnsCache_CapsAtMaxEntries)
{
	DnsCache cache;
	auto now = Clock::now();
	for (size_t i = 0; i <= DnsCache::MAX_ENTRIES; ++i)
	{
		cache.Store("host" + std::to_string(i) + ".example", 119, IPVersion::Auto, OneEndpoint(119), now);
	}
	BOOST_CHECK(!cache.Find("host0.example", 119, IPVersion::Auto, now));
	BOOST_CHECK(cache.Find("host" + std::to_string(DnsCache::MAX_ENTRIES) + ".example", 119, IPVersion::Auto, now));
}

BOOST_AUTO_TEST_CASE(DnsCache_InvalidateRemoves)
{
	DnsCache cache;
	auto now = Clock::now();
	cache.Store("news.example", 119, IPVersion::Auto, OneEndpoint(119), now);
	cache.Invalidate("news.example", 119, IPVersion::Auto);
	BOOST_CHECK(!cache.Find("news.example", 119, IPVersion::Auto, now));
}

BOOST_AUTO_TEST_SUITE_END()
