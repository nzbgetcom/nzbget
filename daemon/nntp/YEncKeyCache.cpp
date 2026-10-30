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
#include "YEncKeyCache.h"

#include <sodium.h>

YEncKeyCache::~YEncKeyCache()
{
	Clear();
}

void YEncKeyCache::Clear()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	Zeroize();
}

void YEncKeyCache::Zeroize()
{
	if (m_filled)
	{
		sodium_memzero(m_entry.key, sizeof(m_entry.key));
		sodium_memzero(m_entry.passwordHash, sizeof(m_entry.passwordHash));
		m_filled = false;
	}
}

// The lock is held across lookup, KDF and insertion: one atomic single-flight
// operation (prevents N parallel 64 MiB Argon2id runs for the same salt).
bool YEncKeyCache::GetOrDeriveKey(const uint8_t salt[16], const std::string& password,
	uint8_t outKey[32], const std::function<bool(uint8_t*)>& derive)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	uint8_t passwordHash[32];
	crypto_hash_sha256(passwordHash, reinterpret_cast<const unsigned char*>(password.data()), password.size());
	if (m_filled && memcmp(m_entry.salt, salt, 16) == 0 &&
		memcmp(m_entry.passwordHash, passwordHash, 32) == 0)
	{
		memcpy(outKey, m_entry.key, 32);
		return true;
	}
	if (!derive(outKey))
	{
		sodium_memzero(passwordHash, sizeof(passwordHash));
		sodium_memzero(outKey, 32);
		return false;
	}
	Zeroize();
	memcpy(m_entry.salt, salt, 16);
	memcpy(m_entry.passwordHash, passwordHash, 32);
	memcpy(m_entry.key, outKey, 32);
	m_filled = true;
	sodium_memzero(passwordHash, sizeof(passwordHash));
	return true;
}
