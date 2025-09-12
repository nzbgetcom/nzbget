/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2025 Denis <denis@nzbget.com>
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

#include "FeedCoordinator.h"
#include "FeedInfo.h"
#include "DownloadInfo.h"

static FeedInfo MakeFeedInfo(
	int id,
	const char* category,
	FeedInfo::CategorySource categorySource,
	const char* name = "Test Feed",
	const char* url = "https://example.com/feed"
)
{
	return FeedInfo(
		id,
		name,
		url,
		false, // backlog
		15,    // interval
		"",    // filter
		true,  // pauseNzb
		category,
		categorySource,
		10,    // priority
		"nzb"  // extensions
	);
}

static void InitItemInfo(
	FeedItemInfo& item,
	const char* url = "https://example.com/file.nzb",
	const char* filename = "TestFile",
	const char* category = "",
	const char* addCategory = "",
	int64 size = 12345,
	time_t time = 1000,
	int priority = 5,
	bool pause = true,
	const char* dupeKey = "dupe-key",
	int dupeScore = 7,
	EDupeMode dupeMode = EDupeMode::dmAll
)
{
	item.SetUrl(url);
	item.SetFilename(filename);
	item.SetCategory(category);
	item.SetAddCategory(addCategory);
	item.SetSize(size);
	item.SetTime(time);
	item.SetPriority(priority);
	item.SetPauseNzb(pause);
	item.SetDupeKey(dupeKey);
	item.SetDupeScore(dupeScore);
	item.SetDupeMode(dupeMode);
}

// Test the NzbInfoCreator class
BOOST_AUTO_TEST_CASE(CreateNzbInfoTest)
{
	const NzbInfoCreator creator;
	const FeedInfo feedInfo = MakeFeedInfo(42, "Movies", FeedInfo::CategorySource::Auto);
	FeedItemInfo itemInfo;
	InitItemInfo(itemInfo);

	const auto nzbInfo = creator.Create(feedInfo, itemInfo);

	BOOST_REQUIRE(nzbInfo);
	BOOST_CHECK_EQUAL(nzbInfo->GetKind(), NzbInfo::nkUrl);
	BOOST_CHECK_EQUAL(nzbInfo->GetFeedId(), 42);
	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetUrl()), "https://example.com/file.nzb");
	BOOST_CHECK_EQUAL(nzbInfo->GetPriority(), 5);
	BOOST_CHECK_EQUAL(nzbInfo->GetAddUrlPaused(), true);
	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetDupeKey()), "dupe-key");
	BOOST_CHECK_EQUAL(nzbInfo->GetDupeScore(), 7);
	BOOST_CHECK_EQUAL(nzbInfo->GetDupeMode(), EDupeMode::dmAll);
	BOOST_CHECK_EQUAL(nzbInfo->GetSize(), 12345);
	BOOST_CHECK_EQUAL(nzbInfo->GetMinTime(), (time_t)1000);
	BOOST_CHECK_EQUAL(nzbInfo->GetMaxTime(), (time_t)1000);
}

BOOST_AUTO_TEST_CASE(FilenameExtensionHandlingTest)
{
	const NzbInfoCreator creator;
	const FeedInfo feedInfo = MakeFeedInfo(1, "TV", FeedInfo::CategorySource::Auto);

	// Test 1: No extension -> .nzb appended
	{
		FeedItemInfo itemInfo;
		InitItemInfo(itemInfo, "https://example.com/1", "Alpha.Beta");
		const auto nzbInfo = creator.Create(feedInfo, itemInfo);
		BOOST_REQUIRE(nzbInfo);
		BOOST_CHECK_EQUAL(std::string(nzbInfo->GetFilename()), "Alpha.Beta.nzb");
	}

	// Test 2: Already has .nzb -> ensure single .nzb
	{
		FeedItemInfo itemInfo;
		InitItemInfo(itemInfo, "https://example.com/2", "Gamma.nZb");
		const auto nzbInfo = creator.Create(feedInfo, itemInfo);
		BOOST_REQUIRE(nzbInfo);
		BOOST_CHECK_EQUAL(std::string(nzbInfo->GetFilename()), "Gamma.nzb");
	}

	// Test 3: Empty filename -> no extension added
	{
		FeedItemInfo itemInfo;
		InitItemInfo(itemInfo, "https://example.com/4", "");
		const auto nzbInfo = creator.Create(feedInfo, itemInfo);
		BOOST_REQUIRE(nzbInfo);
		BOOST_CHECK_EQUAL(std::string(nzbInfo->GetFilename()), "");
	}
}

BOOST_AUTO_TEST_CASE(ApplyCategoryOverridesTest)
{
	const NzbInfoCreator creator;

	// AddCategory should always override everything
	{
		const FeedInfo feedInfo = MakeFeedInfo(1, "FeedCategory", FeedInfo::CategorySource::Auto);
		FeedItemInfo itemInfo;
		InitItemInfo(itemInfo);
		itemInfo.SetCategory("ItemCategory");
		itemInfo.SetAddCategory("OverrideCategory");

		const auto nzbInfo = creator.Create(feedInfo, itemInfo);
		BOOST_REQUIRE(nzbInfo);
		BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCategory()), "OverrideCategory");
	}
}

BOOST_AUTO_TEST_CASE(ApplyCategoryFeedFileSourceTest)
{
	const NzbInfoCreator creator;

	// FeedFile source should use feed category, ignore item category
	{
		const FeedInfo feedInfo = MakeFeedInfo(1, "FeedCategory", FeedInfo::CategorySource::FeedFile);
		FeedItemInfo itemInfo;
		InitItemInfo(itemInfo);
		itemInfo.SetCategory("ItemCategory");

		const auto nzbInfo = creator.Create(feedInfo, itemInfo);
		BOOST_REQUIRE(nzbInfo);
		BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCategory()), "FeedCategory");
	}

	// Empty feed category should result in empty category
	{
		const FeedInfo feedInfo = MakeFeedInfo(1, "", FeedInfo::CategorySource::FeedFile);
		FeedItemInfo itemInfo;
		InitItemInfo(itemInfo);
		itemInfo.SetCategory("ItemCategory");

		const auto nzbInfo = creator.Create(feedInfo, itemInfo);
		BOOST_REQUIRE(nzbInfo);
		BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCategory()), "");
	}
}

BOOST_AUTO_TEST_CASE(ApplyCategoryNZBFileSourceTest)
{
	const NzbInfoCreator creator;

	// NZBFile source should use item category, ignore feed category
	{
		const FeedInfo feedInfo = MakeFeedInfo(1, "FeedCategory", FeedInfo::CategorySource::NZBFile);
		FeedItemInfo itemInfo;
		InitItemInfo(itemInfo);
		itemInfo.SetCategory("ItemCategory");

		const auto nzbInfo = creator.Create(feedInfo, itemInfo);
		BOOST_REQUIRE(nzbInfo);
		BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCategory()), "ItemCategory");
	}

	// Empty item category should result in empty category
	{
		const FeedInfo feedInfo = MakeFeedInfo(1, "FeedCategory", FeedInfo::CategorySource::NZBFile);
		FeedItemInfo itemInfo;
		InitItemInfo(itemInfo);
		itemInfo.SetCategory("");

		const auto nzbInfo = creator.Create(feedInfo, itemInfo);
		BOOST_REQUIRE(nzbInfo);
		BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCategory()), "");
	}
}

BOOST_AUTO_TEST_CASE(ApplyCategoryAutoSourceTest)
{
	const NzbInfoCreator creator;

	// Auto source should try item category first, fallback to feed category
	{
		const FeedInfo feedInfo = MakeFeedInfo(1, "FeedCategory", FeedInfo::CategorySource::Auto);
		FeedItemInfo itemInfo;
		InitItemInfo(itemInfo);
		itemInfo.SetCategory("ItemCategory");

		const auto nzbInfo = creator.Create(feedInfo, itemInfo);
		BOOST_REQUIRE(nzbInfo);
		BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCategory()), "ItemCategory");
	}

	// Auto source should fallback to feed category when item category is empty
	{
		const FeedInfo feedInfo = MakeFeedInfo(1, "FeedCategory", FeedInfo::CategorySource::Auto);
		FeedItemInfo itemInfo;
		InitItemInfo(itemInfo);
		itemInfo.SetCategory("");

		const auto nzbInfo = creator.Create(feedInfo, itemInfo);
		BOOST_REQUIRE(nzbInfo);
		BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCategory()), "FeedCategory");
	}

	// Auto source should use empty string when both are empty
	{
		const FeedInfo feedInfo = MakeFeedInfo(1, "", FeedInfo::CategorySource::Auto);
		FeedItemInfo itemInfo;
		InitItemInfo(itemInfo);
		itemInfo.SetCategory("");

		const auto nzbInfo = creator.Create(feedInfo, itemInfo);
		BOOST_REQUIRE(nzbInfo);
		BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCategory()), "");
	}
}

BOOST_AUTO_TEST_CASE(ApplyCategoryEdgeCasesTest)
{
	const NzbInfoCreator creator;

	// Test with nullptr categories (should be handled gracefully)
	{
		const FeedInfo feedInfo = MakeFeedInfo(1, nullptr, FeedInfo::CategorySource::FeedFile);
		FeedItemInfo itemInfo;
		InitItemInfo(itemInfo);
		itemInfo.SetCategory(nullptr);

		const auto nzbInfo = creator.Create(feedInfo, itemInfo);
		BOOST_REQUIRE(nzbInfo);
		BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCategory()), "");
	}

	// Test with whitespace-only categories
	{
		const FeedInfo feedInfo = MakeFeedInfo(1, "   ", FeedInfo::CategorySource::FeedFile);
		FeedItemInfo itemInfo;
		InitItemInfo(itemInfo);
		itemInfo.SetCategory("   ");

		const auto nzbInfo = creator.Create(feedInfo, itemInfo);
		BOOST_REQUIRE(nzbInfo);
		// Should preserve whitespace (Util::EmptyStr checks for empty, not whitespace)
		BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCategory()), "   ");
	}
}

BOOST_AUTO_TEST_CASE(CreateNzbInfoAllPropertiesTest)
{
	const NzbInfoCreator creator;
	const FeedInfo feedInfo = MakeFeedInfo(42, "Movies>HD", FeedInfo::CategorySource::Auto);
	FeedItemInfo itemInfo;
	InitItemInfo(
		itemInfo,
		"https://example.com/movie.nzb",
		"Movie.Title.2024",
		"Movies>HD",
		"OverrideCategory",
		987654321,
		2000,
		15,
		false,
		"movie-key-123",
		10,
		EDupeMode::dmScore
	);

	const auto nzbInfo = creator.Create(feedInfo, itemInfo);

	BOOST_REQUIRE(nzbInfo);

	// Basic properties
	BOOST_CHECK_EQUAL(nzbInfo->GetKind(), NzbInfo::nkUrl);
	BOOST_CHECK_EQUAL(nzbInfo->GetFeedId(), 42);
	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetUrl()), "https://example.com/movie.nzb");
	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetFilename()), "Movie.Title.2024.nzb");

	// Category (AddCategory should override)
	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCategory()), "OverrideCategory");

	// Priority and pause settings
	BOOST_CHECK_EQUAL(nzbInfo->GetPriority(), 15);
	BOOST_CHECK_EQUAL(nzbInfo->GetAddUrlPaused(), false);

	// Dupe settings
	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetDupeKey()), "movie-key-123");
	BOOST_CHECK_EQUAL(nzbInfo->GetDupeScore(), 10);
	BOOST_CHECK_EQUAL(nzbInfo->GetDupeMode(), EDupeMode::dmScore);

	// Size and time
	BOOST_CHECK_EQUAL(nzbInfo->GetSize(), 987654321);
	BOOST_CHECK_EQUAL(nzbInfo->GetMinTime(), (time_t)2000);
	BOOST_CHECK_EQUAL(nzbInfo->GetMaxTime(), (time_t)2000);
}
