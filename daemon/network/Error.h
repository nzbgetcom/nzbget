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

#ifndef NETWORK_ERROR_H
#define NETWORK_ERROR_H

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

#include <boost/asio/error.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/system/errc.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>

namespace network
{

enum class Error : uint8_t
{
	None = 0x00,
	Canceled = 0x01,
	Timeout = 0x02,
	ConnectFailed = 0x03,
	ResolveFailed = 0x04,
	TlsHandshakeFailed = 0x05,
	TlsVerifyFailed = 0x06,
	ConnectionLost = 0x07,
	MessageTooLarge = 0x08,
	ProtocolViolation = 0x09,
	LocalFailure = 0x0A,
	ResourceExhausted = 0x0B,
	InvalidState = 0x0C,
	Unknown = 0x1F,

	NntpServiceUnavailable = 0x20,
	NntpNoSuchGroup = 0x21,
	NntpArticleNotFound = 0x22,
	NntpAuthRequired = 0x23,
	NntpAuthRejected = 0x24,
	NntpSyntaxError = 0x25,
	NntpPermissionDenied = 0x26,
	NntpUnexpectedResponse = 0x27,
	NntpEncryptionRequired = 0x28,
	NntpUnknownCommand = 0x29,
	NntpNoGroupSelected = 0x2A,
	NntpNoArticleSelected = 0x2B,
	NntpNoNextArticle = 0x2C,
	NntpNoPreviousArticle = 0x2D,

	HttpBadRequest = 0x40,
	HttpUnauthorized = 0x41,
	HttpForbidden = 0x42,
	HttpNotFound = 0x43,
	HttpProxyAuthentication = 0x44,
	HttpRequestTimeout = 0x45,
	HttpTooManyRequests = 0x46,
	HttpServerError = 0x47,
	HttpUnexpectedResponse = 0x48,
	HttpGone = 0x49,
	HttpPayloadTooLarge = 0x4A,
	HttpRangeNotSatisfiable = 0x4B,

	ServerNameFailed = 0x50
};

/**
 * Result<T> carries either a value or a network::Error.
 *
 * Note: constructing from an Error always yields a failure, even for
 * Error::None. Use Result<T>{} to report success.
 */
template <typename T>
using Result = boost::system::result<T, Error>;

inline constexpr std::string_view ToString(Error error) noexcept
{
	switch (error)
	{
		case Error::None: return "";
		case Error::Canceled: return "Operation canceled";
		case Error::Timeout: return "Operation timed out";
		case Error::ConnectFailed: return "Connection failed";
		case Error::ResolveFailed: return "Host resolve failed";
		case Error::TlsHandshakeFailed: return "TLS handshake failed";
		case Error::TlsVerifyFailed: return "TLS certificate verification failed";
		case Error::ConnectionLost: return "Connection lost";
		case Error::MessageTooLarge: return "Message too large";
		case Error::ProtocolViolation: return "Protocol violation";
		case Error::LocalFailure: return "Local configuration or socket error";
		case Error::ResourceExhausted: return "System resource exhausted";
		case Error::InvalidState: return "Invalid socket or client state";
		case Error::Unknown: return "Unknown error";

		case Error::NntpServiceUnavailable: return "NNTP service unavailable (400/499/503)";
		case Error::NntpNoSuchGroup: return "No such newsgroup (411)";
		case Error::NntpArticleNotFound: return "No such article (423/430)";
		case Error::NntpAuthRequired: return "Authentication required (480)";
		case Error::NntpAuthRejected: return "Authentication rejected (481/482)";
		case Error::NntpSyntaxError: return "NNTP command syntax error (501)";
		case Error::NntpPermissionDenied: return "NNTP access denied (502)";
		case Error::NntpUnexpectedResponse: return "Unexpected NNTP response";
		case Error::NntpEncryptionRequired: return "NNTP encryption required (483)";
		case Error::NntpUnknownCommand: return "NNTP command not recognized (500)";
		case Error::NntpNoGroupSelected: return "No newsgroup selected (412)";
		case Error::NntpNoArticleSelected: return "No article selected (420)";
		case Error::NntpNoNextArticle: return "No next article (421)";
		case Error::NntpNoPreviousArticle: return "No previous article (422)";

		case Error::HttpBadRequest: return "HTTP bad request (400)";
		case Error::HttpUnauthorized: return "HTTP unauthorized (401)";
		case Error::HttpForbidden: return "HTTP forbidden (403)";
		case Error::HttpNotFound: return "HTTP not found (404)";
		case Error::HttpProxyAuthentication: return "HTTP proxy authentication required (407)";
		case Error::HttpRequestTimeout: return "HTTP request timeout (408)";
		case Error::HttpTooManyRequests: return "HTTP too many requests (429)";
		case Error::HttpServerError: return "HTTP server error (5xx)";
		case Error::HttpUnexpectedResponse: return "Unexpected HTTP response";
		case Error::HttpGone: return "HTTP gone (410)";
		case Error::HttpPayloadTooLarge: return "HTTP payload too large (413)";
		case Error::HttpRangeNotSatisfiable: return "HTTP range not satisfiable (416)";

		case Error::ServerNameFailed: return "Cannot set TLS server name for certificate verification";
		default: return "Unknown error";
	}
}

/**
 * Translates an Asio error code into the network vocabulary.
 */
inline Error FromAsio(boost::system::error_code ec) noexcept
{
	namespace asio = boost::asio;

	if (!ec)
	{
		return Error::None;
	}

	if (ec == asio::error::operation_aborted)
	{
		return Error::Canceled;
	}

	if (ec == asio::error::timed_out)
	{
		return Error::Timeout;
	}

	if (ec == asio::error::host_not_found
		|| ec == asio::error::host_not_found_try_again
		|| ec == asio::error::no_data
		|| ec == asio::error::no_recovery
		|| ec == asio::error::service_not_found
		|| ec == asio::error::socket_type_not_supported)
	{
		return Error::ResolveFailed;
	}

	if (ec == asio::ssl::error::stream_truncated)
	{
		return Error::ConnectionLost;
	}

	if (ec.category() == asio::ssl::error::get_stream_category()
		|| ec.category() == asio::error::get_ssl_category())
	{
		return Error::TlsHandshakeFailed;
	}

	if (ec == asio::error::eof
		|| ec == asio::error::connection_reset
		|| ec == asio::error::connection_aborted
		|| ec == asio::error::network_reset
		|| ec == asio::error::broken_pipe
		|| ec == asio::error::not_connected
		|| ec == asio::error::shut_down
		|| ec == asio::error::try_again
		|| ec == asio::error::would_block
		|| ec == asio::error::interrupted)
	{
		return Error::ConnectionLost;
	}

	if (ec == asio::error::connection_refused
		|| ec == asio::error::host_unreachable
		|| ec == asio::error::network_unreachable
		|| ec == asio::error::network_down)
	{
		return Error::ConnectFailed;
	}

	if (ec == asio::error::message_size)
	{
		return Error::MessageTooLarge;
	}

	if (ec == asio::error::no_descriptors
		|| ec == asio::error::no_memory
		|| ec == asio::error::no_buffer_space
		|| ec == asio::error::fd_set_failure)
	{
		return Error::ResourceExhausted;
	}

	if (ec == boost::system::errc::make_error_code(boost::system::errc::protocol_error))
	{
		return Error::ProtocolViolation;
	}

	if (ec == asio::error::invalid_argument
		|| ec == asio::error::operation_not_supported
		|| ec == asio::error::address_family_not_supported
		|| ec == asio::error::no_protocol_option
		|| ec == asio::error::address_in_use
		|| ec == asio::error::no_permission
		|| ec == asio::error::access_denied
		|| ec == asio::error::bad_descriptor
		|| ec == asio::error::not_socket
		|| ec == asio::error::fault
		|| ec == asio::error::name_too_long)
	{
		return Error::LocalFailure;
	}

	if (ec == asio::error::already_connected
		|| ec == asio::error::already_started
		|| ec == asio::error::already_open
		|| ec == asio::error::in_progress)
	{
		return Error::InvalidState;
	}

	return Error::Unknown;
}

/**
 * Classifies a final NNTP reply.
 *
 * 3xx continuations (340, 341, 381) belong to the command flow and must be
 * handled by the session before calling this helper.
 */
inline Error FromNntpStatus(int status) noexcept
{
	if (status >= 200 && status < 300)
	{
		return Error::None;
	}

	switch (status)
	{
		case 400: return Error::NntpServiceUnavailable;
		case 411: return Error::NntpNoSuchGroup;
		case 412: return Error::NntpNoGroupSelected;
		case 420: return Error::NntpNoArticleSelected;
		case 421: return Error::NntpNoNextArticle;
		case 422: return Error::NntpNoPreviousArticle;
		case 423:
		case 430: return Error::NntpArticleNotFound;
		case 480: return Error::NntpAuthRequired;
		case 481:
		case 482: return Error::NntpAuthRejected;
		case 483: return Error::NntpEncryptionRequired;
		case 499: return Error::NntpServiceUnavailable;
		case 500: return Error::NntpUnknownCommand;
		case 501: return Error::NntpSyntaxError;
		case 502: return Error::NntpPermissionDenied;
		case 503: return Error::NntpServiceUnavailable;
		default:  return Error::NntpUnexpectedResponse;
	}
}

/**
 * Classifies a final HTTP status. 304 is a successful conditional-GET outcome.
 */
inline Error FromHttpStatus(unsigned status) noexcept
{
	if ((status >= 200 && status < 300) || status == 304)
	{
		return Error::None;
	}

	switch (status)
	{
		case 400: return Error::HttpBadRequest;
		case 401: return Error::HttpUnauthorized;
		case 403: return Error::HttpForbidden;
		case 404: return Error::HttpNotFound;
		case 407: return Error::HttpProxyAuthentication;
		case 408: return Error::HttpRequestTimeout;
		case 410: return Error::HttpGone;
		case 413: return Error::HttpPayloadTooLarge;
		case 416: return Error::HttpRangeNotSatisfiable;
		case 429: return Error::HttpTooManyRequests;
		default:
			return status >= 500 && status < 600
				? Error::HttpServerError
				: Error::HttpUnexpectedResponse;
	}
}

[[noreturn]] inline void throw_exception_from_error(Error error, boost::source_location const&)
{
	throw std::runtime_error(std::string(ToString(error)));
}

} // namespace network

#endif // NETWORK_ERROR_H
