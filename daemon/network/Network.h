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

#ifndef NETWORK_NETWORK_H
#define NETWORK_NETWORK_H

#include <boost/asio/ssl.hpp>
#include "Types.h"
#include "Error.h"

namespace network
{

using async::Awaitable;
using async::ConstBuffer;
using async::ErrorCode;
using async::Executor;
using async::MutableBuffer;
using async::SteadyTimer;
namespace asio = boost::asio;
namespace ip   = boost::asio::ip;
namespace ssl  = boost::asio::ssl;
using tcp      = boost::asio::ip::tcp;
using Socket   = std::variant<
	tcp::socket,
	ssl::stream<tcp::socket>
>;

enum class IPVersion : uint8_t
{
	Auto,
	V4,
	V6
};

/**
 * Transport-level connection settings. A zero timeout disables the matching limit.
 */
struct Options
{
	std::string host;
	std::chrono::milliseconds connectTimeout{30000};
	std::chrono::milliseconds readTimeout{30000};
	std::chrono::milliseconds writeTimeout{30000};
	int receiveBufferSize{0}; // SO_RCVBUF in bytes (0 = OS default)
	int sendBufferSize{0};	// SO_SNDBUF in bytes (0 = OS default)
	bool enableSni{true};
	bool verifyHostname{false}; // TLS only: the certificate must match host; needs a verifying context
	bool tcpKeepAlive{true};
	bool tcpNoDelay{true};
	bool releaseSslBuffers{false};
	IPVersion ipVersion{IPVersion::Auto};
};

}

#endif // NETWORK_NETWORK_H
