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

#ifndef NETWORK_HTTP_CLIENT_H
#define NETWORK_HTTP_CLIENT_H

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <boost/asio/cancellation_signal.hpp>

#include "Network.h"
#include "Parser.h"
#include "Url.h"

namespace network
{

class TlsContexts;
class DnsCache;

namespace http
{

/** Per-client transport settings; a request still supplies its own timeout. */
struct HttpOptions
{
	std::chrono::milliseconds connectTimeout{30000};
	std::chrono::milliseconds readTimeout{30000};
	std::chrono::milliseconds writeTimeout{30000};
	IPVersion ipVersion{IPVersion::Auto};
};

/** Parsed head plus the decoded body; @c url is the final one after redirects. */
struct HttpResponse : Response
{
	std::string body;
	Url url;
	std::string localAddress;
};

/**
 * Minimal HTTP/1.1 GET client. One instance per owner: after CancelAll() every
 * later request fails with Canceled, so a restarted component creates a new one.
 * Requests run on the client's executor; Get may be awaited from another one.
 */
class HttpClient final : public std::enable_shared_from_this<HttpClient>
{
public:
	HttpClient(Executor executor, std::shared_ptr<TlsContexts> tls, std::shared_ptr<DnsCache> dns,
		HttpOptions options = {});

	/**
	 * Runs the request on the client's executor, following redirects.
	 * @param timeout bounds the whole request, redirects included; <= 0 disables it.
	 * @param signal optional caller-owned handle for cancelling just this request.
	 */
	Awaitable<Result<HttpResponse>> Get(std::string url, std::chrono::milliseconds timeout,
		std::shared_ptr<asio::cancellation_signal> signal = nullptr);

	/**
	 * Blocking bridge for synchronous callers; replace it with Get when callers are async.
	 * Only from foreign threads: it blocks on a future the client's executor must complete,
	 * so calling it from that executor deadlocks until timeout + 1 s.
	 * Returns Canceled when the result is not ready one second after @p timeout.
	 */
	Result<HttpResponse> SyncGet(std::string url, std::chrono::milliseconds timeout);

	/** Cancels every request in flight and rejects the ones started afterwards. */
	void CancelAll() noexcept;

	/** Waits for in-flight requests to finish; a stalled resolver is the only unbounded wait. */
	void WaitIdle() noexcept;

private:
	Awaitable<Result<HttpResponse>> Run(std::string url, std::chrono::milliseconds timeout);
	Awaitable<Result<HttpResponse>> Follow(std::string url);
	Awaitable<Result<HttpResponse>> GetOnce(const Url& url);

	bool Register(const std::shared_ptr<asio::cancellation_signal>& signal);
	void Unregister(const std::shared_ptr<asio::cancellation_signal>& signal);

	Executor m_executor;
	std::shared_ptr<TlsContexts> m_tls;
	std::shared_ptr<DnsCache> m_dns;
	HttpOptions m_options;
	std::mutex m_mutex;
	std::condition_variable m_idle;
	std::vector<std::shared_ptr<asio::cancellation_signal>> m_active;
	bool m_canceled{false};
};

}

}

#endif // NETWORK_HTTP_CLIENT_H
