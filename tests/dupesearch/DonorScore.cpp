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
#include "DonorScore.h"

BOOST_AUTO_TEST_SUITE(DupeSearchTest)

BOOST_AUTO_TEST_CASE(DonorScoreBase)
{
	BOOST_CHECK_EQUAL(DonorScore::Base(1000000), 999000);
	BOOST_CHECK_EQUAL(DonorScore::Base(23859118), 23858118);
	BOOST_CHECK_EQUAL(DonorScore::Base(500), 0);
}

BOOST_AUTO_TEST_CASE(DonorScoreTarget)
{
	BOOST_CHECK_EQUAL(DonorScore::Target(1.0, false), 89);
	BOOST_CHECK_EQUAL(DonorScore::Target(1.0, true), 90);
	BOOST_CHECK_EQUAL(DonorScore::Target(-1, false), 89);
	BOOST_CHECK_EQUAL(DonorScore::Target(0.5, false), 49);
	BOOST_CHECK_EQUAL(DonorScore::Target(0.0, false), 9);
	BOOST_CHECK_EQUAL(DonorScore::Target(0.95, false), 85);
	BOOST_CHECK_EQUAL(DonorScore::Target(0.9, true), 82);
}

BOOST_AUTO_TEST_CASE(DonorScoreRanksAreUnique)
{
	DonorScore::Ranks ranks;
	BOOST_CHECK_EQUAL(ranks.Take(1.0, true), 90);
	BOOST_CHECK_EQUAL(ranks.Take(1.0, true), 89);
	BOOST_CHECK_EQUAL(ranks.Take(1.0, false), 88);
	ranks.Release(89);
	BOOST_CHECK_EQUAL(ranks.Take(1.0, false), 89);
	// never down to the dead score
	DonorScore::Ranks low;
	for (int i = 0; i < 20; i++)
	{
		BOOST_CHECK_GE(low.Take(0.0, false), DonorScore::Dead + 1);
	}
}

BOOST_AUTO_TEST_CASE(DonorScoreRerankWholenessFirst)
{
	// added in the wrong order: twin90, other95, twin100, other100
	std::vector<DonorScore::Entry> entries(4);
	entries[0] = { 90, 0.9, true, 1000, 50 };
	entries[1] = { 89, 0.95, false, 1100, 40 };
	entries[2] = { 88, 1.0, true, 1000, 1 };
	entries[3] = { 87, 1.0, false, 1200, 1 };
	std::vector<int> scores = DonorScore::Rerank(entries, 1000);
	BOOST_REQUIRE_EQUAL(scores.size(), 4u);
	BOOST_CHECK_EQUAL(scores[2], 90);	// twin100
	BOOST_CHECK_EQUAL(scores[3], 89);	// other100
	BOOST_CHECK_EQUAL(scores[1], 85);	// other95
	BOOST_CHECK_EQUAL(scores[0], 82);	// twin90
}

BOOST_AUTO_TEST_CASE(DonorScoreRerankKeepsDeadAndFalls)
{
	std::vector<DonorScore::Entry> entries(3);
	entries[0] = { 1, 0.0, false, 1000, 5 };
	entries[1] = { 50, 1.0, false, 1000, 5 };
	entries[2] = { 49, 1.0, false, 1000, 4 };
	std::vector<int> scores = DonorScore::Rerank(entries, 1000);
	BOOST_CHECK_EQUAL(scores[0], 1);
	BOOST_CHECK_EQUAL(scores[1], 89);
	BOOST_CHECK_EQUAL(scores[2], 88);	// same whole percent: strictly falling, grabs break the tie
}

BOOST_AUTO_TEST_SUITE_END()
