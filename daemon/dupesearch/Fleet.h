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


#ifndef FLEET_H
#define FLEET_H

#include <string>
#include <vector>

/*
 * A fleet: several nzb-files a client believes are copies of one release,
 * sent at once (JSON-RPC method "appendfleet"). Nothing downloads before
 * their postings were checked on the configured news servers: the wholest
 * is queued, the others are kept in history as backups, already scored in
 * the order failover should try them. Same postings are checked once. One
 * fleet of a key at a time: a second waits for the first. With a download of
 * the key running, a fleet only adds backups, below it; a posting the key
 * holds already isn't added again. A member is dead below option DupeMinAlive.
 */
class Fleet
{
public:
	struct Member
	{
		std::string name;		// the nzb-file's name
		std::string data;		// its content (fetched first when url is set)
		std::string url;
	};
	struct Request
	{
		std::string dupeKey;
		std::string category;
		int priority = 0;
		int timeoutSec = DefaultTimeoutSec;
		std::vector<Member> members;
	};
	struct Entry
	{
		int nzbId = 0;
		std::string name;
		int rank = 0;
		int alive = -1;			// percent of the sampled articles found, -1 unknown
		std::string status;		// QUEUED, BACKUP, SAME_POSTING, DEAD, ERROR, SKIPPED, COPY
		int sameAs = 0;			// SAME_POSTING: the NZBID of the item it is a copy of (0: not added)
		int sameAsRank = 0;		// SAME_POSTING: the rank of the fleet member it is a copy of
		int member = -1;		// its index in the request
		std::string reason;
	};
	struct Result
	{
		int chosen = 0;			// the download now queued, 0 for none
		bool complete = true;	// every posting was checked within the time limit
		std::string reason;		// ALREADY_QUEUED (chosen: the key's running download); when chosen is 0:
							// ALREADY_DOWNLOADED, ALL_DEAD, NO_USABLE_MEMBERS, NO_MEMBERS, SHUTDOWN, NOT_QUEUED,
							// KEY_BUSY (another fleet of the key held it past the time limit),
							// INCOMPLETE (none found alive, but the time limit cut the check short)
		std::vector<Entry> members;	// in rank order
	};

	static constexpr int DefaultTimeoutSec = 45;
	static constexpr int MaxTimeoutSec = 120;
	static constexpr int MaxMembers = 50;

	/* checks, ranks and adds the members; blocks for up to request.timeoutSec */
	static Result Append(Request request);
};

#endif
