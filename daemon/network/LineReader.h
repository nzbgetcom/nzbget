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

#ifndef NETWORK_LINEREADER_H
#define NETWORK_LINEREADER_H

#include <boost/asio/streambuf.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "Connection.h"

namespace network
{

enum class LineStatus : uint8_t
{
	Complete,
	NeedMore,
	TooLong
};

struct Line
{
	LineStatus status;
	std::string_view text;
};

/**
 * Extracts one line from buffered data. Accepts LF and CRLF; a CR immediately
 * before LF is stripped. On Complete the line and its terminator are consumed.
 * The returned view is valid until the next asio::streambuf::prepare().
 */
Line TakeLine(asio::streambuf& buffer, size_t maxLength);

/**
 * Reads one line from the connection, filling `buffer` as needed.
 * ProtocolViolation when the line exceeds maxLength; ConnectionLost on EOF.
 */
Awaitable<Result<std::string_view>> ReadLine(
	Connection& connection, asio::streambuf& buffer, size_t maxLength);

}

#endif // NETWORK_LINEREADER_H
