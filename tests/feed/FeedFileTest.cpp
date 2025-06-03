/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2023-2025 Denis <denis@nzbget.com>
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

#include "FileSystem.h"
#include "FeedFile.h"

const std::string CURR_DIR = FileSystem::GetCurrentDirectory().Str();

BOOST_AUTO_TEST_CASE(FeedFileTest)
{
	const std::string testFile = CURR_DIR + "/feed/feed.xml";
	const std::string filename = "Crows.And.Sparrows";
	const std::string description = "   Title:     Test.Title         Added to index:     09/05/2025 14:05:12        Total Size         : 200.0 B         Weblink:     N/A";
	FeedFile file(testFile.c_str(), "feedName");

	BOOST_CHECK_EQUAL(file.Parse(), true);

	std::unique_ptr<FeedItemList> items = file.DetachFeedItems();
	FeedItemInfo& feedInfo = items.get()->back();

	BOOST_CHECK_EQUAL(feedInfo.GetCategory(), std::string("Movies>HD"));
	BOOST_CHECK_EQUAL(feedInfo.GetTime(), 1701668883);
	BOOST_CHECK_EQUAL(feedInfo.GetEpisode(), std::string("1"));
	BOOST_CHECK_EQUAL(feedInfo.GetEpisodeNum(), 1);
	BOOST_CHECK_EQUAL(feedInfo.GetSeason(), std::string("S03"));
	BOOST_CHECK_EQUAL(feedInfo.GetSeasonNum(), 3);
	BOOST_CHECK_EQUAL(feedInfo.GetTvmazeId(), 33877);
	BOOST_CHECK_EQUAL(feedInfo.GetTvdbId(), 33877);
	BOOST_CHECK_EQUAL(feedInfo.GetRageId(), 33877);
	BOOST_CHECK_EQUAL(feedInfo.GetImdbId(), 42054);
	BOOST_CHECK_EQUAL(feedInfo.GetUrl(), std::string("https://indexer.com/getnzb/nzb.nzb"));
	BOOST_CHECK_EQUAL(feedInfo.GetDescription(), description);
	BOOST_CHECK_EQUAL(feedInfo.GetFilename(), filename);
	BOOST_CHECK_EQUAL(feedInfo.GetTitle(), filename);
	BOOST_CHECK_EQUAL(feedInfo.GetSize(), 7445312955);

	xmlCleanupParser();
}

BOOST_AUTO_TEST_CASE(FeedFile2Test)
{
	const std::string testFile = CURR_DIR + "/feed/feed2.xml";
	const std::string filename = "[Judas] Kimi to Boku no Saigo no Senjou, Aruiwa Sekai ga Hajimaru Seisen (Our Last Crusade or the Rise of a New World) - S02E08 [1080p][HEVC x265 10bit][Multi-Subs] (Weekly)";
	const std::string description = "        Total Size         : 299.6 MB              |                                                    MultiUp    ";
	const int64 size = static_cast<int64>(std::round(299.6 * 1024 * 1024));
	FeedFile file(testFile.c_str(), "feedName");

	BOOST_CHECK_EQUAL(file.Parse(), true);

	std::unique_ptr<FeedItemList> items = file.DetachFeedItems();
	FeedItemInfo& feedInfo = items.get()->back();

	BOOST_CHECK_EQUAL(feedInfo.GetCategory(), std::string(""));
	BOOST_CHECK_EQUAL(feedInfo.GetTime(), 1748526661);
	BOOST_CHECK_EQUAL(feedInfo.GetEpisode(), std::string(""));
	BOOST_CHECK_EQUAL(feedInfo.GetEpisodeNum(), 0);
	BOOST_CHECK_EQUAL(feedInfo.GetSeason(), std::string(""));
	BOOST_CHECK_EQUAL(feedInfo.GetSeasonNum(), 0);
	BOOST_CHECK_EQUAL(feedInfo.GetTvmazeId(), 0);
	BOOST_CHECK_EQUAL(feedInfo.GetTvdbId(), 0);
	BOOST_CHECK_EQUAL(feedInfo.GetRageId(), 0);
	BOOST_CHECK_EQUAL(feedInfo.GetImdbId(), 0);
	BOOST_CHECK_EQUAL(feedInfo.GetUrl(), std::string("http://storage.animetosho.org/nzbs/000a9b44/%5BJudas%5D%20Kimisen%20-%20S02E08.nzb"));
	BOOST_CHECK_EQUAL(feedInfo.GetDescription(), description);
	BOOST_CHECK_EQUAL(feedInfo.GetFilename(), filename);
	BOOST_CHECK_EQUAL(feedInfo.GetTitle(), filename);
	BOOST_CHECK_EQUAL(feedInfo.GetSize(), size);

	xmlCleanupParser();
}
