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


#ifndef NEWZNAB_H
#define NEWZNAB_H

#include <ctime>
#include <string>
#include <utility>
#include <vector>

/*
 * The Newznab API of an indexer (NZBHydra2 speaks it): searching for another
 * posting of a release.
 */
class Newznab
{
public:
	// one hit of a search: a posting as one indexer lists it
	struct Result
	{
		std::string title;
		std::string link;
		long long size = 0;
		int grabs = 0;
		time_t date = 0;		// usenet date, 0 if the indexer didn't give one
		std::string indexer;	// the indexer behind an aggregator
	};

	struct Page
	{
		std::vector<Result> items;
		bool error = false;		// the indexer answered with an <error>
		int errorCode = 0;
		std::string errorText;
	};

	using Params = std::vector<std::pair<std::string, std::string>>;

	// results asked per page and pages read per query
	static constexpr int PageSize = 100;
	static constexpr int MaxPages = 5;
	// queries run at the same time
	static constexpr int Parallel = 4;

	/* false when the reply isn't XML at all */
	static bool ParseResponse(const std::string& xml, Page& page);

	/* an RFC 822 date as the indexers write it ("Tue, 10 Jun 2025 01:10:05 +0000"); 0 if unreadable */
	static time_t ParseDate(const std::string& text);

	/* the queries for a release: its normalized title, a short form (title,
	 * episode or year, resolution), the short form with the release group
	 * (indexers rename releases; the group finds their postings), and a search
	 * by imdb or tvdb id when the nzb-file carries one */
	static std::vector<Params> BuildQueries(const std::string& title, const std::string& imdb,
		const std::string& tvdb);

	/* base is the api url without parameters */
	static std::string BuildUrl(const std::string& base, const Params& params, const std::string& apiKey);

	/* hides api keys and user:password pairs in a url or text meant for the log */
	static std::string Mask(const std::string& text);

	/* runs the queries (Parallel at a time), reading further pages while a
	 * page comes back full; merges the results by link, the first one wins.
	 * A failing query is logged and skipped. */
	struct SearchStats
	{
		int queries = 0;
		int failed = 0;
		int pages = 0;
	};
	static std::vector<Result> Search(const std::string& base, const std::string& apiKey,
		const std::vector<Params>& queries, time_t deadline, SearchStats* stats);
};

#endif
