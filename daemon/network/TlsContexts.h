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

#ifndef NETWORK_TLS_CONTEXTS_H
#define NETWORK_TLS_CONTEXTS_H

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include <openssl/x509.h>

#include "FileSystem.h"
#include "Error.h"
#include "Network.h"

namespace network
{

/** Mirrors Options::ECertVerifLevel (daemon/main/Options.h) without depending on it. */
enum class CertVerification
{
	None,    // no verification at all
	Minimal, // verify the chain, not the hostname
	Strict   // verify the chain and that the hostname matches the certificate
};

/** Attaches the trust store (if any) and sets peer verification on a client context. */
void ConfigureVerification(SSL_CTX* ctx, X509_STORE* store, bool verify);

/**
 * Owns the CA trust store and client TLS contexts, created lazily per
 * (verification, cipher). Thread-safe; contexts are cached for the lifetime of
 * this object.
 */
class TlsContexts final
{
public:
	struct Config
	{
		fs::path certStore; // CA file or directory; required when certCheck is enabled
		bool certCheck{true};
	};

	explicit TlsContexts(Config config);

	TlsContexts(const TlsContexts&) = delete;
	TlsContexts& operator=(const TlsContexts&) = delete;

	/** Returns the cached context for the key, creating it on first use. */
	Result<std::shared_ptr<ssl::context>> Get(CertVerification verification, std::string_view cipher = {});
	std::shared_ptr<X509_STORE> GetCertStore() const noexcept { return m_certStore; }

private:
	using Key = std::pair<CertVerification, std::string>;

	bool m_certCheck{true};
	std::shared_ptr<X509_STORE> m_certStore;
	std::mutex m_mutex;
	std::map<Key, std::shared_ptr<ssl::context>> m_contexts;
};

}

#endif // NETWORK_TLS_CONTEXTS_H
