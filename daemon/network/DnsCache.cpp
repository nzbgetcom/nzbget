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

#include <algorithm>

#include "DnsCache.h"

namespace network
{

std::optional<Endpoints> DnsCache::Find(std::string_view host, uint16_t port, IPVersion version, Clock::time_point now)
{
	std::lock_guard lock(m_mutex);
	auto it = m_entries.find(Key{std::string(host), port, version});
	if (it == m_entries.end())
	{
		return std::nullopt;
	}
	if (now >= it->second.expiresAt)
	{
		m_entries.erase(it);
		return std::nullopt;
	}
	return it->second.endpoints;
}

void DnsCache::Store(std::string_view host, uint16_t port, IPVersion version, Endpoints endpoints, Clock::time_point now)
{
	std::lock_guard lock(m_mutex);
	Key key{std::string(host), port, version};
	if (!m_entries.contains(key) && m_entries.size() >= MAX_ENTRIES)
	{
		Evict(now);
	}
	const auto ttl = endpoints.empty() ? NEGATIVE_TTL : POSITIVE_TTL;
	m_entries.insert_or_assign(std::move(key), Entry{std::move(endpoints), now + ttl, m_sequence++});
}

void DnsCache::Invalidate(std::string_view host, uint16_t port, IPVersion version)
{
	std::lock_guard lock(m_mutex);
	m_entries.erase(Key{std::string(host), port, version});
}

void DnsCache::Evict(Clock::time_point now)
{
	std::erase_if(m_entries, [now](const auto& item) { return now >= item.second.expiresAt; });
	if (m_entries.size() < MAX_ENTRIES)
	{
		return;
	}
	auto oldest = std::min_element(m_entries.begin(), m_entries.end(),
		[](const auto& a, const auto& b) { return a.second.sequence < b.second.sequence; });
	m_entries.erase(oldest);
}

}
