/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2023 Denis <denis@nzbget.com>
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

BOOST_AUTO_TEST_CASE(FeedFileTest)
{
	std::string path = FileSystem::GetCurrentDirectory().Str();
	std::string testFile = path + "/feed/feed.xml";
	FeedFile file(testFile.c_str(), "feedName");

	BOOST_CHECK(file.Parse() == true);

	std::unique_ptr<FeedItemList> items = file.DetachFeedItems();
	FeedItemInfo& feedInfo = items.get()->back();

	BOOST_CHECK(feedInfo.GetCategory() == std::string("Movies>HD"));
	BOOST_CHECK(feedInfo.GetEpisode() == std::string("1"));
	BOOST_CHECK(feedInfo.GetEpisodeNum() == 1);
	BOOST_CHECK(feedInfo.GetSeason() == std::string("S03"));
	BOOST_CHECK(feedInfo.GetSeasonNum() == 3);
	BOOST_CHECK(feedInfo.GetTvmazeId() == 33877);
	BOOST_CHECK(feedInfo.GetTvdbId() == 33877);
	BOOST_CHECK(feedInfo.GetRageId() == 33877);
	BOOST_CHECK(feedInfo.GetImdbId() == 42054);
	BOOST_CHECK(feedInfo.GetUrl() == std::string("https://indexer.com/getnzb/nzb.nzb"));
	BOOST_CHECK(feedInfo.GetDescription() == std::string("Description"));
	BOOST_CHECK(feedInfo.GetFilename() == std::string("Crows.And.Sparrows"));
	BOOST_CHECK(feedInfo.GetSize() == 7445312955);
	BOOST_CHECK(feedInfo.GetTitle() == std::string("Crows.And.Sparrows"));

	xmlCleanupParser();
}
