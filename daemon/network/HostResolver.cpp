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

#include "HostResolver.h"

#include <algorithm>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/this_coro.hpp>

namespace network
{

Awaitable<Result<Endpoints>> ResolveHost(Executor executor, std::string_view host, int port,
	IPVersion ipVersion, std::chrono::milliseconds timeout)
{
	// A literal address is parsed locally: the system resolver can stall on it and ignore the deadline.
	ErrorCode addressEc;
	std::ignore = asio::ip::make_address(host, addressEc);
	const auto flags = addressEc
		? tcp::resolver::numeric_service
		: tcp::resolver::numeric_host | tcp::resolver::numeric_service;

	tcp::resolver resolver{executor};
	auto [ec, results] = co_await resolver.async_resolve(host, std::to_string(port), flags,
			asio::cancel_after(timeout, asio::as_tuple(asio::deferred)));

	if (ec == asio::error::operation_aborted)
	{
		const auto state = co_await asio::this_coro::cancellation_state;
		co_return Result<Endpoints>{state.cancelled() != asio::cancellation_type::none ? Error::Canceled : Error::ResolveFailed};
	}
	if (ec)
	{
		co_return Result<Endpoints>{FromAsio(ec)};
	}

	Endpoints resolved;
	for (const auto& entry : results)
	{
		resolved.push_back(entry.endpoint());
	}

	auto endpoints = OrderEndpoints(resolved, ipVersion);
	if (endpoints.empty())
	{
		co_return Result<Endpoints>{Error::ResolveFailed};
	}
	co_return Result<Endpoints>{std::move(endpoints)};
}

/** Keeps only the families allowed by `ipVersion` and interleaves them, IPv6 first, so dialing alternates families. */
Endpoints OrderEndpoints(const Endpoints& resolved, IPVersion ipVersion)
{
	Endpoints v4;
	Endpoints v6;
	for (const auto& endpoint : resolved)
	{
		if (endpoint.address().is_v4() && ipVersion != IPVersion::V6)
		{
			v4.push_back(endpoint);
		}
		else if (endpoint.address().is_v6() && ipVersion != IPVersion::V4)
		{
			v6.push_back(endpoint);
		}
	}

	Endpoints endpoints;
	endpoints.reserve(v4.size() + v6.size());
	for (size_t i = 0; i < std::max(v4.size(), v6.size()); ++i)
	{
		if (i < v6.size())
		{
			endpoints.push_back(v6[i]);
		}
		if (i < v4.size())
		{
			endpoints.push_back(v4[i]);
		}
	}
	return endpoints;
}

}
