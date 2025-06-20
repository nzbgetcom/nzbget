/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2008-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2024-2025 Denis <denis@nzbget.com>
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

OpenSSL::X509StorePtr TlsSocket::m_X509Store{ nullptr, &X509_STORE_free };

void TlsSocket::Init()
{
	debug("Initializing TLS library");

	if (OPENSSL_init_ssl(0, nullptr) == 0)
	{
		error("Could not initialize the OpenSSL library");
	}
}

void TlsSocket::InitOptions(const char* certStore)
{
	if (Util::EmptyStr(certStore)) 
		return;

	InitX509Store(certStore);
}

void TlsSocket::InitX509Store(std::string_view certStore)
{
	m_X509Store.reset(X509_STORE_new());

	if (!m_X509Store)
	{
		error("Could not create certificate store");
		return;
	}

	if (!X509_STORE_load_locations(m_X509Store.get(), certStore.data(), nullptr))
	{
		error("Could not load certificate store location from %s", certStore.data());
		return;
	}
}

void TlsSocket::Final()
{
	m_X509Store.reset();
	OPENSSL_cleanup();
}

TlsSocket::~TlsSocket()
{
	Close();
	ERR_clear_error();
}

void TlsSocket::ReportError(const char* errMsg, bool suppressable)
{
	auto errcode = ERR_get_error();
	do
	{
		char errstr[1024];
		ERR_error_string_n(errcode, errstr, sizeof(errstr));
		errstr[1024 - 1] = '\0';

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
	m_context.reset(SSL_CTX_new(TLS_method()));

	if (!m_context)
	{
		ReportError("Could not create TLS context", false);
		return false;
	}

	if (!m_certFile.empty() && !m_keyFile.empty())
	{
		if (!SSL_CTX_use_certificate_chain_file(m_context.get(), m_certFile.c_str()))
		{
			ReportError("Could not load certificate file", false);
			Close();
			return false;
		}
		if (!SSL_CTX_use_PrivateKey_file(m_context.get(), m_keyFile.c_str(), SSL_FILETYPE_PEM))
		{
			ReportError("Could not load key file", false);
			Close();
			return false;
		}
	}

	if (m_isClient && m_X509Store)
	{
		SSL_CTX_set1_cert_store(m_context.get(), m_X509Store.get());

		if (m_certVerifLevel > Options::ECertVerifLevel::cvNone)
		{
			SSL_CTX_set_verify(m_context.get(), SSL_VERIFY_PEER, nullptr);
		}
		else
		{
			SSL_CTX_set_verify(m_context.get(), SSL_VERIFY_NONE, nullptr);
		}
	}

	m_session.reset(SSL_new(m_context.get()));
	if (!m_session)
	{
		ReportError("Could not create TLS session", false);
		Close();
		return false;
	}

	if (!m_cipher.empty() && !SSL_set_cipher_list(m_session.get(), m_cipher.c_str()))
	{
		ReportError("Could not select cipher for TLS", false);
		Close();
		return false;
	}

	if (m_isClient && !m_host.empty() && !SSL_set_tlsext_host_name(m_session.get(), m_host.c_str()))
	{
		ReportError("Could not set host name for TLS");
		Close();
		return false;
	}

	if (!SSL_set_fd(m_session.get(), static_cast<int>(m_socket)))
	{
		ReportError("Could not set the file descriptor for TLS");
		Close();
		return false;
	}

	if (!SSL_CTX_set_min_proto_version(m_context.get(), SSL3_VERSION))
	{
		ReportError("Could not set minimum protocol to SSL3", false);
		return false;
	}

	int error_code = m_isClient ? SSL_connect(m_session.get()) : SSL_accept(m_session.get());
	if (error_code < 1 && m_certVerifLevel > Options::ECertVerifLevel::cvNone)
	{
		long verifyRes = SSL_get_verify_result(m_session.get());
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

	if (m_isClient && m_X509Store && !ValidateCert())
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
	OpenSSL::X509Ptr cert{ SSL_get_peer_certificate(m_session.get()), &X509_free };
	if (!cert)
	{
		PrintError(BString<1024>("TLS certificate verification failed for %s: no certificate provided by server."
			" For more info visit https://nzbget.com/documentation/certificate-verification/", m_host.c_str()));
		return false;
	}

#ifdef HAVE_X509_CHECK_HOST
	// hostname verification
	if (m_certVerifLevel > Options::ECertVerifLevel::cvMinimal && !m_host.empty() && !X509_check_host(cert.get(), m_host.c_str(), m_host.size(), 0, nullptr))
	{
		const unsigned char* certHost = nullptr;
        // Find the position of the CN field in the Subject field of the certificate
        int common_name_loc = X509_NAME_get_index_by_NID(X509_get_subject_name(cert.get()), NID_commonName, -1);
        if (common_name_loc >= 0)
		{
			// Extract the CN field
			X509_NAME_ENTRY* common_name_entry = X509_NAME_get_entry(X509_get_subject_name(cert.get()), common_name_loc);
			if (common_name_entry != nullptr)
			{
				// Convert the CN field to a C string
				ASN1_STRING* common_name_asn1 = X509_NAME_ENTRY_get_data(common_name_entry);
				if (common_name_asn1 != nullptr)
				{
					certHost = ASN1_STRING_get0_data(common_name_asn1);
				}
			}
        }

		PrintError(BString<1024>("TLS certificate verification failed for %s: certificate hostname mismatch (%s)."
			" For more info visit https://nzbget.com/documentation/certificate-verification/", m_host.c_str(), certHost));
		return false;
	}
#endif

	return true;
}

void TlsSocket::Close()
{
	if (m_session && m_connected)
	{
		if (SSL_shutdown(m_session.get()) == 0)
		{
			// The bidirectional shutdown is not yet complete. Call SSL_shutdown again.
			SSL_shutdown(m_session.get());
		}
	}

	m_context.reset();
	m_session.reset();
}

int TlsSocket::Send(const char* buffer, int size)
{
	m_retCode = SSL_write(m_session.get(), buffer, size);

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
	m_retCode = SSL_read(m_session.get(), buffer, size);

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
