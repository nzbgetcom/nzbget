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

#include "daemon/network/Url.h"

using namespace network;

BOOST_AUTO_TEST_SUITE(NetworkTest)

BOOST_AUTO_TEST_CASE(UrlParsesHttpsWithDefaults)
{
	auto r = ParseUrl("https://Example.COM/a/b?x=1#frag");
	BOOST_REQUIRE(r);
	BOOST_CHECK(r->tls);
	BOOST_CHECK_EQUAL(r->host, "example.com");
	BOOST_CHECK_EQUAL(r->port, 443);
	BOOST_CHECK_EQUAL(r->target, "/a/b?x=1");
	BOOST_CHECK_EQUAL(r->HostHeader(), "example.com");
}

BOOST_AUTO_TEST_CASE(UrlParsesHttpExplicitPort)
{
	auto r = ParseUrl("HTTP://host:8080");
	BOOST_REQUIRE(r);
	BOOST_CHECK(!r->tls);
	BOOST_CHECK_EQUAL(r->port, 8080);
	BOOST_CHECK_EQUAL(r->target, "/");
	BOOST_CHECK_EQUAL(r->HostHeader(), "host:8080");
}

BOOST_AUTO_TEST_CASE(UrlQueryWithoutPathGetsSlash)
{
	auto r = ParseUrl("http://host?a=b");
	BOOST_REQUIRE(r);
	BOOST_CHECK_EQUAL(r->target, "/?a=b");
}

BOOST_AUTO_TEST_CASE(UrlParsesIpv6Literal)
{
	auto r = ParseUrl("http://[::1]:81/x");
	BOOST_REQUIRE(r);
	BOOST_CHECK_EQUAL(r->host, "::1");
	BOOST_CHECK_EQUAL(r->port, 81);
	BOOST_CHECK_EQUAL(r->HostHeader(), "[::1]:81");

	auto d = ParseUrl("https://[2001:db8::1]/");
	BOOST_REQUIRE(d);
	BOOST_CHECK_EQUAL(d->HostHeader(), "[2001:db8::1]");
}

BOOST_AUTO_TEST_CASE(UrlRejectsMalformed)
{
	for (const char* bad : {
		"", "example.com", "ftp://host/", "http://", "http:///x", "http://host:/",
		"http://host:0/", "http://host:65536/", "http://host:12a/", "http://u:p@host/",
		"http://ho st/", "http://host/a b", "http://host/\r\nX: y", "http://[::1/", "http://[::1]x/",
		"http://a:b:c/", "http://[]/"})
	{
		BOOST_CHECK_MESSAGE(!ParseUrl(bad), "accepted: " << bad);
	}
}

BOOST_AUTO_TEST_CASE(UrlResolvesRedirectReferences)
{
	auto base = ParseUrl("https://a.com/dir/page?q=1");
	BOOST_REQUIRE(base);

	auto abs = ResolveUrl(*base, "http://b.com:81/z");
	BOOST_REQUIRE(abs);
	BOOST_CHECK_EQUAL(abs->host, "b.com");
	BOOST_CHECK(!abs->tls);

	auto schemeRel = ResolveUrl(*base, "//c.com/p");
	BOOST_REQUIRE(schemeRel);
	BOOST_CHECK(schemeRel->tls);
	BOOST_CHECK_EQUAL(schemeRel->host, "c.com");
	BOOST_CHECK_EQUAL(schemeRel->target, "/p");

	auto absPath = ResolveUrl(*base, "/root?x#f");
	BOOST_REQUIRE(absPath);
	BOOST_CHECK_EQUAL(absPath->host, "a.com");
	BOOST_CHECK_EQUAL(absPath->target, "/root?x");

	auto rel = ResolveUrl(*base, "other");
	BOOST_REQUIRE(rel);
	BOOST_CHECK_EQUAL(rel->target, "/dir/other");

	auto query = ResolveUrl(*base, "?y=2");
	BOOST_REQUIRE(query);
	BOOST_CHECK_EQUAL(query->target, "/dir/page?y=2");

	BOOST_CHECK(!ResolveUrl(*base, ""));
	BOOST_CHECK(!ResolveUrl(*base, "ftp://x/"));
	BOOST_CHECK(!ResolveUrl(*base, "mailto:x@y"));
	BOOST_CHECK(!ResolveUrl(*base, "javascript:alert(1)"));
}

BOOST_AUTO_TEST_CASE(UrlRemovesDotSegments)
{
	auto r = ParseUrl("http://host/a/./b/../c");
	BOOST_REQUIRE(r);
	BOOST_CHECK_EQUAL(r->target, "/a/c");

	auto trailing = ParseUrl("http://host/a/b/..");
	BOOST_REQUIRE(trailing);
	BOOST_CHECK_EQUAL(trailing->target, "/a/");

	auto query = ParseUrl("http://host/a/../b?q=1");
	BOOST_REQUIRE(query);
	BOOST_CHECK_EQUAL(query->target, "/b?q=1");
}

BOOST_AUTO_TEST_CASE(UrlResolveRemovesDotSegments)
{
	auto base = ParseUrl("https://a.com/dir/page?q=1");
	BOOST_REQUIRE(base);

	auto parent = ResolveUrl(*base, "../x");
	BOOST_REQUIRE(parent);
	BOOST_CHECK_EQUAL(parent->target, "/x");

	auto inner = ResolveUrl(*base, "a/../b");
	BOOST_REQUIRE(inner);
	BOOST_CHECK_EQUAL(inner->target, "/dir/b");

	auto absolute = ResolveUrl(*base, "/a/b/../../c?z");
	BOOST_REQUIRE(absolute);
	BOOST_CHECK_EQUAL(absolute->target, "/c?z");
}

BOOST_AUTO_TEST_SUITE_END()
