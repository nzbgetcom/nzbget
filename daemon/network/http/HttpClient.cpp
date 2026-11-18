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

#include <future>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_future.hpp>

#include "HttpClient.h"
#include "Dial.h"
#include "DnsCache.h"
#include "HostResolver.h"
#include "TlsContexts.h"
#include "Util.h"

using namespace network;
using namespace network::http;

namespace
{

constexpr int MAX_REDIRECTS = 5;
constexpr std::string_view USER_AGENT = "nzbget";
constexpr size_t READ_CHUNK_SIZE = size_t{16} * 1024;

bool IsRedirect(unsigned status)
{
	return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}

/** Copies decoded body bytes; the parser itself only hands out views. */
struct StreamSink final : BodySink
{
	explicit StreamSink(std::string& target) : out{target} {}

	std::string& out;

	Result<void> OnBody(std::string_view data) override
	{
		out.append(data);
		return {};
	}
};

/** Maps the exception being handled to an Error; call only from a catch block. */
Error ClassifyCurrentException()
{
	try
	{
		throw;
	}
	catch (const boost::system::system_error& e)
	{
		const Error error = FromAsio(e.code());
		return error == Error::Unknown ? Error::LocalFailure : error;
	}
	catch (const std::bad_alloc&)
	{
		return Error::ResourceExhausted;
	}
	catch (...)
	{
		return Error::LocalFailure;
	}
}

}

HttpClient::HttpClient(Executor executor, std::shared_ptr<TlsContexts> tls, std::shared_ptr<DnsCache> dns,
	HttpOptions options)
	: m_executor{std::move(executor)}
	, m_tls{std::move(tls)}
	, m_dns{std::move(dns)}
	, m_options{options}
{
}

Result<HttpResponse> HttpClient::SyncGet(std::string url, std::chrono::milliseconds timeout)
{
	// Get keeps its own copy of the signal in its frame, so it stays valid after the early return below.
	auto signal = std::make_shared<asio::cancellation_signal>();
	auto future = asio::co_spawn(m_executor, Get(std::move(url), timeout, signal), asio::use_future);

	if (timeout <= std::chrono::milliseconds::zero())
	{
		future.wait();
	}
	else if (future.wait_for(timeout + std::chrono::seconds{1}) != std::future_status::ready)
	{
		asio::post(m_executor, [signal] { signal->emit(asio::cancellation_type::all); });
		return Error::Canceled;
	}

	try
	{
		return future.get();
	}
	catch (...)
	{
		return ClassifyCurrentException();
	}
}

Awaitable<Result<HttpResponse>> HttpClient::Get(std::string url, std::chrono::milliseconds timeout,
	std::shared_ptr<asio::cancellation_signal> signal)
{
	auto request = signal ? std::move(signal) : std::make_shared<asio::cancellation_signal>();
	if (!Register(request))
	{
		co_return Error::Canceled;
	}

	Result<HttpResponse> result = Error::LocalFailure;
	try
	{
		result = co_await asio::co_spawn(m_executor, Run(std::move(url), timeout),
			asio::bind_cancellation_slot(request->slot(), asio::use_awaitable));
	}
	catch (...)
	{
		result = ClassifyCurrentException();
	}

	Unregister(request);
	co_return result;
}

void HttpClient::CancelAll() noexcept
{
	std::vector<std::shared_ptr<asio::cancellation_signal>> active;
	{
		std::lock_guard lock(m_mutex);
		m_canceled = true;
		active = m_active;
	}

	// asio::cancellation_signal is not thread-safe: its handler is installed on
	// the executor thread, so the emit has to run there as well.
	auto signals = std::make_shared<std::vector<std::shared_ptr<asio::cancellation_signal>>>(std::move(active));
	try
	{
		asio::post(m_executor, [signals]
		{
			for (const auto& signal : *signals)
			{
				signal->emit(asio::cancellation_type::all);
			}
		});
	}
	catch (...)
	{
		// Out of memory while queueing: the requests still end by their own timeout.
	}
}

void HttpClient::WaitIdle() noexcept
{
	std::unique_lock lock(m_mutex);
	m_idle.wait(lock, [this] { return m_active.empty(); });
}

bool HttpClient::Register(const std::shared_ptr<asio::cancellation_signal>& signal)
{
	std::lock_guard lock(m_mutex);
	if (m_canceled)
	{
		return false;
	}
	m_active.push_back(signal);
	return true;
}

void HttpClient::Unregister(const std::shared_ptr<asio::cancellation_signal>& signal)
{
	std::lock_guard lock(m_mutex);
	std::erase(m_active, signal);
	if (m_active.empty())
	{
		m_idle.notify_all();
	}
}

Awaitable<Result<HttpResponse>> HttpClient::Run(std::string url, std::chrono::milliseconds timeout)
{
	if (timeout <= std::chrono::milliseconds::zero())
	{
		co_return co_await Follow(std::move(url));
	}

	Result<HttpResponse> result = Error::Canceled;
	try
	{
		// The two std::move(url) calls are in mutually exclusive branches.
		// cppcheck-suppress accessMoved
		result = co_await asio::co_spawn(m_executor, Follow(std::move(url)),
			asio::cancel_after(timeout, asio::use_awaitable));
	}
	catch (const boost::system::system_error& e)
	{
		if (e.code() != asio::error::operation_aborted)
		{
			co_return FromAsio(e.code());
		}
	}
	catch (...)
	{
		co_return ClassifyCurrentException();
	}

	// Canceled means either the request deadline or the caller's slot.
	if (result.has_error() && result.error() == Error::Canceled)
	{
		const auto state = co_await asio::this_coro::cancellation_state;
		if (state.cancelled() == asio::cancellation_type::none)
		{
			co_return Error::Timeout;
		}
	}
	co_return result;
}

// The coroutine owns the URL across suspension, so it must be taken by value.
// cppcheck-suppress passedByValue
Awaitable<Result<HttpResponse>> HttpClient::Follow(std::string text)
{
	try
	{
		auto parsed = ParseUrl(text);
		if (parsed.has_error())
		{
			co_return parsed.error();
		}

		Url url = std::move(parsed.value());
		for (int redirects = 0;; ++redirects)
		{
			auto response = co_await GetOnce(url);
			if (response.has_error() || !IsRedirect(response.value().status))
			{
				co_return response;
			}

			const auto* location = response.value().Header("location");
			if (!location || redirects >= MAX_REDIRECTS)
			{
				co_return Error::HttpUnexpectedResponse;
			}

			auto next = ResolveUrl(url, *location);
			if (next.has_error() || (url.tls && !next.value().tls))
			{
				co_return Error::HttpUnexpectedResponse;
			}
			url = std::move(next.value());
		}
	}
	catch (...)
	{
		co_return ClassifyCurrentException();
	}
}

Awaitable<Result<HttpResponse>> HttpClient::GetOnce(const Url& url)
{
	std::shared_ptr<ssl::context> tls;
	if (url.tls)
	{
		auto context = m_tls->Get(CertVerification::Strict);
		if (context.has_error())
		{
			co_return context.error();
		}
		tls = std::move(context.value());
	}

	Options options;
	options.host = url.host;
	options.verifyHostname = url.tls;
	options.connectTimeout = m_options.connectTimeout;
	options.readTimeout = m_options.readTimeout;
	options.writeTimeout = m_options.writeTimeout;
	options.ipVersion = m_options.ipVersion;
	const auto version = m_options.ipVersion;
	const auto port = static_cast<uint16_t>(url.port);

	Endpoints endpoints;
	if (auto cached = m_dns->Find(url.host, port, version))
	{
		endpoints = std::move(*cached);
		if (endpoints.empty())
		{
			co_return Error::ResolveFailed;
		}
	}
	else
	{
		auto resolved = co_await ResolveHost(m_executor, url.host, url.port, version, options.connectTimeout);
		if (resolved.has_error())
		{
			m_dns->Store(url.host, port, version, Endpoints{});
			co_return resolved.error();
		}
		m_dns->Store(url.host, port, version, resolved.value());
		endpoints = std::move(resolved.value());
	}

	auto dialed = co_await Dial(m_executor, std::move(endpoints), std::move(options), std::move(tls));
	if (dialed.has_error())
	{
		m_dns->Invalidate(url.host, port, version);
		co_return dialed.error();
	}
	auto connection = std::move(dialed.value());
	std::string localAddress = connection->LocalEndpoint().address().to_string();

	std::string request = std::format(
		"GET {} HTTP/1.1\r\n"
		"Host: {}\r\n"
		"User-Agent: {}\r\n"
		"Accept: */*\r\n"
		"Connection: close\r\n\r\n",
		url.target, url.HostHeader(), USER_AGENT
	);
	if (auto sent = co_await connection->Send(asio::buffer(request)); sent.has_error())
	{
		co_return sent.error();
	}

	HttpResponse response;
	StreamSink sink{response.body};
	Parser parser;
	std::array<char, READ_CHUNK_SIZE> chunk;
	for (;;)
	{
		auto read = co_await connection->Read(asio::buffer(chunk));
		if (read.has_error())
		{
			// A close-delimited body ends with the connection; anything else is truncation.
			if (!parser.Finish().has_value())
			{
				co_return read.error();
			}
			break;
		}

		if (auto fed = parser.Feed({chunk.data(), read.value()}, sink); fed.has_error())
		{
			co_return fed.error();
		}
		if (parser.Done())
		{
			break;
		}
	}

	static_cast<Response&>(response) = std::move(parser.Head());
	std::erase_if(response.headers, [](const auto& header)
	{
		return Util::StrCaseCmp(header.first, "transfer-encoding");
	});

	const auto* contentEncoding = response.Header("content-encoding");
	if (contentEncoding && !IsRedirect(response.status) && !Util::StrCaseCmp(Util::TrimView(*contentEncoding), "identity"))
	{
		co_return Error::HttpUnexpectedResponse;
	}

	response.url = url;
	response.localAddress = std::move(localAddress);
	std::ignore = co_await connection->Close();
	co_return response;
}
