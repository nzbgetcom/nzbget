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

#ifndef NETWORK_COMPONENT_H
#define NETWORK_COMPONENT_H

#include <memory>

#include "Component.h"
#include "Types.h"
#include "TlsContexts.h"

namespace network
{

namespace http
{
class HttpClient;
}

/// Owns the HTTP client with its TLS contexts and DNS cache; acquired in the constructor, released in Stop().
class NetworkComponent final : public async::Component
{
public:
	using Config = TlsContexts::Config;

	NetworkComponent(Config config, async::Executor executor);

	std::shared_ptr<http::HttpClient> GetHttpClient() const noexcept { return m_httpClient; }

	/// Trust store from Config, shared with the blocking TLS stack; null when certificate checking is off.
	std::shared_ptr<X509_STORE> GetCertStore() const noexcept { return m_tlsContexts->GetCertStore(); }

	/// Cancels in-flight HTTP requests and waits for them to finish; there is no deadline, the second stop signal is the escape.
	void Stop() noexcept override;

private:
	std::shared_ptr<TlsContexts> m_tlsContexts;
	std::shared_ptr<http::HttpClient> m_httpClient;
};

}

namespace async
{
template<> struct ComponentTraits<network::NetworkComponent>
{
	static constexpr ComponentId Id = ComponentId::Network;
	static constexpr std::string_view Name = "network";
};
}

#endif // NETWORK_COMPONENT_H
