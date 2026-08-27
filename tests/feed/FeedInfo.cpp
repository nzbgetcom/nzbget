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
#include "FeedInfo.h"

BOOST_AUTO_TEST_SUITE(FeedTest)

BOOST_AUTO_TEST_CASE(BuildDupeKeyPrefersEpisodeIdentityOverImdb)
{
	FeedItemInfo firstEpisode;
	firstEpisode.SetImdbId(28369634);
	firstEpisode.SetTvdbId(439270);
	firstEpisode.SetSeason("S02");
	firstEpisode.SetEpisode("E69");
	firstEpisode.BuildDupeKey(nullptr, nullptr, nullptr, nullptr);

	FeedItemInfo secondEpisode;
	secondEpisode.SetImdbId(28369634);
	secondEpisode.SetTvdbId(439270);
	secondEpisode.SetSeason("S02");
	secondEpisode.SetEpisode("E71");
	secondEpisode.BuildDupeKey(nullptr, nullptr, nullptr, nullptr);

	BOOST_CHECK_EQUAL(firstEpisode.GetDupeKey(), "tvdbid=439270-S02-E69");
	BOOST_CHECK_EQUAL(secondEpisode.GetDupeKey(), "tvdbid=439270-S02-E71");
	BOOST_CHECK_NE(firstEpisode.GetDupeKey(), secondEpisode.GetDupeKey());

	FeedItemInfo movie;
	movie.SetImdbId(28369634);
	movie.BuildDupeKey(nullptr, nullptr, nullptr, nullptr);
	BOOST_CHECK_EQUAL(movie.GetDupeKey(), "imdb=28369634");

	FeedItemInfo episodeWithoutSeriesId;
	episodeWithoutSeriesId.SetImdbId(28369634);
	episodeWithoutSeriesId.SetSeason("S02");
	episodeWithoutSeriesId.SetEpisode("E69");
	episodeWithoutSeriesId.BuildDupeKey(nullptr, nullptr, nullptr, nullptr);
	BOOST_CHECK_EQUAL(episodeWithoutSeriesId.GetDupeKey(), "imdb=28369634");
}

BOOST_AUTO_TEST_CASE(BuildDupeKeyUsesSeriesIdentifierPriority)
{
	FeedItemInfo series;
	series.SetImdbId(28369634);
	series.SetRageId(12345);
	series.SetTvdbId(439270);
	series.SetTvmazeId(123456);
	series.SetSeason("S02");
	series.SetEpisode("E69");
	series.BuildDupeKey(nullptr, nullptr, nullptr, "Landarztpraxis");
	BOOST_CHECK_EQUAL(series.GetDupeKey(), "series=Landarztpraxis-S02-E69");

	FeedItemInfo rageEpisode;
	rageEpisode.SetImdbId(28369634);
	rageEpisode.SetRageId(12345);
	rageEpisode.SetTvdbId(439270);
	rageEpisode.SetTvmazeId(123456);
	rageEpisode.SetSeason("S02");
	rageEpisode.SetEpisode("E69");
	rageEpisode.BuildDupeKey(nullptr, nullptr, nullptr, nullptr);
	BOOST_CHECK_EQUAL(rageEpisode.GetDupeKey(), "rageid=12345-S02-E69");

	FeedItemInfo tvdbEpisode;
	tvdbEpisode.SetImdbId(28369634);
	tvdbEpisode.SetTvdbId(439270);
	tvdbEpisode.SetTvmazeId(123456);
	tvdbEpisode.SetSeason("S02");
	tvdbEpisode.SetEpisode("E69");
	tvdbEpisode.BuildDupeKey(nullptr, nullptr, nullptr, nullptr);
	BOOST_CHECK_EQUAL(tvdbEpisode.GetDupeKey(), "tvdbid=439270-S02-E69");

	FeedItemInfo tvmazeEpisode;
	tvmazeEpisode.SetImdbId(28369634);
	tvmazeEpisode.SetTvmazeId(123456);
	tvmazeEpisode.SetSeason("S02");
	tvmazeEpisode.SetEpisode("E69");
	tvmazeEpisode.BuildDupeKey(nullptr, nullptr, nullptr, nullptr);
	BOOST_CHECK_EQUAL(tvmazeEpisode.GetDupeKey(), "tvmazeid=123456-S02-E69");
}

BOOST_AUTO_TEST_SUITE_END()
