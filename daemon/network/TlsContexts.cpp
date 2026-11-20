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

#include "TlsContexts.h"
#include "Log.h"

#include <openssl/err.h>
#include <openssl/ssl.h>

namespace network
{

void ConfigureVerification(SSL_CTX* ctx, X509_STORE* store, bool verify)
{
	if (store)
	{
		SSL_CTX_set1_cert_store(ctx, store);
	}
	SSL_CTX_set_verify(ctx, verify ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, nullptr);
}

namespace
{

std::shared_ptr<X509_STORE> LoadCertStore(const fs::path& location)
{
	auto locationStr = fs::u8string(location);
	if (locationStr.empty())
	{
		error("Certificate store is required when certificate check is enabled");
		return nullptr;
	}

	std::shared_ptr<X509_STORE> store(X509_STORE_new(), &X509_STORE_free);
	if (!store)
	{
		error("Could not create certificate store");
		return nullptr;
	}

	fs::error_code ec;
	const bool isDirectory = fs::is_directory(location, ec);
	if (ec)
	{
		error("Could not inspect certificate store %s: %s", locationStr.c_str(), ec.message().c_str());
		return nullptr;
	}

	const bool loaded = isDirectory
		? X509_STORE_load_locations(store.get(), nullptr, locationStr.c_str()) == 1
		: X509_STORE_load_locations(store.get(), locationStr.c_str(), nullptr) == 1;
	if (!loaded)
	{
		error("Could not load certificate store from %s", locationStr.c_str());
		return nullptr;
	}

	return store;
}

std::shared_ptr<ssl::context> BuildContext(X509_STORE* store, bool verify)
{
	auto context = std::make_shared<ssl::context>(ssl::context::tls_client);
	context->set_options(ssl::context::no_sslv3 | ssl::context::no_tlsv1 | ssl::context::no_tlsv1_1);
	ConfigureVerification(context->native_handle(), store, verify);
	return context;
}

bool ApplyCipher(SSL_CTX* ctx, std::string_view cipher)
{
	const std::string name(cipher);
	const bool tls13 = SSL_CTX_set_ciphersuites(ctx, name.c_str()) == 1;
	const bool legacy = SSL_CTX_set_cipher_list(ctx, name.c_str()) == 1;
	ERR_clear_error();
	return tls13 || legacy;
}

}

TlsContexts::TlsContexts(Config config)
	: m_certCheck(config.certCheck)
{
	if (OPENSSL_init_ssl(0, nullptr) == 0)
	{
		error("OpenSSL initialization failed");
	}

	if (m_certCheck)
	{
		m_certStore = LoadCertStore(config.certStore);
		if (!m_certStore)
		{
			error("Certificate check is enabled but the certificate store is unavailable; TLS connections will fail");
		}
	}
}

Result<std::shared_ptr<ssl::context>> TlsContexts::Get(CertVerification verification, std::string_view cipher)
{
	const bool verify = m_certCheck && verification != CertVerification::None;
	if (verify && !m_certStore)
	{
		return Error::TlsVerifyFailed;
	}

	std::lock_guard lock(m_mutex);
	Key key{verification, std::string(cipher)};
	if (auto it = m_contexts.find(key); it != m_contexts.end())
	{
		return it->second;
	}

	auto context = BuildContext(verify ? m_certStore.get() : nullptr, verify);
	if (!cipher.empty() && !ApplyCipher(context->native_handle(), cipher))
	{
		error("Could not set TLS cipher suite %s", key.second.c_str());
		return Error::LocalFailure;
	}

	m_contexts.emplace(std::move(key), context);
	return context;
}

}
