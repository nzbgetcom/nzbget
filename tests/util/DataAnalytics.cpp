/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2024-2026 Denis <denis@nzbget.com>
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
#include "DataAnalytics.h"

BOOST_AUTO_TEST_SUITE(UtilTest)

BOOST_AUTO_TEST_CASE(test_data_analytics)
{
	DataAnalytics da;

	da.Compute();

	BOOST_CHECK_EQUAL(da.GetMin(), 0.0);
	BOOST_CHECK_EQUAL(da.GetMax(), 0.0);
	BOOST_CHECK_EQUAL(da.GetAvg(), 0.0);
	BOOST_CHECK_EQUAL(da.GetMedian(), 0.0);
	BOOST_CHECK_EQUAL(da.GetPercentile25(), 0.0);
	BOOST_CHECK_EQUAL(da.GetPercentile75(), 0.0);
	BOOST_CHECK_EQUAL(da.GetPercentile90(), 0.0);

	da.AddMeasurement(5.0);
	da.AddMeasurement(10.0);
	da.AddMeasurement(15.0);

	da.Compute();

	BOOST_CHECK_EQUAL(da.GetMin(), 5.0);
	BOOST_CHECK_EQUAL(da.GetMax(), 15.0);
	BOOST_CHECK_EQUAL(da.GetAvg(), 10.0);
	BOOST_CHECK_EQUAL(da.GetMedian(), 10.0);
	BOOST_CHECK_EQUAL(da.GetPercentile25(), 7.5);
	BOOST_CHECK_EQUAL(da.GetPercentile75(), 12.5);
	BOOST_CHECK_EQUAL(da.GetPercentile90(), 14.0);


	da.AddMeasurement(2.0);
	da.AddMeasurement(8.0);

	da.Compute();

	BOOST_CHECK_EQUAL(da.GetMin(), 2.0);
	BOOST_CHECK_EQUAL(da.GetMax(), 15.0);
	BOOST_CHECK_EQUAL(da.GetAvg(), 8.0);
	BOOST_CHECK_EQUAL(da.GetMedian(), 8.0);
	BOOST_CHECK_EQUAL(da.GetPercentile25(), 5.0);
	BOOST_CHECK_EQUAL(da.GetPercentile75(), 10.0);
	BOOST_CHECK_EQUAL(da.GetPercentile90(), 13.0);
}

BOOST_AUTO_TEST_SUITE_END()
