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

#include "Network.h"
#include "Connection.h"

#include <boost/asio/ip/address.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/version.hpp>

using namespace network;

namespace
{

bool SetPeerName(SSL* ssl, std::string_view host)
{
	X509_VERIFY_PARAM* param = SSL_get0_param(ssl);
	X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);

	ErrorCode ec;
	std::ignore = asio::ip::make_address(host, ec);
	if (!ec)
	{
		return X509_VERIFY_PARAM_set1_ip_asc(param, host.data()) == 1;
	}

	return X509_VERIFY_PARAM_set1_host(param, host.data(), host.size()) == 1;
}

}

Connection::Connection(Executor executor, Options options, std::shared_ptr<ssl::context> sslCtx)
	: m_executor{executor}
	, m_options{std::move(options)}
	, m_sslCtx{std::move(sslCtx)}
	, m_socket{MakeSocket(m_executor, m_sslCtx)}
	, m_connectTimer{m_executor}
	, m_readTimer{m_executor}
	, m_writeTimer{m_executor}
{
}

Socket Connection::MakeSocket(Executor executor, std::shared_ptr<ssl::context> sslCtx)
{
	if (sslCtx)
	{
#if BOOST_VERSION >= 109200
		// Per-stream TLS input buffer; the size argument exists only since Boost 1.92.
		constexpr size_t SSL_INPUT_BUFFER_SIZE = size_t{64} * 1024;
		return Socket(std::in_place_type<ssl::stream<tcp::socket>>,
			std::move(executor), *sslCtx, SSL_INPUT_BUFFER_SIZE);
#else
		return Socket(std::in_place_type<ssl::stream<tcp::socket>>,
			std::move(executor), *sslCtx);
#endif
	}

	return Socket(std::in_place_type<tcp::socket>, executor);
}

Connection::~Connection()
{
	ForceCloseSocket();
}

Awaitable<Result<void>> Connection::Connect(tcp::endpoint endpoint)
{
	if (m_canceled.load(std::memory_order_acquire))
	{
		co_return Error::Canceled;
	}
	if (m_state.load(std::memory_order_acquire) != State::Disconnected)
	{
		co_return Error::InvalidState;
	}

	ResetSocket();
	m_state.store(State::Connecting, std::memory_order_release);

	auto& socket = GetRawSocket();
	ErrorCode ec;
	std::ignore = socket.open(endpoint.protocol(), ec);
	if (!ec) std::ignore = socket.set_option(tcp::no_delay(m_options.tcpNoDelay), ec);
	if (!ec && m_options.tcpKeepAlive)
	{
		std::ignore = socket.set_option(asio::socket_base::keep_alive(true), ec);
	}
	if (!ec && m_options.receiveBufferSize > 0)
	{
		std::ignore = socket.set_option(
			asio::socket_base::receive_buffer_size(m_options.receiveBufferSize), ec);
	}
	if (!ec && m_options.sendBufferSize > 0)
	{
		std::ignore = socket.set_option(
			asio::socket_base::send_buffer_size(m_options.sendBufferSize), ec);
	}

	if (!ec)
	{
		ArmTimer(m_connectTimer, m_options.connectTimeout);

		std::tie(ec) = co_await socket.async_connect(
			endpoint, asio::as_tuple(asio::use_awaitable));
		DisarmTimer(m_connectTimer);
	}

	if (ec)
	{
		ForceCloseSocket();
		if (m_canceled.load(std::memory_order_acquire)) co_return Error::Canceled;
		if (m_connectTimer.timedOut) co_return Error::Timeout;
		const Error error = FromAsio(ec);
		// An OS-level timeout means the endpoint did not answer; the configured timeout is handled above.
		co_return error == Error::Timeout ? Error::ConnectFailed : error;
	}

	if (auto* stream = std::get_if<ssl::stream<tcp::socket>>(&m_socket))
	{
		m_state.store(State::Handshaking, std::memory_order_release);

		SSL* ssl = stream->native_handle();

		if (m_options.releaseSslBuffers)
		{
			::SSL_set_mode(ssl, SSL_MODE_RELEASE_BUFFERS);
		}

		ErrorCode addressEc;
		std::ignore = asio::ip::make_address(m_options.host, addressEc);
		bool isHostname = addressEc && !m_options.host.empty();
		if (m_options.enableSni && isHostname)
		{
			if (::SSL_set_tlsext_host_name(ssl, m_options.host.c_str()) != 1)
			{
				ForceCloseSocket();
				co_return Error::LocalFailure;
			}
		}

		if (m_options.verifyHostname && ::SSL_get_verify_mode(ssl) != SSL_VERIFY_NONE
			&& (m_options.host.empty() || !SetPeerName(ssl, m_options.host)))
		{
			ForceCloseSocket();
			co_return Error::ServerNameFailed;
		}

		ArmTimer(m_connectTimer, m_options.connectTimeout);

		std::tie(ec) = co_await stream->async_handshake(
			ssl::stream_base::client, asio::as_tuple(asio::use_awaitable));
		DisarmTimer(m_connectTimer);

		if (ec)
		{
			ForceCloseSocket();
			if (m_canceled.load(std::memory_order_acquire)) co_return Error::Canceled;
			if (m_connectTimer.timedOut) co_return Error::Timeout;
			co_return FromAsio(ec);
		}

		// With SSL_VERIFY_NONE the handshake succeeds and the failure is only recorded in the result.
		if (::SSL_get_verify_mode(ssl) != SSL_VERIFY_NONE && ::SSL_get_verify_result(ssl) != X509_V_OK)
		{
			ForceCloseSocket();
			co_return Error::TlsVerifyFailed;
		}
	}

	if (m_canceled.load(std::memory_order_acquire))
	{
		CloseNow();
		co_return Error::Canceled;
	}

	m_state.store(State::Connected, std::memory_order_release);
	co_return Result<void>{};
}

void Connection::Cancel() noexcept
{
	if (m_canceled.exchange(true, std::memory_order_acq_rel))
	{
		return;
	}

	auto self = shared_from_this();
	asio::post(m_executor, [self] { self->CloseNow(); });
}

void Connection::ForceCloseSocket() noexcept
{
	m_state.store(State::Disconnected, std::memory_order_release);

	auto& socket = GetRawSocket();
	if (socket.is_open())
	{
		ErrorCode ec;
		std::ignore = socket.shutdown(tcp::socket::shutdown_both, ec);
		std::ignore = socket.close(ec);
	}
}

void Connection::CloseNow() noexcept
{
	DisarmTimer(m_connectTimer);
	DisarmTimer(m_readTimer);
	DisarmTimer(m_writeTimer);
	ForceCloseSocket();
}

void Connection::ArmTimer(TimerSlot& slot, std::chrono::milliseconds timeout)
{
	slot.timedOut = false;
	if (timeout <= std::chrono::milliseconds::zero())
	{
		return;
	}

	auto self = shared_from_this();
	const auto armed = ++slot.generation;
	slot.timer.expires_after(timeout);
	slot.timer.async_wait([self, &slot, armed](ErrorCode ec)
	{
		if (ec || armed != slot.generation)
		{
			return;
		}
		slot.timedOut = true;
		self->CloseNow();
	});
}

void Connection::DisarmTimer(TimerSlot& slot) noexcept
{
	++slot.generation;
	slot.timer.cancel();
}

void Connection::ResetSocket() noexcept
{
	CloseNow();
	m_connectTimer.timedOut = false;
	m_readTimer.timedOut = false;
	m_writeTimer.timedOut = false;
	m_readPending = false;
	m_writePending = false;

	if (auto* stream = std::get_if<ssl::stream<tcp::socket>>(&m_socket))
	{
		// Reuse the stream object but start the next handshake from scratch.
		::SSL_clear(stream->native_handle());
	}
}

Awaitable<Result<void>> Connection::Close()
{
	// A cancelled caller must still finish the TLS shutdown, so the cancellation throw is suppressed here and restored after.
	const bool throwIfCancelled = co_await asio::this_coro::throw_if_cancelled();
	co_await asio::this_coro::throw_if_cancelled(false);
	auto result = co_await CloseConnection();
	co_await asio::this_coro::throw_if_cancelled(throwIfCancelled);
	co_return result;
}

/// Sends TLS close_notify within a 2 s bound, then closes the socket; a stale timer is ignored by generation.
Awaitable<Result<void>> Connection::CloseConnection()
{
	if (m_state.load(std::memory_order_acquire) == State::Disconnected)
	{
		co_return Result<void>{};
	}

	const bool connected = m_state.load(std::memory_order_acquire) == State::Connected;
	m_state.store(State::Closing, std::memory_order_release);

	ErrorCode ec;
	if (auto* stream = std::get_if<ssl::stream<tcp::socket>>(&m_socket);
		connected && stream && stream->lowest_layer().is_open())
	{
		static constexpr auto TIMEOUT = std::chrono::seconds{2};
		ArmTimer(m_writeTimer, TIMEOUT);

		std::tie(ec) = co_await stream->async_shutdown(
			asio::as_tuple(asio::use_awaitable));
		DisarmTimer(m_writeTimer);
	}

	ForceCloseSocket();

	// The peer closing first is a normal close, not a failure.
	if (!ec || ec == asio::error::eof || ec == ssl::error::stream_truncated)
	{
		co_return Result<void>{};
	}
	if (m_canceled.load(std::memory_order_acquire)) co_return Error::Canceled;
	if (m_writeTimer.timedOut) co_return Error::Timeout;

	co_return FromAsio(ec);
}

template <typename Stream>
Awaitable<Result<void>> Connection::WriteAll(
	Stream& stream, std::span<const ConstBuffer> buffers)
{
	ErrorCode ec;
	ArmTimer(m_writeTimer, m_options.writeTimeout);

	std::tie(ec, std::ignore) = co_await asio::async_write(
		stream, buffers, asio::as_tuple(asio::use_awaitable));
	DisarmTimer(m_writeTimer);

	if (ec)
	{
		if (m_canceled.load(std::memory_order_acquire)) co_return Error::Canceled;
		if (m_writeTimer.timedOut) co_return Error::Timeout;
		co_return FromAsio(ec);
	}

	co_return Result<void>{};
}

template <typename Stream>
Awaitable<Result<size_t>> Connection::ReadSome(
	Stream& stream, MutableBuffer buffer)
{
	ErrorCode ec;
	size_t n = 0;
	ArmTimer(m_readTimer, m_options.readTimeout);

	std::tie(ec, n) = co_await stream.async_read_some(
		buffer, asio::as_tuple(asio::use_awaitable));
	DisarmTimer(m_readTimer);

	if (ec)
	{
		if (m_canceled.load(std::memory_order_acquire)) co_return Error::Canceled;
		if (m_readTimer.timedOut) co_return Error::Timeout;
		co_return FromAsio(ec);
	}

	co_return n;
}

template <typename Op>
decltype(auto) Connection::Dispatch(Op&& op)
{
	return std::visit(std::forward<Op>(op), m_socket);
}

Awaitable<Result<void>> Connection::Send(ConstBuffer buffer)
{
	const std::span<const ConstBuffer> buffers(&buffer, 1);
	co_return co_await Send(buffers);
}

Awaitable<Result<void>> Connection::Send(std::span<const ConstBuffer> buffers)
{
	auto self = shared_from_this();
	if (m_state.load(std::memory_order_acquire) != State::Connected)
	{
		co_return Error::InvalidState;
	}

	if (m_writePending)
	{
		co_return Error::InvalidState;
	}
	m_writePending = true;

	Result<void> result = co_await Dispatch([&](auto& stream) { return WriteAll(stream, buffers); });

	m_writePending = false;
	if (!result)
	{
		ForceCloseSocket();
	}

	co_return result;
}

Awaitable<Result<size_t>> Connection::Read(MutableBuffer buffer)
{
	auto self = shared_from_this();
	if (m_state.load(std::memory_order_acquire) != State::Connected)
	{
		co_return Error::InvalidState;
	}

	if (m_readPending)
	{
		co_return Error::InvalidState;
	}
	m_readPending = true;

	Result<size_t> result = co_await Dispatch([&](auto& stream) { return ReadSome(stream, buffer); });

	m_readPending = false;
	if (!result)
	{
		ForceCloseSocket();
		co_return result;
	}

	co_return result.value();
}

tcp::endpoint Connection::LocalEndpoint() noexcept
{
	ErrorCode ec;
	auto endpoint = GetRawSocket().local_endpoint(ec);
	return ec ? tcp::endpoint{} : endpoint;
}

tcp::socket& Connection::GetRawSocket() noexcept
{
	if (std::holds_alternative<ssl::stream<tcp::socket>>(m_socket))
		return std::get<ssl::stream<tcp::socket>>(m_socket).next_layer();
	return std::get<tcp::socket>(m_socket);
}
