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

#include <atomic>
#include <memory>
#include <stdexcept>
#include <thread>
#include <boost/test/unit_test.hpp>
#include "ArticleFetcher.h"
#include "Util.h"

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

namespace
{
	ArticleFetcher::FetchedArticle MakeResult(char tag, int64 size = 4)
	{
		ArticleFetcher::FetchedArticle result;
		result.Data.assign((size_t)size, tag);
		result.Offset = 0;
		result.FileSize = size;
		result.Success = true;
		return result;
	}
}

BOOST_AUTO_TEST_CASE(ArticleBatchFetcherDeliversInSubmissionOrder)
{
	// later requests complete FASTER; delivery must still be 0,1,2,...
	ArticleBatchFetcher batch([](const ArticleBatchFetcher::Request& request)
	{
		int index = atoi(request.MessageId);
		Util::Sleep(40 - index * 10);
		return MakeResult((char)('a' + index));
	});
	batch.SetWorkerCount(3);

	auto noGroups = std::make_shared<std::vector<CString>>();
	std::vector<ArticleBatchFetcher::Request> requests;
	for (int i = 0; i < 4; i++)
	{
		requests.push_back({CString(BString<20>("%i", i)), noGroups});
	}
	batch.Begin(std::move(requests));

	ArticleFetcher::FetchedArticle fetched;
	for (int i = 0; i < 4; i++)
	{
		BOOST_REQUIRE(batch.Next(fetched));
		BOOST_CHECK_EQUAL(fetched.Data[0], (char)('a' + i));
	}
	BOOST_CHECK(!batch.Next(fetched));
}

BOOST_AUTO_TEST_CASE(ArticleBatchFetcherHonorsWindowBound)
{
	// more workers than the window, instant fetches, and a consumer that
	// never delivers: claims must settle at exactly the window bound
	std::atomic<int> started{0};
	ArticleBatchFetcher batch([&](const ArticleBatchFetcher::Request&)
	{
		started++;
		return MakeResult('w');
	});
	batch.SetWorkerCount(ArticleBatchFetcher::MaxWindowParts * 2);

	auto noGroups = std::make_shared<std::vector<CString>>();
	std::vector<ArticleBatchFetcher::Request> requests;
	for (int i = 0; i < ArticleBatchFetcher::MaxWindowParts * 3; i++)
	{
		requests.push_back({CString("m"), noGroups});
	}
	batch.Begin(std::move(requests));
	Util::Sleep(300);
	BOOST_CHECK_EQUAL(started.load(), ArticleBatchFetcher::MaxWindowParts);
	ArticleFetcher::FetchedArticle fetched;
	int delivered = 0;
	while (batch.Next(fetched))
	{
		delivered++;
	}
	BOOST_CHECK_EQUAL(delivered, ArticleBatchFetcher::MaxWindowParts * 3);
}

BOOST_AUTO_TEST_CASE(ArticleBatchFetcherByteCapThrottlesClaims)
{
	std::atomic<int> started{0};
	ArticleBatchFetcher batch([&](const ArticleBatchFetcher::Request&)
	{
		started++;
		return MakeResult('b', ArticleBatchFetcher::MaxBufferedBytes + 1);
	});
	batch.SetWorkerCount(1);

	auto noGroups = std::make_shared<std::vector<CString>>();
	std::vector<ArticleBatchFetcher::Request> requests;
	for (int i = 0; i < 3; i++)
	{
		requests.push_back({CString("m"), noGroups});
	}
	batch.Begin(std::move(requests));
	Util::Sleep(300);
	// the single buffered oversized result exceeds the cap: no further claim
	// until the consumer drains it
	BOOST_CHECK_EQUAL(started.load(), 1);
	ArticleFetcher::FetchedArticle fetched;
	BOOST_REQUIRE(batch.Next(fetched));
	Util::Sleep(300);
	BOOST_CHECK_EQUAL(started.load(), 2);
	batch.CancelRemaining();
}

BOOST_AUTO_TEST_CASE(ArticleBatchFetcherCancelStopsClaims)
{
	std::atomic<int> started{0};
	ArticleBatchFetcher batch([&](const ArticleBatchFetcher::Request&)
	{
		started++;
		Util::Sleep(50);
		return MakeResult('c');
	});
	batch.SetWorkerCount(2);

	auto noGroups = std::make_shared<std::vector<CString>>();
	std::vector<ArticleBatchFetcher::Request> requests;
	for (int i = 0; i < 20; i++)
	{
		requests.push_back({CString("m"), noGroups});
	}
	batch.Begin(std::move(requests));
	ArticleFetcher::FetchedArticle fetched;
	BOOST_REQUIRE(batch.Next(fetched));
	batch.CancelRemaining();
	int claimedAtCancel = started.load();
	Util::Sleep(200);
	// in-flight fetches may finish, but no NEW claims after the cancel
	BOOST_CHECK_LE(started.load(), claimedAtCancel + 2);
	BOOST_CHECK(!batch.Next(fetched));
}

BOOST_AUTO_TEST_CASE(ArticleBatchFetcherInlineModeFetchesOnCallerThread)
{
	std::atomic<int> calls{0};
	ArticleBatchFetcher batch([&](const ArticleBatchFetcher::Request& request)
	{
		calls++;
		return MakeResult((char)('a' + atoi(request.MessageId)));
	});
	batch.SetWorkerCount(0);

	auto noGroups = std::make_shared<std::vector<CString>>();
	std::vector<ArticleBatchFetcher::Request> requests;
	for (int i = 0; i < 3; i++)
	{
		requests.push_back({CString(BString<20>("%i", i)), noGroups});
	}
	batch.Begin(std::move(requests));
	ArticleFetcher::FetchedArticle fetched;
	BOOST_REQUIRE(batch.Next(fetched));
	// inline mode is strictly lazy: exactly one fetch per delivered Next()
	BOOST_CHECK_EQUAL(calls.load(), 1);
	BOOST_CHECK_EQUAL(fetched.Data[0], 'a');
	BOOST_REQUIRE(batch.Next(fetched));
	BOOST_CHECK_EQUAL(fetched.Data[0], 'b');
	BOOST_REQUIRE(batch.Next(fetched));
	BOOST_CHECK(!batch.Next(fetched));
	BOOST_CHECK_EQUAL(calls.load(), 3);
}

BOOST_AUTO_TEST_CASE(ArticleBatchFetcherThrowingFetchDeliversFailedResult)
{
	// a throwing fetch (e.g. bad_alloc on a huge decode buffer) must neither
	// kill the process nor wedge the window on a never-ready slot: the
	// consumer receives a failed result for that request, still in order
	ArticleBatchFetcher batch([](const ArticleBatchFetcher::Request& request)
	{
		int index = atoi(request.MessageId);
		if (index == 1)
		{
			throw std::runtime_error("simulated fetch failure");
		}
		return MakeResult((char)('a' + index));
	});
	batch.SetWorkerCount(2);

	auto noGroups = std::make_shared<std::vector<CString>>();
	std::vector<ArticleBatchFetcher::Request> requests;
	for (int i = 0; i < 4; i++)
	{
		requests.push_back({CString(BString<20>("%i", i)), noGroups});
	}
	batch.Begin(std::move(requests));

	ArticleFetcher::FetchedArticle fetched;
	for (int i = 0; i < 4; i++)
	{
		BOOST_REQUIRE(batch.Next(fetched));
		if (i == 1)
		{
			BOOST_CHECK(!fetched.Success);
		}
		else
		{
			BOOST_CHECK(fetched.Success);
			BOOST_CHECK_EQUAL(fetched.Data[0], (char)('a' + i));
		}
	}
	BOOST_CHECK(!batch.Next(fetched));
}

BOOST_AUTO_TEST_CASE(ArticleBatchFetcherStopUnblocksConsumer)
{
	std::atomic<bool> release{false};
	ArticleBatchFetcher batch([&](const ArticleBatchFetcher::Request&)
	{
		while (!release)
		{
			Util::Sleep(1);
		}
		return MakeResult('s');
	});
	batch.SetWorkerCount(1);

	auto noGroups = std::make_shared<std::vector<CString>>();
	std::vector<ArticleBatchFetcher::Request> requests;
	for (int i = 0; i < 2; i++)
	{
		requests.push_back({CString("m"), noGroups});
	}
	batch.Begin(std::move(requests));

	std::thread stopper([&] { Util::Sleep(100); batch.Stop(); });
	ArticleFetcher::FetchedArticle fetched;
	BOOST_CHECK(!batch.Next(fetched));	// unblocked by Stop, no result
	release = true;						// let the worker exit its fetch
	stopper.join();
}

BOOST_AUTO_TEST_CASE(ArticleBatchFetcherRequestsOwnTheirGroups)
{
	// requests must OWN their group list: the repair pass destroys the donor
	// NzbInfo (the former Groups pointee) between donors while CancelRemaining
	// leaves in-flight workers running - a raw pointer here was a use-after-free
	std::atomic<bool> release{false};
	std::atomic<bool> groupsIntact{false};
	ArticleBatchFetcher batch([&](const ArticleBatchFetcher::Request& request)
	{
		while (!release)
		{
			Util::Sleep(1);
		}
		groupsIntact = request.Groups && request.Groups->size() == 1 &&
			!strcmp((*request.Groups)[0], "alt.binaries.ownership");
		return MakeResult('g');
	});
	batch.SetWorkerCount(1);

	auto sourceGroups = std::make_shared<std::vector<CString>>();
	sourceGroups->emplace_back("alt.binaries.ownership");
	std::vector<ArticleBatchFetcher::Request> requests;
	requests.push_back({CString("m"), sourceGroups});
	batch.Begin(std::move(requests));

	// drop the caller's only reference while the worker is still gated:
	// the request's own shared copy must keep the vector alive
	sourceGroups.reset();
	release = true;

	ArticleFetcher::FetchedArticle fetched;
	BOOST_REQUIRE(batch.Next(fetched));
	BOOST_CHECK(groupsIntact.load());
	BOOST_CHECK(!batch.Next(fetched));
}

BOOST_AUTO_TEST_SUITE_END()
