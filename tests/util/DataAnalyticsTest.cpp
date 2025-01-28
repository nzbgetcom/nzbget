#include "nzbget.h"

#include <boost/test/unit_test.hpp>
#include "DataAnalytics.h"

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
