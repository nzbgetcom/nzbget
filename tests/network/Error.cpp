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

#include <boost/asio/error.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/system/errc.hpp>

#include "Network.h"
#include "Error.h"

using namespace network;

BOOST_AUTO_TEST_SUITE(NetworkTest)

BOOST_AUTO_TEST_CASE(Error_ToStringCoversAllValues)
{
	constexpr Error errors[] = {
		Error::None,
		Error::Canceled,
		Error::Timeout,
		Error::ConnectFailed,
		Error::ResolveFailed,
		Error::TlsHandshakeFailed,
		Error::TlsVerifyFailed,
		Error::ConnectionLost,
		Error::MessageTooLarge,
		Error::ProtocolViolation,
		Error::LocalFailure,
		Error::ResourceExhausted,
		Error::InvalidState,
		Error::Unknown,
		Error::NntpServiceUnavailable,
		Error::NntpNoSuchGroup,
		Error::NntpArticleNotFound,
		Error::NntpAuthRequired,
		Error::NntpAuthRejected,
		Error::NntpSyntaxError,
		Error::NntpPermissionDenied,
		Error::NntpUnexpectedResponse,
		Error::NntpEncryptionRequired,
		Error::NntpUnknownCommand,
		Error::NntpNoGroupSelected,
		Error::NntpNoArticleSelected,
		Error::NntpNoNextArticle,
		Error::NntpNoPreviousArticle,
		Error::HttpBadRequest,
		Error::HttpUnauthorized,
		Error::HttpForbidden,
		Error::HttpNotFound,
		Error::HttpProxyAuthentication,
		Error::HttpRequestTimeout,
		Error::HttpTooManyRequests,
		Error::HttpServerError,
		Error::HttpUnexpectedResponse,
		Error::HttpGone,
		Error::HttpPayloadTooLarge,
		Error::HttpRangeNotSatisfiable,
		Error::ServerNameFailed,
	};

	for (const Error error : errors)
	{
		if (error == Error::None)
		{
			continue;
		}

		BOOST_CHECK(!ToString(error).empty());
	}

	BOOST_CHECK(ToString(Error::None).empty());
	BOOST_CHECK(ToString(static_cast<Error>(255)) == "Unknown error");
}

BOOST_AUTO_TEST_CASE(Error_ResultCarriesErrorAndValue)
{
	Result<void> ok;
	BOOST_CHECK(ok);

	Result<void> failed{Error::Timeout};
	BOOST_CHECK(!failed);
	BOOST_CHECK(failed.error() == Error::Timeout);

	Result<size_t> value{size_t{7}};
	BOOST_CHECK(value);
	BOOST_CHECK_EQUAL(*value, 7u);

	Result<size_t> failedValue{Error::Canceled};
	BOOST_CHECK(!failedValue);
	BOOST_CHECK(failedValue.error() == Error::Canceled);
}

BOOST_AUTO_TEST_CASE(Error_FromNntpStatus)
{
	BOOST_CHECK(FromNntpStatus(200) == Error::None);
	BOOST_CHECK(FromNntpStatus(211) == Error::None);
	BOOST_CHECK(FromNntpStatus(222) == Error::None);
	BOOST_CHECK(FromNntpStatus(281) == Error::None);
	BOOST_CHECK(FromNntpStatus(400) == Error::NntpServiceUnavailable);
	BOOST_CHECK(FromNntpStatus(411) == Error::NntpNoSuchGroup);
	BOOST_CHECK(FromNntpStatus(412) == Error::NntpNoGroupSelected);
	BOOST_CHECK(FromNntpStatus(420) == Error::NntpNoArticleSelected);
	BOOST_CHECK(FromNntpStatus(421) == Error::NntpNoNextArticle);
	BOOST_CHECK(FromNntpStatus(422) == Error::NntpNoPreviousArticle);
	BOOST_CHECK(FromNntpStatus(423) == Error::NntpArticleNotFound);
	BOOST_CHECK(FromNntpStatus(430) == Error::NntpArticleNotFound);
	BOOST_CHECK(FromNntpStatus(480) == Error::NntpAuthRequired);
	BOOST_CHECK(FromNntpStatus(481) == Error::NntpAuthRejected);
	BOOST_CHECK(FromNntpStatus(482) == Error::NntpAuthRejected);
	BOOST_CHECK(FromNntpStatus(483) == Error::NntpEncryptionRequired);
	BOOST_CHECK(FromNntpStatus(499) == Error::NntpServiceUnavailable);
	BOOST_CHECK(FromNntpStatus(500) == Error::NntpUnknownCommand);
	BOOST_CHECK(FromNntpStatus(501) == Error::NntpSyntaxError);
	BOOST_CHECK(FromNntpStatus(502) == Error::NntpPermissionDenied);
	BOOST_CHECK(FromNntpStatus(503) == Error::NntpServiceUnavailable);
	BOOST_CHECK(FromNntpStatus(504) == Error::NntpUnexpectedResponse);
	// 381 is a continuation handled by the session, never a final reply.
	BOOST_CHECK(FromNntpStatus(381) == Error::NntpUnexpectedResponse);
	BOOST_CHECK(FromNntpStatus(451) == Error::NntpUnexpectedResponse);
	BOOST_CHECK(FromNntpStatus(999) == Error::NntpUnexpectedResponse);
}

BOOST_AUTO_TEST_CASE(Error_FromHttpStatus)
{
	BOOST_CHECK(FromHttpStatus(200) == Error::None);
	BOOST_CHECK(FromHttpStatus(204) == Error::None);
	BOOST_CHECK(FromHttpStatus(299) == Error::None);
	BOOST_CHECK(FromHttpStatus(301) == Error::HttpUnexpectedResponse);
	BOOST_CHECK(FromHttpStatus(302) == Error::HttpUnexpectedResponse);
	BOOST_CHECK(FromHttpStatus(304) == Error::None);
	BOOST_CHECK(FromHttpStatus(399) == Error::HttpUnexpectedResponse);
	BOOST_CHECK(FromHttpStatus(400) == Error::HttpBadRequest);
	BOOST_CHECK(FromHttpStatus(401) == Error::HttpUnauthorized);
	BOOST_CHECK(FromHttpStatus(403) == Error::HttpForbidden);
	BOOST_CHECK(FromHttpStatus(404) == Error::HttpNotFound);
	BOOST_CHECK(FromHttpStatus(407) == Error::HttpProxyAuthentication);
	BOOST_CHECK(FromHttpStatus(408) == Error::HttpRequestTimeout);
	BOOST_CHECK(FromHttpStatus(410) == Error::HttpGone);
	BOOST_CHECK(FromHttpStatus(413) == Error::HttpPayloadTooLarge);
	BOOST_CHECK(FromHttpStatus(416) == Error::HttpRangeNotSatisfiable);
	BOOST_CHECK(FromHttpStatus(429) == Error::HttpTooManyRequests);
	BOOST_CHECK(FromHttpStatus(500) == Error::HttpServerError);
	BOOST_CHECK(FromHttpStatus(503) == Error::HttpServerError);
	BOOST_CHECK(FromHttpStatus(418) == Error::HttpUnexpectedResponse);
	BOOST_CHECK(FromHttpStatus(600) == Error::HttpUnexpectedResponse);
}

BOOST_AUTO_TEST_CASE(Error_FromAsioTransport)
{
	namespace asio = boost::asio;

	BOOST_CHECK(FromAsio({}) == Error::None);
	BOOST_CHECK(FromAsio(asio::error::timed_out) == Error::Timeout);
	BOOST_CHECK(FromAsio(asio::error::operation_aborted) == Error::Canceled);
	BOOST_CHECK(FromAsio(asio::error::eof) == Error::ConnectionLost);
	BOOST_CHECK(FromAsio(asio::error::connection_reset) == Error::ConnectionLost);
	BOOST_CHECK(FromAsio(asio::error::connection_aborted) == Error::ConnectionLost);
	BOOST_CHECK(FromAsio(asio::error::network_reset) == Error::ConnectionLost);
	BOOST_CHECK(FromAsio(asio::error::broken_pipe) == Error::ConnectionLost);
	BOOST_CHECK(FromAsio(asio::error::not_connected) == Error::ConnectionLost);
	BOOST_CHECK(FromAsio(asio::error::shut_down) == Error::ConnectionLost);
	BOOST_CHECK(FromAsio(asio::error::try_again) == Error::ConnectionLost);
	BOOST_CHECK(FromAsio(asio::error::would_block) == Error::ConnectionLost);
	BOOST_CHECK(FromAsio(asio::error::interrupted) == Error::ConnectionLost);
	BOOST_CHECK(FromAsio(asio::error::connection_refused) == Error::ConnectFailed);
	BOOST_CHECK(FromAsio(asio::error::host_unreachable) == Error::ConnectFailed);
	BOOST_CHECK(FromAsio(asio::error::network_unreachable) == Error::ConnectFailed);
	BOOST_CHECK(FromAsio(asio::error::network_down) == Error::ConnectFailed);
	BOOST_CHECK(FromAsio(asio::error::host_not_found) == Error::ResolveFailed);
	BOOST_CHECK(FromAsio(asio::error::host_not_found_try_again) == Error::ResolveFailed);
	BOOST_CHECK(FromAsio(asio::error::no_data) == Error::ResolveFailed);
	BOOST_CHECK(FromAsio(asio::error::no_recovery) == Error::ResolveFailed);
	BOOST_CHECK(FromAsio(asio::error::service_not_found) == Error::ResolveFailed);
	BOOST_CHECK(FromAsio(asio::error::socket_type_not_supported) == Error::ResolveFailed);
	BOOST_CHECK(FromAsio(asio::error::message_size) == Error::MessageTooLarge);
	BOOST_CHECK(FromAsio(asio::error::no_descriptors) == Error::ResourceExhausted);
	BOOST_CHECK(FromAsio(asio::error::no_memory) == Error::ResourceExhausted);
	BOOST_CHECK(FromAsio(asio::error::no_buffer_space) == Error::ResourceExhausted);
	BOOST_CHECK(FromAsio(asio::error::fd_set_failure) == Error::ResourceExhausted);
	BOOST_CHECK(FromAsio(asio::error::invalid_argument) == Error::LocalFailure);
	BOOST_CHECK(FromAsio(asio::error::operation_not_supported) == Error::LocalFailure);
	BOOST_CHECK(FromAsio(asio::error::address_family_not_supported) == Error::LocalFailure);
	BOOST_CHECK(FromAsio(asio::error::no_protocol_option) == Error::LocalFailure);
	BOOST_CHECK(FromAsio(asio::error::address_in_use) == Error::LocalFailure);
	BOOST_CHECK(FromAsio(asio::error::no_permission) == Error::LocalFailure);
	BOOST_CHECK(FromAsio(asio::error::access_denied) == Error::LocalFailure);
	BOOST_CHECK(FromAsio(asio::error::bad_descriptor) == Error::LocalFailure);
	BOOST_CHECK(FromAsio(asio::error::not_socket) == Error::LocalFailure);
	BOOST_CHECK(FromAsio(asio::error::fault) == Error::LocalFailure);
	BOOST_CHECK(FromAsio(asio::error::name_too_long) == Error::LocalFailure);
	BOOST_CHECK(FromAsio(asio::error::already_connected) == Error::InvalidState);
	BOOST_CHECK(FromAsio(asio::error::already_started) == Error::InvalidState);
	BOOST_CHECK(FromAsio(asio::error::already_open) == Error::InvalidState);
	BOOST_CHECK(FromAsio(asio::error::in_progress) == Error::InvalidState);
	BOOST_CHECK(FromAsio(asio::error::not_found) == Error::Unknown);
	BOOST_CHECK(FromAsio(boost::system::error_code(
		12345, boost::system::system_category())) == Error::Unknown);
}

BOOST_AUTO_TEST_CASE(Error_FromAsioTlsAndFraming)
{
	namespace asio = boost::asio;

	BOOST_CHECK(FromAsio(asio::ssl::error::stream_truncated) == Error::ConnectionLost);
	BOOST_CHECK(FromAsio(boost::system::error_code(
		999, asio::ssl::error::get_stream_category())) == Error::TlsHandshakeFailed);
	BOOST_CHECK(FromAsio(boost::system::error_code(
		999, asio::error::get_ssl_category())) == Error::TlsHandshakeFailed);
	BOOST_CHECK(FromAsio(boost::system::errc::make_error_code(
		boost::system::errc::protocol_error)) == Error::ProtocolViolation);
}

BOOST_AUTO_TEST_SUITE_END()
