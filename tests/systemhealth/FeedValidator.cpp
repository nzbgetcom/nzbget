#include "nzbget.h"

#include <boost/test/unit_test.hpp>
#include "Options.h"
#include "FeedValidator.h"
#include "FeedInfo.h"

BOOST_AUTO_TEST_SUITE(SystemHealthTest)

struct FeedFixture
{
	std::unique_ptr<FeedInfo> CreateFeed(
		const char* url,
		unsigned int certLevel = Options::cvStrict)
	{
		return std::make_unique<FeedInfo>(1, "TestFeed", url, false, 15, "", false, "",
										  FeedInfo::CategorySource::Auto, 0, "", certLevel);
	}
};

BOOST_FIXTURE_TEST_SUITE(FeedValidatorsSuite, FeedFixture)

BOOST_AUTO_TEST_CASE(TestFeedCertVerification)
{
	BOOST_CHECK(SystemHealth::Feeds::FeedCertValidator(
					*CreateFeed("https://my.feed.com", Options::cvStrict))
					.Validate()
					.IsOk());

	BOOST_CHECK(SystemHealth::Feeds::FeedCertValidator(
					*CreateFeed("", Options::cvNone))
					.Validate()
					.IsOk());

	BOOST_CHECK(SystemHealth::Feeds::FeedCertValidator(
					*CreateFeed("https://my.feed.com", Options::cvNone))
					.Validate()
					.IsWarning());

	BOOST_CHECK(SystemHealth::Feeds::FeedCertValidator(
					*CreateFeed("https://my.feed.com", Options::cvMinimal))
					.Validate()
					.IsOk());

	BOOST_CHECK(SystemHealth::Feeds::FeedCertValidator(
					*CreateFeed("http://my.feed.com", Options::cvNone))
					.Validate()
					.IsOk());
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
