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
#include "DupeProbe.h"

BOOST_AUTO_TEST_SUITE(QueueTest)

BOOST_AUTO_TEST_CASE(DupeProbeSampleIndexesTest)
{
	// the middle of each of <count> equal parts: spread over the whole posting
	std::vector<size_t> indexes = DupeProbe::SampleIndexes(100, 10);
	BOOST_REQUIRE_EQUAL(indexes.size(), 10U);
	BOOST_CHECK_EQUAL(indexes.front(), 5U);
	BOOST_CHECK_EQUAL(indexes.back(), 95U);
	for (size_t i = 1; i < indexes.size(); i++)
	{
		BOOST_CHECK_EQUAL(indexes[i] - indexes[i - 1], 10U);
	}

	// fewer articles than samples: each article once
	indexes = DupeProbe::SampleIndexes(4, 10);
	BOOST_REQUIRE_EQUAL(indexes.size(), 4U);
	BOOST_CHECK_EQUAL(indexes[0], 0U);
	BOOST_CHECK_EQUAL(indexes[3], 3U);

	// all indexes are valid and increasing for any size
	for (size_t total = 1; total < 300; total++)
	{
		indexes = DupeProbe::SampleIndexes(total, 10);
		for (size_t i = 0; i < indexes.size(); i++)
		{
			BOOST_CHECK(indexes[i] < total);
			BOOST_CHECK(i == 0 || indexes[i] > indexes[i - 1]);
		}
	}

	BOOST_CHECK(DupeProbe::SampleIndexes(0, 10).empty());
	BOOST_CHECK(DupeProbe::SampleIndexes(10, 0).empty());
}

BOOST_AUTO_TEST_CASE(DupeProbeClassifyTest)
{
	BOOST_CHECK(DupeProbe::Classify("223 0 <id> article exists") == DupeProbe::daExists);
	BOOST_CHECK(DupeProbe::Classify("430 No Such Article Found") == DupeProbe::daMissing);
	BOOST_CHECK(DupeProbe::Classify("423 No such article number") == DupeProbe::daMissing);
	BOOST_CHECK(DupeProbe::Classify("412 no newsgroup selected") == DupeProbe::daMissing);
	// some providers answer 451 for a missing article
	BOOST_CHECK(DupeProbe::Classify("451 Article not found") == DupeProbe::daMissing);

	// no evidence: no reply, connection and authentication problems, server errors
	BOOST_CHECK(DupeProbe::Classify(nullptr) == DupeProbe::daUnknown);
	BOOST_CHECK(DupeProbe::Classify("400 service temporarily unavailable") == DupeProbe::daUnknown);
	BOOST_CHECK(DupeProbe::Classify("499 connection limit") == DupeProbe::daUnknown);
	BOOST_CHECK(DupeProbe::Classify("481 authentication failed") == DupeProbe::daUnknown);
	BOOST_CHECK(DupeProbe::Classify("502 too many connections") == DupeProbe::daUnknown);
	BOOST_CHECK(DupeProbe::Classify("500 unknown command") == DupeProbe::daUnknown);
}

BOOST_AUTO_TEST_CASE(DupeProbeIsDeadTest)
{
	// nothing exists and enough servers answered definitively
	BOOST_CHECK(DupeProbe::IsDead(0, 8, 8));
	BOOST_CHECK(DupeProbe::IsDead(0, 5, 8));
	BOOST_CHECK(!DupeProbe::IsDead(0, 4, 8));

	// with fewer servers than the minimum, all of them must agree
	BOOST_CHECK(DupeProbe::IsDead(0, 1, 1));
	BOOST_CHECK(DupeProbe::IsDead(0, 3, 3));
	BOOST_CHECK(!DupeProbe::IsDead(0, 2, 3));

	// B45: one stray sample found doesn't make a posting alive, two do
	BOOST_CHECK(DupeProbe::IsDead(1, 8, 8));
	BOOST_CHECK(!DupeProbe::IsDead(2, 8, 8));
	BOOST_CHECK(!DupeProbe::IsDead(3, 5, 5));
	BOOST_CHECK(!DupeProbe::IsDead(1, 4, 8));

	// no definitive server: no verdict
	BOOST_CHECK(!DupeProbe::IsDead(0, 0, 0));
	BOOST_CHECK(!DupeProbe::IsDead(0, 0, 6));
}

BOOST_AUTO_TEST_SUITE_END()
