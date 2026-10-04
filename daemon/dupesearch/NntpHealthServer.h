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


#ifndef NNTPHEALTHSERVER_H
#define NNTPHEALTHSERVER_H

#include "DonorHealth.h"

/*
 * A news server of nzbget as the donor health check talks to it: through
 * the connections of nzbget's own server pool (so the server's connection
 * limit holds), at most half of them at a time (nzbget keeps the rest for
 * its downloads; the cap is shared by every check that runs).
 */
class NntpHealthServer : public DonorHealth::Server
{
public:
	// the most connections a check may hold at one server, whatever its configuration says
	static constexpr int MaxCap = 20;
	// seconds a check waits for a free connection of the pool
	static constexpr int ConnectionWaitSec = 10;

	NntpHealthServer(int serverId, int cap) : Server(cap), m_serverId(serverId) {}

	int ServerId() const { return m_serverId; }

	/* the cap for a server: half of its connections, between 1 and MaxCap */
	static int CapFor(int connections);

	/* the active servers of the pool, one per server group (the servers of a
	 * group are the same account), reusing the state of the ones already
	 * known, so pauses and caps persist across searches */
	static DonorHealth::ServerList Servers();

protected:
	std::vector<DonorHealth::Answer> Exchange(const std::vector<DonorHealth::Request>& batch) override;

private:
	int m_serverId;
};

#endif
