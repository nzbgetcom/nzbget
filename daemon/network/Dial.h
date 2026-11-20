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

#ifndef NETWORK_DIAL_H
#define NETWORK_DIAL_H

#include <functional>
#include <memory>

#include "Connection.h"
#include "HostResolver.h"

namespace network
{

using DialAttempt = std::function<Awaitable<Result<std::shared_ptr<Connection>>>(Executor, tcp::endpoint)>;

/**
 * Happy Eyeballs (RFC 8305): endpoints start one after another, each 250 ms after the previous one.
 * The first connected endpoint wins and the others are cancelled. TLS when @p tls is set.
 * A TLS error is reported only if no endpoint connects.
 */
Awaitable<Result<std::shared_ptr<Connection>>> Dial(Executor executor, Endpoints endpoints,
	Options options, std::shared_ptr<ssl::context> tls);

namespace detail
{

/** The race behind Dial, with the connect step supplied by @p attempt. Exposed for tests. */
Awaitable<Result<std::shared_ptr<Connection>>> DialUsing(Executor executor, Endpoints endpoints,
	DialAttempt attempt);

}

}

#endif // NETWORK_DIAL_H
