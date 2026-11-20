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

#ifndef NETWORK_DNS_CACHE_H
#define NETWORK_DNS_CACHE_H

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include "HostResolver.h"
#include "Network.h"

namespace network
{

/**
 * Short-lived host:port to endpoints cache. An empty answer is cached as a
 * failure for a shorter time. Thread-safe.
 */
class DnsCache final
{
public:
	using Clock = std::chrono::steady_clock;

	static constexpr size_t MAX_ENTRIES = 64;
	static constexpr auto POSITIVE_TTL = std::chrono::seconds{60};
	static constexpr auto NEGATIVE_TTL = std::chrono::seconds{5};

	/** Returns std::nullopt on a miss or an expired entry; a cached failure returns an empty Endpoints. */
	std::optional<Endpoints> Find(std::string_view host, uint16_t port, IPVersion version,
		Clock::time_point now = Clock::now());
	void Store(std::string_view host, uint16_t port, IPVersion version, Endpoints endpoints,
		Clock::time_point now = Clock::now());
	void Invalidate(std::string_view host, uint16_t port, IPVersion version);

private:
	using Key = std::tuple<std::string, uint16_t, IPVersion>;

	struct Entry
	{
		Endpoints endpoints;
		Clock::time_point expiresAt;
		uint64_t sequence;
	};

	void Evict(Clock::time_point now);

	std::mutex m_mutex;
	std::map<Key, Entry> m_entries;
	uint64_t m_sequence{0};
};

}

#endif // NETWORK_DNS_CACHE_H
