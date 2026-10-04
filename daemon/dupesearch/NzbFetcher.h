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


#ifndef NZBFETCHER_H
#define NZBFETCHER_H

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include "Newznab.h"
#include "NzbReader.h"
#include "Posting.h"

/*
 * Downloads nzb-files from an indexer for the duplicate search. Every
 * download costs a grab of the indexer's daily limit, so it is careful:
 * one download in flight per indexer, one retry after a failure, and an
 * indexer that answers 403 or 429 twice in a row is at its grab limit and
 * isn't asked again for CooldownSec (kept across restarts: the limits
 * last for hours). One posting that several indexers list is downloaded
 * once, from the next listing only if a download fails.
 */
class NzbFetcher
{
public:
	enum EReason
	{
		frOk,
		frFetch,		// the download failed
		frParse,		// not an nzb-file (an error answer, an empty body, HTML)
		frRefused		// the indexer is at its grab limit: not asked
	};

	struct Fetched
	{
		Newznab::Result listing;
		EReason reason = frFetch;
		std::string data;
		NzbSummary info;
	};

	// what happened to the listings of the postings fetched
	struct Stats
	{
		int ok = 0;
		int fetch = 0;
		int parse = 0;
		int refused = 0;
		int relisted = 0;			// listings of a fetched posting that weren't needed
		int listingMismatch = 0;	// postings only an nzb-file of another size was served for
		void Add(const Stats& other);
	};

	static constexpr int CooldownSec = 30 * 60;
	static constexpr int RetryDelaySec = 2;
	static constexpr size_t MaxNzbBytes = 64 * 1024 * 1024;

	/* the file the cooldowns are kept in (empty: not kept) */
	void SetStatePath(const std::string& path);
	void Load();

	/* downloads the nzb-file of one listing; the data of an nzb-file is returned only for frOk */
	Fetched Fetch(const Newznab::Result& listing, time_t deadline, int retries = 1);

	/* one nzb-file of a posting: its listings in turn until one serves an
	 * nzb-file of its listed size. A listing that serves another size (an
	 * indexer may serve another indexer's nzb-file) is kept only if no
	 * listing serves the posting itself. */
	Fetched FetchPosting(const Posting::Group& listings, time_t deadline, Stats& stats);

	bool IsRefused(const std::string& indexer);

private:
	std::mutex m_mutex;
	std::map<std::string, std::unique_ptr<std::mutex>> m_indexerLocks;
	std::map<std::string, time_t> m_refusedUntil;
	std::string m_statePath;

	std::mutex& IndexerLock(const std::string& indexer);
	void Refuse(const std::string& indexer);
	void Save();
};

#endif
