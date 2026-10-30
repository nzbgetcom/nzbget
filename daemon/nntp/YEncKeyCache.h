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

#ifndef YENCKEYCACHE_H
#define YENCKEYCACHE_H

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <string>
#include <functional>

// Single-entry job-owned Argon2id master-key cache.
// Deriving a 64 MiB Argon2id key costs ~15 ms per article; a 1000-article
// encrypted release would otherwise repeat the identical KDF per article.
// The cache is owned by NzbInfo (one per download job) so parallel
// ArticleDownloader threads share the derived key (the lock is held across
// the KDF: single-flight), and the key is zeroized when the job's NzbInfo
// is destroyed.
class YEncKeyCache
{
public:
	YEncKeyCache() = default;
	YEncKeyCache(const YEncKeyCache&) = delete;
	YEncKeyCache& operator=(const YEncKeyCache&) = delete;
	~YEncKeyCache();

	// Single-flight derivation; returns true and copies the 32-byte key.
	// The lock is held across the derive callback, so N parallel downloads
	// run the Argon2id KDF exactly once. A password edit cannot reuse the
	// old key: the entry is keyed by salt + password hash.
	bool GetOrDeriveKey(const uint8_t salt[16], const std::string& password,
		uint8_t outKey[32], const std::function<bool(uint8_t*)>& derive);

	// Zeroizes and drops the cached key (password change / job teardown).
	void Clear();

#ifdef NZBGET_TESTING_HOOKS
	// Observe cache occupancy without exposing it in production.
	size_t Size() { std::lock_guard<std::mutex> lock(m_mutex); return m_filled ? 1 : 0; }
#endif

private:
	// Zeroizes the entry; caller must hold m_mutex.
	void Zeroize();

	struct Entry
	{
		uint8_t salt[16] = {0};
		uint8_t key[32] = {0};
		uint8_t passwordHash[32] = {0};
	};

	std::mutex m_mutex;
	bool m_filled = false;
	Entry m_entry;
};

#endif // YENCKEYCACHE_H
