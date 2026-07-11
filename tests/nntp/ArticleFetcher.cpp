/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Denis <denis@nzbget.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "nzbget.h"

#include <boost/test/unit_test.hpp>
#include "ArticleFetcher.h"

BOOST_AUTO_TEST_SUITE(NNTPTest)

BOOST_AUTO_TEST_CASE(ArticleFetchLimitsRejectUnboundedHeader)
{
	ArticleFetchLimits limits;

	BOOST_CHECK(limits.AddRawBytes(ArticleFetchLimits::MaxHeaderBytes));
	BOOST_CHECK(limits.HeaderWithinLimit(false));
	BOOST_CHECK(limits.AddRawBytes(1));
	BOOST_CHECK(!limits.HeaderWithinLimit(false));
	BOOST_CHECK(limits.HeaderWithinLimit(true));
}

BOOST_AUTO_TEST_CASE(ArticleFetchLimitsRejectUnboundedWireBody)
{
	ArticleFetchLimits limits;

	BOOST_CHECK(limits.AddRawBytes(ArticleFetchLimits::MaxRawBytes));
	BOOST_CHECK(!limits.AddRawBytes(1));
}

BOOST_AUTO_TEST_CASE(ArticleFetchLimitsEnforceDeclaredDecodedRange)
{
	ArticleFetchLimits limits;

	BOOST_CHECK(!limits.AddDecodedBytes(1, 0, 10, 100));
	BOOST_CHECK(!limits.AddDecodedBytes(1, 11, 10, 100));
	BOOST_CHECK(!limits.AddDecodedBytes(1, 91, 101, 100));

	BOOST_CHECK(limits.AddDecodedBytes(4, 11, 20, 100));
	BOOST_CHECK(limits.AddDecodedBytes(6, 11, 20, 100));
	BOOST_CHECK(!limits.AddDecodedBytes(1, 11, 20, 100));
}

BOOST_AUTO_TEST_CASE(ArticleFetchLimitsEnforceHardDecodedCap)
{
	ArticleFetchLimits limits;
	const int64 tooLargeEnd = ArticleFetchLimits::MaxDecodedBytes + 1;

	BOOST_CHECK(!limits.AddDecodedBytes(1, 1, tooLargeEnd, tooLargeEnd));
	BOOST_CHECK(limits.AddDecodedBytes(
		ArticleFetchLimits::MaxDecodedBytes, 1,
		ArticleFetchLimits::MaxDecodedBytes,
		ArticleFetchLimits::MaxDecodedBytes));
	BOOST_CHECK(!limits.AddDecodedBytes(
		1, 1, ArticleFetchLimits::MaxDecodedBytes,
		ArticleFetchLimits::MaxDecodedBytes));
}

BOOST_AUTO_TEST_SUITE_END()
