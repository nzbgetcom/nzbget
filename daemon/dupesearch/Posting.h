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


#ifndef POSTING_H
#define POSTING_H

#include <cstdint>
#include <string>
#include <vector>
#include "Newznab.h"

/*
 * Postings: one upload of a release to Usenet, as the indexers list it
 * (an aggregator lists one posting once per indexer) and as nzbget sees it
 * (its message-ids). Telling them apart decides what is worth fetching and
 * what is a duplicate of something already held.
 */
class Posting
{
public:
	// listings of one size posted this close together are one posting on
	// several indexers (each lists it with its own usenet date)
	static constexpr int RelistWindowSec = 120;
	// share of the articles two nzb-files may have in common before they
	// count as one posting (a re-listed posting can carry a re-uploaded segment)
	static constexpr double SamePostingShare = 0.01;
	// message-id hashes kept in a posting's sketch
	static constexpr size_t SketchSize = 64;
	// an indexer may serve another indexer's nzb-file for a listing: more than
	// this share off the listed size
	static constexpr double ListingMismatchShare = 0.02;

	using Sketch = std::vector<uint32_t>;
	using Group = std::vector<Newznab::Result>;

	/* the SketchSize smallest CRC32s of the message-ids: postings that share
	 * most of their articles share most of their sketch, distinct postings
	 * about none; small enough to keep for every posting seen */
	static Sketch MakeSketch(const std::vector<std::string>& messageIds);
	static bool SameSketch(const Sketch& a, const Sketch& b);

	/* more than SamePostingShare of the articles in common (both sorted, unique) */
	static bool SamePosting(const std::vector<std::string>& a, const std::vector<std::string>& b);

	/* the listings of one posting together; a listing without a size or a date
	 * stays alone */
	static std::vector<Group> GroupListings(std::vector<Newznab::Result> results);

	/* the indexer served an nzb-file more than ListingMismatchShare off the size it lists */
	static bool ListingMismatch(const Newznab::Result& listing, long long nzbBytes);

	/* the order to fetch postings in: each posting's listings with the most
	 * grabbed first, postings by closeness to the pick's size, then grabs,
	 * then date, one posting of each distinct size before the rest; at most
	 * maxPostings of them when that is above 0 */
	static std::vector<Group> OrderPostings(const std::vector<Newznab::Result>& candidates,
		long long pickBytes, int maxPostings);
};

#endif
