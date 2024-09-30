/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2008-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#ifndef TLSSOCKET_H
#define TLSSOCKET_H

#ifndef DISABLE_TLS

class TlsSocket
{
public:
	TlsSocket(
		SOCKET socket,
		bool isClient,
		const char* host,
		const char* certFile,
		const char* keyFile,
		const char* cipher,
		int certVerifLevel)
		: m_socket(socket)
		, m_isClient(isClient)
		, m_host(host ? host : "")
		, m_certFile(certFile ? certFile : "")
		, m_keyFile(keyFile ? keyFile : "")
		, m_cipher(cipher ? cipher : "")
		, m_certVerifLevel(certVerifLevel) {}

	virtual ~TlsSocket();

	static void Init();
	static void InitOptions(const char* certStore);
	static void Final();

	bool Start();
	void Close();
	int Send(const char* buffer, int size);
	int Recv(char* buffer, int size);
	void SetSuppressErrors(bool suppressErrors) { m_suppressErrors = suppressErrors; }

protected:
	virtual void PrintError(const char* errMsg);

private:
#ifdef HAVE_OPENSSL
	static void InitX509Store(const std::string& certStore);
	static void FreeX509Store(X509_STORE* store);

	static X509_STORE* m_X509Store;
#endif
	static std::string m_certStore;

	SOCKET m_socket;

	std::string m_host;
	std::string m_certFile;
	std::string m_keyFile;
	std::string m_cipher;

	bool m_isClient;
	bool m_suppressErrors = false;
	bool m_initialized = false;
	bool m_connected = false;

	int m_retCode;
	int m_certVerifLevel;

	// using "void*" to prevent the including of GnuTLS/OpenSSL header files into TlsSocket.h
	void* m_context = nullptr;
	void* m_session = nullptr;

	void ReportError(const char* errMsg, bool suppressable = true);
	bool ValidateCert();
};

#endif
#endif
