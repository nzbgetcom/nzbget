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

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <functional>
#include <thread>

#include "daemon/network/Connection.h"

using namespace network;
using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(NetworkTest)

namespace
{

template <typename Awaitable>
auto RunBlocking(asio::io_context& ctx, Awaitable&& awaitable)
{
	ctx.restart(); // ctx stays stopped after an earlier RunBlocking on the same context.
	auto fut = asio::co_spawn(ctx, std::forward<Awaitable>(awaitable), asio::use_future);
	std::thread stopper{[&ctx, &fut] { fut.wait(); ctx.stop(); }};
	ctx.run();
	stopper.join();
	return fut.get();
}

Options TestOptions()
{
	Options options;
	// Windows retries a refused loopback SYN for ~2 s before reporting it; keep headroom.
	options.connectTimeout = 5s;
	options.readTimeout = 2s;
	options.writeTimeout = 2s;
	return options;
}

/// Accepts one connection, echoes back whatever it reads once.
Awaitable<void> EchoOnce(tcp::acceptor& acceptor)
{
	auto socket = co_await acceptor.async_accept(asio::use_awaitable);
	char buf[64];
	size_t n = co_await socket.async_read_some(asio::buffer(buf), asio::use_awaitable);
	co_await asio::async_write(socket, asio::buffer(buf, n), asio::use_awaitable);
}

/// Cancels `connection` after `delay`, from the connection's own executor.
Awaitable<void> CancelAfter(Connection& connection, std::chrono::milliseconds delay)
{
	asio::steady_timer timer(co_await asio::this_coro::executor);
	timer.expires_after(delay);
	co_await timer.async_wait(asio::use_awaitable);
	connection.Cancel();
}

/// Closes a disconnected connection, then gets cancelled: the next await must still throw.
Awaitable<bool> CloseThenAwaitAfterCancel(Connection& connection, asio::cancellation_signal& signal)
{
	co_await connection.Close();

	asio::steady_timer timer(co_await asio::this_coro::executor);
	timer.expires_after(1s);
	// Emit after the wait is armed: a cancellation signal does not remember past emissions.
	asio::post(timer.get_executor(), [&signal] { signal.emit(asio::cancellation_type::terminal); });
	try
	{
		co_await timer.async_wait(asio::bind_cancellation_slot(signal.slot(), asio::use_awaitable));
		co_return false;
	}
	catch (const boost::system::system_error& e)
	{
		co_return e.code() == asio::error::operation_aborted;
	}
}

/// Accepts one connection and drops it without a word.
Awaitable<void> AcceptOnly(tcp::acceptor& acceptor)
{
	auto socket = co_await acceptor.async_accept(asio::use_awaitable);
}

/// Accepts one connection, writes a short tail and closes.
Awaitable<void> WriteThenClose(tcp::acceptor& acceptor)
{
	auto socket = co_await acceptor.async_accept(asio::use_awaitable);
	co_await asio::async_write(socket, asio::buffer("tail", 4), asio::use_awaitable);
	socket.close();
}

/// Accepts one connection and reads exactly `total` bytes back to it.
Awaitable<void> EchoExactly(tcp::acceptor& acceptor, size_t total)
{
	auto socket = co_await acceptor.async_accept(asio::use_awaitable);
	std::string data(total, '\0');
	size_t got = 0;
	while (got < total)
	{
		got += co_await socket.async_read_some(
			asio::buffer(data.data() + got, total - got), asio::use_awaitable);
	}
	co_await asio::async_write(socket, asio::buffer(data), asio::use_awaitable);
}

}

BOOST_AUTO_TEST_CASE(Connection_ConnectSendReadClose_RoundTrip)
{
	asio::io_context ctx;
	tcp::acceptor acceptor(ctx, tcp::endpoint(tcp::v4(), 0));
	const auto port = acceptor.local_endpoint().port();
	asio::co_spawn(ctx, EchoOnce(acceptor), asio::detached);

	Options options = TestOptions();
	auto conn = std::make_shared<Connection>(ctx.get_executor(), options, nullptr);

	auto result = RunBlocking(ctx, [&]() -> Awaitable<Result<void>> {
		auto connectResult = co_await conn->Connect(
			tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));
		if (!connectResult) co_return connectResult;

		BOOST_CHECK(conn->IsConnected());

		const char message[] = "ping";
		auto sendResult = co_await conn->Send(ConstBuffer(message, sizeof(message) - 1));
		if (!sendResult) co_return sendResult;

		char reply[64] = {};
		auto readResult = co_await conn->Read(MutableBuffer(reply, sizeof(reply)));
		if (!readResult) co_return readResult.error();

		BOOST_CHECK_EQUAL(std::string(reply, readResult.value()), "ping");

		co_return co_await conn->Close();
	}());

	BOOST_REQUIRE(result);
	BOOST_CHECK(!conn->IsConnected());
}

BOOST_AUTO_TEST_CASE(Connection_Send_WhenNotConnected_ReturnsInvalidState)
{
	asio::io_context ctx;
	Options options = TestOptions();
	auto conn = std::make_shared<Connection>(ctx.get_executor(), options, nullptr);

	const char message[] = "x";
	auto result = RunBlocking(ctx, conn->Send(ConstBuffer(message, 1)));

	BOOST_CHECK(!result);
	BOOST_CHECK(result.error() == Error::InvalidState);
}

BOOST_AUTO_TEST_CASE(Connection_Read_WhenNotConnected_ReturnsInvalidState)
{
	asio::io_context ctx;
	Options options = TestOptions();
	auto conn = std::make_shared<Connection>(ctx.get_executor(), options, nullptr);

	char buf[1];
	auto result = RunBlocking(ctx, conn->Read(MutableBuffer(buf, sizeof(buf))));

	BOOST_CHECK(!result);
	BOOST_CHECK(result.error() == Error::InvalidState);
}

BOOST_AUTO_TEST_CASE(Connection_Connect_Timeout_ReturnsTimeout)
{
	asio::io_context ctx;
	Options options = TestOptions();
	options.connectTimeout = 100ms;
	auto conn = std::make_shared<Connection>(ctx.get_executor(), options, nullptr);

	// RFC 5737 TEST-NET-1: reserved, never routed, no SYN-ACK will arrive.
	auto result = RunBlocking(ctx, conn->Connect(
		tcp::endpoint(asio::ip::make_address("192.0.2.1"), 81)));

	BOOST_CHECK(!result);
	BOOST_CHECK(result.error() == Error::Timeout);
	BOOST_CHECK(!conn->IsConnected());
}

BOOST_AUTO_TEST_CASE(Connection_Cancel_ForceClosesAndResetsState)
{
	asio::io_context ctx;
	tcp::acceptor acceptor(ctx, tcp::endpoint(tcp::v4(), 0));
	const auto port = acceptor.local_endpoint().port();
	asio::co_spawn(ctx, AcceptOnly(acceptor), asio::detached);

	Options options = TestOptions();
	auto conn = std::make_shared<Connection>(ctx.get_executor(), options, nullptr);

	auto result = RunBlocking(ctx, conn->Connect(
		tcp::endpoint(asio::ip::make_address("127.0.0.1"), port)));
	BOOST_REQUIRE(result);
	BOOST_CHECK(conn->IsConnected());

	conn->Cancel();
	ctx.restart();
	ctx.run();

	BOOST_CHECK(!conn->IsConnected());
}

BOOST_AUTO_TEST_CASE(Connection_Send_GathersMultipleBuffers)
{
	asio::io_context ctx;
	tcp::acceptor acceptor(ctx, tcp::endpoint(tcp::v4(), 0));
	const auto port = acceptor.local_endpoint().port();
	asio::co_spawn(ctx, EchoExactly(acceptor, 8), asio::detached);

	Options options = TestOptions();
	auto conn = std::make_shared<Connection>(ctx.get_executor(), options, nullptr);

	auto result = RunBlocking(ctx, [&]() -> Awaitable<Result<void>> {
		auto connectResult = co_await conn->Connect(
			tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));
		if (!connectResult) co_return connectResult;

		const char first[] = "ping";
		const char second[] = "pong";
		const ConstBuffer buffers[] = {
			ConstBuffer(first, sizeof(first) - 1),
			ConstBuffer(second, sizeof(second) - 1),
		};
		auto sendResult = co_await conn->Send(std::span<const ConstBuffer>(buffers));
		if (!sendResult) co_return sendResult;

		char reply[64] = {};
		auto readResult = co_await conn->Read(MutableBuffer(reply, sizeof(reply)));
		if (!readResult) co_return readResult.error();

		BOOST_CHECK_EQUAL(std::string(reply, readResult.value()), "pingpong");
		co_return co_await conn->Close();
	}());

	BOOST_REQUIRE(result);
}

BOOST_AUTO_TEST_CASE(Connection_Read_PeerCloseAfterData_DeliversDataThenConnectionLost)
{
	asio::io_context ctx;
	tcp::acceptor acceptor(ctx, tcp::endpoint(tcp::v4(), 0));
	const auto port = acceptor.local_endpoint().port();
	asio::co_spawn(ctx, WriteThenClose(acceptor), asio::detached);

	Options options = TestOptions();
	auto conn = std::make_shared<Connection>(ctx.get_executor(), options, nullptr);

	auto result = RunBlocking(ctx, [&]() -> Awaitable<Result<void>> {
		auto connectResult = co_await conn->Connect(
			tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));
		if (!connectResult) co_return connectResult;

		std::string received;
		for (;;)
		{
			char buf[16];
			auto readResult = co_await conn->Read(MutableBuffer(buf, sizeof(buf)));
			if (!readResult)
			{
				BOOST_CHECK(readResult.error() == Error::ConnectionLost);
				break;
			}
			received.append(buf, readResult.value());
		}

		BOOST_CHECK_EQUAL(received, "tail");
		co_return Result<void>{};
	}());

	BOOST_REQUIRE(result);
	BOOST_CHECK(!conn->IsConnected());
}


/// Accepts one connection and keeps it open.
Awaitable<void> HoldOnce(tcp::acceptor& acceptor)
{
	auto socket = co_await acceptor.async_accept(asio::use_awaitable);
	asio::steady_timer keepAlive(co_await asio::this_coro::executor);
	keepAlive.expires_after(std::chrono::seconds{5});
	co_await keepAlive.async_wait(asio::use_awaitable);
}

/// Accepts one connection and drops it.
Awaitable<void> CloseOnce(tcp::acceptor& acceptor)
{
	auto socket = co_await acceptor.async_accept(asio::use_awaitable);
	socket.close();
}

// Exhaustive transition table for the public API:
//
//   Disconnected + Send/Read          -> InvalidState,  Disconnected
//   Disconnected + Close              -> success,       Disconnected
//   Disconnected + Connect (refused)  -> ConnectFailed, Disconnected
//   Disconnected + Connect (timeout)  -> Timeout,       Disconnected
//   Connecting   + Cancel             -> Canceled,      Disconnected
//   Connected    + Connect            -> InvalidState,  Connected
//   Connected    + Read (peer closed) -> ConnectionLost, Disconnected
//   Connected    + Read (timeout)     -> Timeout,       Disconnected
//   Connected    + Close              -> success,       Disconnected
BOOST_AUTO_TEST_CASE(Connection_StateMachine_Transitions)
{
	using State = Connection::State;
	using Action = std::function<Awaitable<Result<void>>(Connection&, uint16_t)>;

	const auto run = [](const char* name, Options options,
		Awaitable<void> (*server)(tcp::acceptor&), const Action& action,
		State expectedState, Error expectedError)
	{
		asio::io_context ctx;
		tcp::acceptor acceptor(ctx, tcp::endpoint(tcp::v4(), 0));
		if (server) asio::co_spawn(ctx, server(acceptor), asio::detached);

		auto conn = std::make_shared<Connection>(ctx.get_executor(), std::move(options), nullptr);
		auto result = RunBlocking(ctx, action(*conn, acceptor.local_endpoint().port()));

		BOOST_TEST_CONTEXT(name)
		{
			BOOST_CHECK_EQUAL(static_cast<int>(expectedError),
				static_cast<int>(result ? Error::None : result.error()));
			BOOST_CHECK_EQUAL(static_cast<int>(expectedState),
				static_cast<int>(conn->GetState()));
		}
	};

	const auto endpoint = [](uint16_t port)
	{
		return tcp::endpoint(asio::ip::make_address("127.0.0.1"), port);
	};

	const Action sendOne = [](Connection& c, uint16_t) -> Awaitable<Result<void>>
	{
		const char data[] = "x";
		co_return co_await c.Send(ConstBuffer(data, 1));
	};

	const Action readOne = [](Connection& c, uint16_t) -> Awaitable<Result<void>>
	{
		char buf[8];
		const auto r = co_await c.Read(MutableBuffer(buf, sizeof(buf)));
		if (!r) co_return r.error();
		co_return Result<void>{};
	};

	const Action closeConn = [](Connection& c, uint16_t) -> Awaitable<Result<void>>
	{
		co_return co_await c.Close();
	};

	const Action connectOnce = [endpoint](Connection& c, uint16_t port) -> Awaitable<Result<void>>
	{
		co_return co_await c.Connect(endpoint(port));
	};

	const Action connectTwice = [endpoint](Connection& c, uint16_t port) -> Awaitable<Result<void>>
	{
		const auto first = co_await c.Connect(endpoint(port));
		if (!first) co_return first;
		co_return co_await c.Connect(endpoint(port));
	};

	const Action connectThenClose = [endpoint](Connection& c, uint16_t port) -> Awaitable<Result<void>>
	{
		const auto first = co_await c.Connect(endpoint(port));
		if (!first) co_return first;
		co_return co_await c.Close();
	};

	const Action connectThenRead = [endpoint](Connection& c, uint16_t port) -> Awaitable<Result<void>>
	{
		const auto first = co_await c.Connect(endpoint(port));
		if (!first) co_return first;
		char buf[8];
		const auto r = co_await c.Read(MutableBuffer(buf, sizeof(buf)));
		if (!r) co_return r.error();
		co_return Result<void>{};
	};

	run("Disconnected.Send", TestOptions(), nullptr, sendOne,
		State::Disconnected, Error::InvalidState);
	run("Disconnected.Read", TestOptions(), nullptr, readOne,
		State::Disconnected, Error::InvalidState);
	run("Disconnected.Close", TestOptions(), nullptr, closeConn,
		State::Disconnected, Error::None);

	run("Connected.Connect", TestOptions(), HoldOnce, connectTwice,
		State::Connected, Error::InvalidState);
	run("Connected.Close", TestOptions(), HoldOnce, connectThenClose,
		State::Disconnected, Error::None);
	run("Connected.Read.PeerClose", TestOptions(), CloseOnce, connectThenRead,
		State::Disconnected, Error::ConnectionLost);

	Options readTimeout = TestOptions();
	readTimeout.readTimeout = 100ms;
	run("Connected.Read.Timeout", std::move(readTimeout), HoldOnce, connectThenRead,
		State::Disconnected, Error::Timeout);

	// Refused: bind then close the listener, so nothing is listening on the port.
	{
		asio::io_context ctx;
		tcp::acceptor acceptor(ctx, tcp::endpoint(tcp::v4(), 0));
		const uint16_t port = acceptor.local_endpoint().port();
		acceptor.close();

		auto conn = std::make_shared<Connection>(ctx.get_executor(), TestOptions(), nullptr);
		const auto result = RunBlocking(ctx, conn->Connect(endpoint(port)));

		BOOST_TEST_CONTEXT("Disconnected.Connect.Refused")
		{
			BOOST_REQUIRE(!result);
			BOOST_CHECK_MESSAGE(result.error() == Error::ConnectFailed,
				"unexpected error: " << network::ToString(result.error())
					<< " (" << static_cast<int>(result.error()) << ")");
			BOOST_CHECK(conn->GetState() == State::Disconnected);
		}
	}

	// Timeout: TEST-NET-1 is never routed, so no SYN-ACK arrives.
	{
		asio::io_context ctx;
		Options options = TestOptions();
		options.connectTimeout = 100ms;
		auto conn = std::make_shared<Connection>(ctx.get_executor(), std::move(options), nullptr);
		const auto result = RunBlocking(ctx,
			conn->Connect(tcp::endpoint(asio::ip::make_address("192.0.2.1"), 81)));

		BOOST_TEST_CONTEXT("Disconnected.Connect.Timeout")
		{
			BOOST_REQUIRE(!result);
			BOOST_CHECK(result.error() == Error::Timeout);
			BOOST_CHECK(conn->GetState() == State::Disconnected);
		}
	}

	// Cancel while the connect is still pending.
	{
		asio::io_context ctx;
		Options options = TestOptions();
		options.connectTimeout = 5s;
		auto conn = std::make_shared<Connection>(ctx.get_executor(), std::move(options), nullptr);

		asio::co_spawn(ctx, CancelAfter(*conn, 50ms), asio::detached);

		const auto result = RunBlocking(ctx,
			conn->Connect(tcp::endpoint(asio::ip::make_address("192.0.2.1"), 81)));

		BOOST_TEST_CONTEXT("Connecting.Cancel")
		{
			BOOST_REQUIRE(!result);
			BOOST_CHECK(result.error() == Error::Canceled);
			BOOST_CHECK(conn->GetState() == State::Disconnected);
		}
	}
}

BOOST_AUTO_TEST_CASE(ConnectionCloseKeepsCancellationForCaller)
{
	asio::io_context ctx;
	auto conn = std::make_shared<Connection>(ctx.get_executor(), TestOptions(), nullptr);
	asio::cancellation_signal signal;

	auto fut = asio::co_spawn(ctx, CloseThenAwaitAfterCancel(*conn, signal), asio::use_future);
	ctx.run();

	BOOST_CHECK(fut.get());
}

BOOST_AUTO_TEST_CASE(Connection_CancelDuringRead_ReturnsCanceledNotTimeout)
{
	asio::io_context ctx;
	tcp::acceptor acceptor(ctx, tcp::endpoint(tcp::v4(), 0));
	const auto port = acceptor.local_endpoint().port();
	asio::co_spawn(ctx, HoldOnce(acceptor), asio::detached);

	Options options = TestOptions();
	options.readTimeout = 5s;
	auto conn = std::make_shared<Connection>(ctx.get_executor(), options, nullptr);
	auto connected = RunBlocking(ctx, conn->Connect(
		tcp::endpoint(asio::ip::make_address("127.0.0.1"), port)));
	BOOST_REQUIRE(connected);

	asio::co_spawn(ctx, CancelAfter(*conn, 50ms), asio::detached);
	char buf[8];
	auto result = RunBlocking(ctx, conn->Read(MutableBuffer(buf, sizeof(buf))));

	BOOST_REQUIRE(!result);
	BOOST_CHECK(result.error() == Error::Canceled);
	BOOST_CHECK(!conn->IsConnected());
}

BOOST_AUTO_TEST_CASE(Connection_CancelFromForeignThread_ReturnsCanceled)
{
	asio::io_context ctx;
	tcp::acceptor acceptor(ctx, tcp::endpoint(tcp::v4(), 0));
	const auto port = acceptor.local_endpoint().port();
	asio::co_spawn(ctx, HoldOnce(acceptor), asio::detached);

	Options options = TestOptions();
	options.readTimeout = 5s;
	auto conn = std::make_shared<Connection>(ctx.get_executor(), options, nullptr);
	auto connected = RunBlocking(ctx, conn->Connect(
		tcp::endpoint(asio::ip::make_address("127.0.0.1"), port)));
	BOOST_REQUIRE(connected);

	std::thread canceller{[conn]
	{
		std::this_thread::sleep_for(50ms);
		conn->Cancel();
	}};
	char buf[8];
	auto result = RunBlocking(ctx, conn->Read(MutableBuffer(buf, sizeof(buf))));
	canceller.join();

	BOOST_REQUIRE(!result);
	BOOST_CHECK(result.error() == Error::Canceled);
	BOOST_CHECK(!conn->IsConnected());
}

BOOST_AUTO_TEST_SUITE_END()
