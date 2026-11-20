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

#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

#include "daemon/network/Connection.h"
#include "daemon/network/LineReader.h"

using namespace network;
using namespace std::chrono_literals;

BOOST_AUTO_TEST_SUITE(NetworkTest)

namespace
{

template <typename Awaitable>
auto RunBlocking(asio::io_context& ctx, Awaitable&& awaitable)
{
	ctx.restart();
	auto fut = asio::co_spawn(ctx, std::forward<Awaitable>(awaitable), asio::use_future);
	std::thread stopper{[&ctx, &fut] { fut.wait(); ctx.stop(); }};
	ctx.run();
	stopper.join();
	return fut.get();
}

Options TestOptions()
{
	Options options;
	options.connectTimeout = 2s;
	options.readTimeout = 2s;
	options.writeTimeout = 2s;
	return options;
}

std::string_view View(const asio::streambuf& buffer)
{
	const auto bytes = buffer.data();
	return {static_cast<const char*>(bytes.data()), bytes.size()};
}

/// Writes "one\r", waits, then writes the rest: the first read has no complete line.
Awaitable<void> WriteSplitLine(tcp::acceptor& acceptor)
{
	auto socket = co_await acceptor.async_accept(asio::use_awaitable);
	co_await asio::async_write(socket, asio::buffer("one\r", 4), asio::use_awaitable);

	asio::steady_timer timer(co_await asio::this_coro::executor);
	timer.expires_after(20ms);
	co_await timer.async_wait(asio::use_awaitable);

	co_await asio::async_write(socket, asio::buffer("\nnext\r\n", 7), asio::use_awaitable);
}

/// Writes a line longer than the reader's limit and keeps the connection open.
Awaitable<void> WriteLongLine(tcp::acceptor& acceptor)
{
	auto socket = co_await acceptor.async_accept(asio::use_awaitable);
	std::string line(32, 'a');
	co_await asio::async_write(socket, asio::buffer(line), asio::use_awaitable);

	asio::steady_timer timer(co_await asio::this_coro::executor);
	timer.expires_after(1s);
	co_await timer.async_wait(asio::use_awaitable);
}

/// Accepts a connection and sends nothing.
Awaitable<void> HoldOpen(tcp::acceptor& acceptor)
{
	auto socket = co_await acceptor.async_accept(asio::use_awaitable);

	asio::steady_timer timer(co_await asio::this_coro::executor);
	timer.expires_after(1s);
	co_await timer.async_wait(asio::use_awaitable);
}

/// Writes an incomplete line and closes the connection.
Awaitable<void> WriteThenClose(tcp::acceptor& acceptor)
{
	auto socket = co_await acceptor.async_accept(asio::use_awaitable);
	co_await asio::async_write(socket, asio::buffer("partial", 7), asio::use_awaitable);
	socket.close();
}

template <typename Server>
Result<std::string_view> ReadOneLine(asio::io_context& ctx, tcp::acceptor& acceptor,
	asio::streambuf& buffer, size_t maxLength, Server server,
	std::chrono::milliseconds readTimeout = 2s)
{
	asio::co_spawn(ctx, server(acceptor), asio::detached);

	Options options = TestOptions();
	options.readTimeout = readTimeout;

	return RunBlocking(ctx, [&]() -> Awaitable<Result<std::string_view>>
	{
		auto conn = std::make_shared<Connection>(ctx.get_executor(), options, nullptr);
		auto connected = co_await conn->Connect(tcp::endpoint(
			asio::ip::make_address("127.0.0.1"), acceptor.local_endpoint().port()));
		if (connected.has_error())
		{
			co_return connected.error();
		}

		auto line = co_await ReadLine(*conn, buffer, maxLength);
		co_await conn->Close();
		co_return line;
	});
}

}

BOOST_AUTO_TEST_CASE(LineReader_TakeLineHandlesLfCrlfEmptyAndIncomplete)
{
	asio::streambuf buffer;
	constexpr std::string_view input = "one\r\ntwo\n\r\nthree";

	auto space = buffer.prepare(input.size());
	BOOST_REQUIRE(space.size() >= input.size());
	std::memcpy(space.data(), input.data(), input.size());
	buffer.commit(input.size());

	auto line = TakeLine(buffer, 16);
	BOOST_REQUIRE(line.status == LineStatus::Complete);
	BOOST_CHECK(line.text == "one");

	line = TakeLine(buffer, 16);
	BOOST_REQUIRE(line.status == LineStatus::Complete);
	BOOST_CHECK(line.text == "two");

	line = TakeLine(buffer, 16);
	BOOST_REQUIRE(line.status == LineStatus::Complete);
	BOOST_CHECK(line.text.empty());

	line = TakeLine(buffer, 16);
	BOOST_CHECK(line.status == LineStatus::NeedMore);
	BOOST_CHECK(line.text.empty());
	BOOST_CHECK(View(buffer) == "three");
}

BOOST_AUTO_TEST_CASE(LineReader_TakeLineTooLong)
{
	asio::streambuf buffer;
	constexpr std::string_view complete = "0123456789\r\n";

	auto space = buffer.prepare(complete.size());
	BOOST_REQUIRE(space.size() >= complete.size());
	std::memcpy(space.data(), complete.data(), complete.size());
	buffer.commit(complete.size());

	auto line = TakeLine(buffer, 4);
	BOOST_CHECK(line.status == LineStatus::TooLong);
	BOOST_CHECK(line.text.empty());
	BOOST_CHECK(buffer.size() == 0);

	constexpr std::string_view incomplete = "0123456789";
	space = buffer.prepare(incomplete.size());
	BOOST_REQUIRE(space.size() >= incomplete.size());
	std::memcpy(space.data(), incomplete.data(), incomplete.size());
	buffer.commit(incomplete.size());

	line = TakeLine(buffer, 4);
	BOOST_CHECK(line.status == LineStatus::TooLong);
	BOOST_CHECK(buffer.size() == 0);
}

BOOST_AUTO_TEST_CASE(LineReader_ReadLineSplitAcrossReads)
{
	asio::io_context ctx;
	tcp::acceptor acceptor(ctx, tcp::endpoint(tcp::v4(), 0));

	asio::streambuf buffer;
	auto result = ReadOneLine(ctx, acceptor, buffer, 64, WriteSplitLine);

	BOOST_REQUIRE(result);
	BOOST_CHECK(result.value() == "one");
	BOOST_CHECK(View(buffer) == "next\r\n");
}

BOOST_AUTO_TEST_CASE(LineReader_ReadLineTooLong)
{
	asio::io_context ctx;
	tcp::acceptor acceptor(ctx, tcp::endpoint(tcp::v4(), 0));

	asio::streambuf buffer;
	auto result = ReadOneLine(ctx, acceptor, buffer, 8, WriteLongLine);

	BOOST_REQUIRE(!result);
	BOOST_CHECK(result.error() == Error::ProtocolViolation);
}

BOOST_AUTO_TEST_CASE(LineReader_ReadLineTimesOut)
{
	asio::io_context ctx;
	tcp::acceptor acceptor(ctx, tcp::endpoint(tcp::v4(), 0));

	asio::streambuf buffer;
	auto result = ReadOneLine(ctx, acceptor, buffer, 64, HoldOpen, 50ms);

	BOOST_REQUIRE(!result);
	BOOST_CHECK(result.error() == Error::Timeout);
}

BOOST_AUTO_TEST_CASE(LineReader_ReadLineConnectionLost)
{
	asio::io_context ctx;
	tcp::acceptor acceptor(ctx, tcp::endpoint(tcp::v4(), 0));

	asio::streambuf buffer;
	auto result = ReadOneLine(ctx, acceptor, buffer, 64, WriteThenClose);

	BOOST_REQUIRE(!result);
	BOOST_CHECK(result.error() == Error::ConnectionLost);
}

BOOST_AUTO_TEST_SUITE_END()
