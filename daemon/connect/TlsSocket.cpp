/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2008-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2024 Denis <denis@nzbget.com>
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

#ifndef DISABLE_TLS

#include "TlsSocket.h"
#include "Thread.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"
#include "Options.h"

std::string TlsSocket::m_certStore;
X509_STORE* TlsSocket::m_X509Store = nullptr;

#ifndef CRYPTO_set_locking_callback
#define NEED_CRYPTO_LOCKING
#endif

#ifdef NEED_CRYPTO_LOCKING

/**
 * Mutexes for OpenSSL
 */

std::vector<std::unique_ptr<Mutex>> g_OpenSSLMutexes;

static void openssl_locking(int mode, int n, const char* file, int line)
{
	Mutex* mutex = g_OpenSSLMutexes[n].get();
	if (mode & CRYPTO_LOCK)
	{
		mutex->Lock();
	}
	else
	{
		mutex->Unlock();
	}
}

static struct CRYPTO_dynlock_value* openssl_dynlock_create(const char *file, int line)
{
	return (CRYPTO_dynlock_value*)new Mutex();
}

static void openssl_dynlock_destroy(struct CRYPTO_dynlock_value *l, const char *file, int line)
{
	Mutex* mutex = (Mutex*)l;
	delete mutex;
}

static void openssl_dynlock_lock(int mode, struct CRYPTO_dynlock_value *l, const char *file, int line)
{
	Mutex* mutex = (Mutex*)l;
	if (mode & CRYPTO_LOCK)
	{
		mutex->Lock();
	}
	else
	{
		mutex->Unlock();
	}
}

#endif /* NEED_CRYPTO_LOCKING */


void TlsSocket::Init()
{
	debug("Initializing TLS library");


#ifdef NEED_CRYPTO_LOCKING
	for (int i = 0, num = CRYPTO_num_locks(); i < num; i++)
	{
		g_OpenSSLMutexes.emplace_back(std::make_unique<Mutex>());
	}

	CRYPTO_set_locking_callback(openssl_locking);
	CRYPTO_set_dynlock_create_callback(openssl_dynlock_create);
	CRYPTO_set_dynlock_destroy_callback(openssl_dynlock_destroy);
	CRYPTO_set_dynlock_lock_callback(openssl_dynlock_lock);
#endif /* NEED_CRYPTO_LOCKING */

	SSL_load_error_strings();
	SSL_library_init();
	OpenSSL_add_all_algorithms();
}

void TlsSocket::InitOptions(const char* certStore)
{
	if (Util::EmptyStr(certStore)) 
		return;

	m_certStore = certStore;

	InitX509Store(m_certStore);
}

void TlsSocket::InitX509Store(const std::string& certStore)
{
	if (m_X509Store) 
	{
		FreeX509Store(m_X509Store);
	}

	m_X509Store = X509_STORE_new();
	if (!m_X509Store)
	{
		error("Could not create certificate store");
		return;
	}

	if (!X509_STORE_load_locations(m_X509Store, certStore.c_str(), nullptr))
	{
		FreeX509Store(m_X509Store);
		error("Could not load certificate store location");
		return;
	}
}

void TlsSocket::FreeX509Store(X509_STORE* store)
{
	X509_STORE_free(m_X509Store);
	m_X509Store = nullptr;
}

void TlsSocket::Final()
{

	X509_STORE_free(m_X509Store);

#ifndef LIBRESSL_VERSION_NUMBER
#if OPENSSL_VERSION_NUMBER < 0x30000000L
	FIPS_mode_set(0);
#else
	EVP_default_properties_enable_fips(nullptr, 0);
#endif
#endif
#ifdef NEED_CRYPTO_LOCKING
	CRYPTO_set_locking_callback(nullptr);
	CRYPTO_set_id_callback(nullptr);
#endif
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	ERR_remove_state(0);
#endif
#if	OPENSSL_VERSION_NUMBER >= 0x10002000L && ! defined (LIBRESSL_VERSION_NUMBER)
	SSL_COMP_free_compression_methods();
#endif
	//ENGINE_cleanup();
	CONF_modules_free();
	CONF_modules_unload(1);
#ifndef OPENSSL_NO_COMP
	COMP_zlib_cleanup();
#endif
	ERR_free_strings();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
}

TlsSocket::~TlsSocket()
{
	Close();

#if OPENSSL_VERSION_NUMBER < 0x10100000L
	ERR_remove_state(0);
#endif
}

void TlsSocket::ReportError(const char* errMsg, bool suppressable)
{
	int errcode = ERR_get_error();
	do
	{
		char errstr[1024];
		ERR_error_string_n(errcode, errstr, sizeof(errstr));
		errstr[1024-1] = '\0';

		if (suppressable && m_suppressErrors)
		{
			debug("%s: %s", errMsg, errstr);
		}
		else if (errcode != 0)
		{
			PrintError(BString<1024>("%s: %s", errMsg, errstr));
		}
		else
		{
			PrintError(errMsg);
		}

		errcode = ERR_get_error();
	} while (errcode);
}

void TlsSocket::PrintError(const char* errMsg)
{
	error("%s", errMsg);
}

bool TlsSocket::Start()
{
	m_context = SSL_CTX_new(SSLv23_method());

	if (!m_context)
	{
		ReportError("Could not create TLS context", false);
		return false;
	}

	if (!m_certFile.empty() && !m_keyFile.empty())
	{
		if (SSL_CTX_use_certificate_chain_file((SSL_CTX*)m_context, m_certFile.c_str()) != 1)
		{
			ReportError("Could not load certificate file", false);
			Close();
			return false;
		}
		if (SSL_CTX_use_PrivateKey_file((SSL_CTX*)m_context, m_keyFile.c_str(), SSL_FILETYPE_PEM) != 1)
		{
			ReportError("Could not load key file", false);
			Close();
			return false;
		}
		if (!SSL_CTX_set_options((SSL_CTX*)m_context, SSL_OP_NO_SSLv3))
		{
			ReportError("Could not select minimum protocol version for TLS", false);
			Close();
			return false;
		}

		// For ECC certificates
		EC_KEY* ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
		if (!ecdh)
		{
			ReportError("Could not generate ecdh parameters for TLS", false);
			Close();
			return false;
		}
		if (!SSL_CTX_set_tmp_ecdh((SSL_CTX*)m_context, ecdh))
		{
			ReportError("Could not set ecdh parameters for TLS", false);
			EC_KEY_free(ecdh);
			Close();
			return false;
		}
		EC_KEY_free(ecdh);
	}

	if (m_isClient && !m_certStore.empty())
	{
		if (!m_X509Store)
		{
			error("Could not set certificate store location. Make sure the CertPath option is correct.");
			Close();
			return false;
		}

		SSL_CTX_set1_cert_store((SSL_CTX*)m_context, m_X509Store);

		if (m_certVerifLevel > Options::ECertVerifLevel::cvNone)
		{
			SSL_CTX_set_verify((SSL_CTX*)m_context, SSL_VERIFY_PEER, nullptr);
		}
		else
		{
			SSL_CTX_set_verify((SSL_CTX*)m_context, SSL_VERIFY_NONE, nullptr);
		}
	}

	m_session = SSL_new((SSL_CTX*)m_context);
	if (!m_session)
	{
		ReportError("Could not create TLS session", false);
		Close();
		return false;
	}

	if (!m_cipher.empty() && !SSL_set_cipher_list((SSL*)m_session, m_cipher.c_str()))
	{
		ReportError("Could not select cipher for TLS", false);
		Close();
		return false;
	}

	if (m_isClient && !m_host.empty() && !SSL_set_tlsext_host_name((SSL*)m_session, m_host.c_str()))
	{
		ReportError("Could not set host name for TLS");
		Close();
		return false;
	}

	if (!SSL_set_fd((SSL*)m_session, (int)m_socket))
	{
		ReportError("Could not set the file descriptor for TLS");
		Close();
		return false;
	}

	int error_code = m_isClient ? SSL_connect((SSL*)m_session) : SSL_accept((SSL*)m_session);
	if (error_code < 1 && m_certVerifLevel > Options::ECertVerifLevel::cvNone)
	{
		long verifyRes = SSL_get_verify_result((SSL*)m_session);
		if (verifyRes != X509_V_OK)
		{
			PrintError(BString<1024>("TLS certificate verification failed for %s: %s."
				" For more info visit https://nzbget.com/documentation/certificate-verification/",
				m_host.c_str(), X509_verify_cert_error_string(verifyRes)));
		}
		else
		{
			ReportError(BString<1024>("TLS handshake failed for %s", m_host.c_str()));
		}
		Close();
		return false;
	}

	if (m_isClient && !m_certStore.empty() && !ValidateCert())
	{
		Close();
		return false;
	}

	m_connected = true;
	return true;
}

bool TlsSocket::ValidateCert()
{
	// verify a server certificate was presented during the negotiation
	X509* cert = SSL_get_peer_certificate((SSL*)m_session);
	if (!cert)
	{
		PrintError(BString<1024>("TLS certificate verification failed for %s: no certificate provided by server."
			" For more info visit https://nzbget.com/documentation/certificate-verification/", m_host.c_str()));
		return false;
	}

#ifdef HAVE_X509_CHECK_HOST
	// hostname verification
	if (m_certVerifLevel > Options::ECertVerifLevel::cvMinimal && !m_host.empty() && X509_check_host(cert, m_host.c_str(), m_host.size(), 0, nullptr) != 1)
	{
		const unsigned char* certHost = nullptr;
        // Find the position of the CN field in the Subject field of the certificate
        int common_name_loc = X509_NAME_get_index_by_NID(X509_get_subject_name(cert), NID_commonName, -1);
        if (common_name_loc >= 0)
		{
			// Extract the CN field
			X509_NAME_ENTRY* common_name_entry = X509_NAME_get_entry(X509_get_subject_name(cert), common_name_loc);
			if (common_name_entry != nullptr)
			{
				// Convert the CN field to a C string
				ASN1_STRING* common_name_asn1 = X509_NAME_ENTRY_get_data(common_name_entry);
				if (common_name_asn1 != nullptr)
				{
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
					certHost = ASN1_STRING_get0_data(common_name_asn1);
#else
					certHost = ASN1_STRING_data(common_name_asn1);
#endif
				}
			}
        }

		PrintError(BString<1024>("TLS certificate verification failed for %s: certificate hostname mismatch (%s)."
			" For more info visit https://nzbget.com/documentation/certificate-verification/", m_host.c_str(), certHost));
		X509_free(cert);
		return false;
	}
#endif

	X509_free(cert);
	return true;
}

void TlsSocket::Close()
{
	if (m_session)
	{
		if (m_connected)
		{
			SSL_shutdown((SSL*)m_session);
		}
		SSL_free((SSL*)m_session);

		m_session = nullptr;
	}

	if (m_context)
	{
		SSL_CTX_free((SSL_CTX*)m_context);

		m_context = nullptr;
	}
}

int TlsSocket::Send(const char* buffer, int size)
{
	m_retCode = SSL_write((SSL*)m_session, buffer, size);

	if (m_retCode < 0)
	{
		if (ERR_peek_error() == 0)
		{
			ReportError("Could not write to TLS-Socket: Connection closed by remote host");
		}
		else
			ReportError("Could not write to TLS-Socket");
		return -1;
	}

	return m_retCode;
}

int TlsSocket::Recv(char* buffer, int size)
{
	m_retCode = SSL_read((SSL*)m_session, buffer, size);

	if (m_retCode < 0)
	{
		if (ERR_peek_error() == 0)
		{
			ReportError("Could not read from TLS-Socket: Connection closed by remote host");
		}
		else
		{
			ReportError("Could not read from TLS-Socket");
		}
		return -1;
	}

	return m_retCode;
}

#endif
