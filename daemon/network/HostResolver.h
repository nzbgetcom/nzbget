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

#ifndef NETWORK_HOST_RESOLVER_H
#define NETWORK_HOST_RESOLVER_H

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

#include "Network.h"
#include "Error.h"

namespace network
{

using Endpoints = std::vector<tcp::endpoint>;

/**
 * Resolves host:port to endpoints, IPv4 and IPv6 interleaved. Literal addresses skip DNS.
 * Keeps no state: callers that resolve the same host often cache the result themselves.
 */
Awaitable<Result<Endpoints>> ResolveHost(Executor executor, std::string_view host, int port,
	IPVersion ipVersion, std::chrono::milliseconds timeout);

/** Keeps the families allowed by @p ipVersion; Auto interleaves IPv6 first, then IPv4. */
Endpoints OrderEndpoints(const Endpoints& resolved, IPVersion ipVersion);

}

#endif // NETWORK_HOST_RESOLVER_H
