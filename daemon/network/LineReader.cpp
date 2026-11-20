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

#include "LineReader.h"

namespace network
{

/** Takes one LF-terminated line (CR stripped) from `buffer`; an over-long line is dropped and reported as TooLong. */
Line TakeLine(asio::streambuf& buffer, size_t maxLength)
{
	const auto bytes = buffer.data();
	const std::string_view data{static_cast<const char*>(bytes.data()), bytes.size()};
	const size_t newline = data.find('\n');
	if (newline == std::string_view::npos)
	{
		if (data.size() > maxLength + 1)
		{
			buffer.consume(data.size());
			return {LineStatus::TooLong, {}};
		}

		return {LineStatus::NeedMore, {}};
	}

	size_t length = newline;
	if (length > 0 && data[length - 1] == '\r')
	{
		--length;
	}

	const std::string_view line = data.substr(0, length);
	buffer.consume(newline + 1);

	if (line.size() > maxLength)
	{
		return {LineStatus::TooLong, {}};
	}

	return {LineStatus::Complete, line};
}

Awaitable<Result<std::string_view>> ReadLine(
	Connection& connection, asio::streambuf& buffer, size_t maxLength)
{
	constexpr size_t READ_CHUNK = 4096;

	for (;;)
	{
		const Line line = TakeLine(buffer, maxLength);
		if (line.status == LineStatus::Complete)
		{
			co_return line.text;
		}
		if (line.status == LineStatus::TooLong)
		{
			co_return Error::ProtocolViolation;
		}

		auto read = co_await connection.Read(buffer.prepare(READ_CHUNK));
		if (read.has_error())
		{
			co_return read.error();
		}
		if (read.value() == 0)
		{
			co_return Error::ConnectionLost;
		}

		buffer.commit(read.value());
	}
}

}
