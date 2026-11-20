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

#include "NetworkComponent.h"
#include "DnsCache.h"
#include "http/HttpClient.h"

namespace network
{

NetworkComponent::NetworkComponent(Config config, async::Executor executor)
	: m_tlsContexts(std::make_shared<TlsContexts>(std::move(config))),
	  m_httpClient(std::make_shared<http::HttpClient>(executor, m_tlsContexts, std::make_shared<DnsCache>()))
{
}

void NetworkComponent::Stop() noexcept
{
	if (!m_httpClient)
	{
		return;
	}
	m_httpClient->CancelAll();
	// A blocking DNS lookup can only end on its own, so this waits at most as long as the OS resolver.
	m_httpClient->WaitIdle();
	m_httpClient.reset();
}

}
