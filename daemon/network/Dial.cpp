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

#include "Dial.h"

#include <boost/asio/experimental/cancellation_condition.hpp>
#include <boost/asio/experimental/parallel_group.hpp>

#include <exception>
#include <utility>

namespace network
{

namespace
{

// RFC 8305 default delay; make it configurable when a server needs a different value.
constexpr std::chrono::milliseconds ATTEMPT_DELAY{250};

/// Waits out the stagger delay, then connects. A cancelled delay is reported as Canceled, not as a connect failure.
Awaitable<std::shared_ptr<Connection>> RunAttempt(Executor executor, const DialAttempt& attempt,
	tcp::endpoint endpoint, std::chrono::milliseconds delay)
{
	if (delay > std::chrono::milliseconds::zero())
	{
		SteadyTimer timer(executor);
		timer.expires_after(delay);
		auto [ec] = co_await timer.async_wait(asio::as_tuple(asio::use_awaitable));
		if (ec)
		{
			throw Error::Canceled;
		}
	}

	auto connected = co_await attempt(executor, endpoint);
	if (connected.has_error())
	{
		throw connected.error();
	}
	co_return std::move(connected.value());
}

/// Lazy: the attempt is spawned only when the parallel group awaits it, so `delay` is measured from that moment.
auto StartAttempt(Executor executor, const DialAttempt& attempt, tcp::endpoint endpoint,
	std::chrono::milliseconds delay)
{
	return asio::co_spawn(executor, RunAttempt(executor, attempt, endpoint, delay), asio::deferred);
}

using AttemptOp = decltype(StartAttempt(std::declval<Executor>(), std::declval<const DialAttempt&>(),
	std::declval<tcp::endpoint>(), std::chrono::milliseconds{}));

Awaitable<Result<std::shared_ptr<Connection>>> ConnectOne(Executor executor, tcp::endpoint endpoint,
	Options options, std::shared_ptr<ssl::context> tls)
{
	auto connection = std::make_shared<Connection>(executor, std::move(options), std::move(tls));
	auto connected = co_await connection->Connect(endpoint);
	if (connected.has_error())
	{
		co_return connected.error();
	}
	co_return std::move(connection);
}

/// TLS errors win: another address will not fix a bad certificate, so they explain the failure best.
Error ChooseError(const std::vector<std::exception_ptr>& errors)
{
	Error chosen = Error::ConnectFailed;
	for (const auto& error : errors)
	{
		if (!error)
		{
			continue;
		}
		try
		{
			std::rethrow_exception(error);
		}
		catch (Error error)
		{
			if (error == Error::TlsVerifyFailed || error == Error::TlsHandshakeFailed)
			{
				return error;
			}
			// Other per-endpoint errors explain the dial failure no better than ConnectFailed.
			if (error == Error::Canceled || error == Error::Timeout)
			{
				chosen = error;
			}
		}
		catch (const boost::system::system_error& e)
		{
			if (e.code() == asio::error::operation_aborted)
			{
				return Error::Canceled;
			}
		}
	}
	return chosen;
}

}

namespace detail
{

// Coroutine parameters must own their data: the frame outlives the caller's temporaries.
// cppcheck-suppress passedByValue
Awaitable<Result<std::shared_ptr<Connection>>> DialUsing(Executor executor, Endpoints endpoints,
	DialAttempt attempt)
{
	if (endpoints.empty())
	{
		co_return Error::ConnectFailed;
	}

	std::vector<AttemptOp> attempts;
	attempts.reserve(endpoints.size());
	for (size_t i = 0; i < endpoints.size(); ++i)
	{
		attempts.push_back(StartAttempt(executor, attempt, endpoints[i], ATTEMPT_DELAY * static_cast<int>(i)));
	}

	// `attempts` is not used after the move.
	// cppcheck-suppress accessMoved
	auto [order, errors, connections] = co_await asio::experimental::make_parallel_group(std::move(attempts))
		.async_wait(asio::experimental::wait_for_one_success(), asio::use_awaitable);

	for (size_t index : order)
	{
		if (!errors[index])
		{
			co_return std::move(connections[index]);
		}
	}
	co_return ChooseError(errors);
}

}

Awaitable<Result<std::shared_ptr<Connection>>> Dial(Executor executor, Endpoints endpoints,
	// The coroutine frame outlives the caller's temporaries, so the options are owned by value.
	// cppcheck-suppress passedByValue
	Options options, std::shared_ptr<ssl::context> tls)
{
	co_return co_await detail::DialUsing(executor, std::move(endpoints),
		[options, tls](Executor attemptExecutor, tcp::endpoint endpoint)
		{
			return ConnectOne(attemptExecutor, endpoint, options, tls);
		});
}

}
