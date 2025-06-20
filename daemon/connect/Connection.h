/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef CONNECTION_H
#define CONNECTION_H

#include <atomic>
#include <string>
#include "NString.h"
#ifndef HAVE_GETADDRINFO
#ifndef HAVE_GETHOSTBYNAME_R
#include "Thread.h"
#endif
#endif
#ifndef DISABLE_TLS
#include "TlsSocket.h"
#endif

class Connection
{
public:
	enum EStatus
	{
		csConnected,
		csDisconnected,
		csListening,
		csCancelled,
		csBroken
	};

	enum EIPVersion
	{
		ipAuto,
		ipV4,
		ipV6
	};

	Connection(const char* host, int port, bool tls);
	Connection(SOCKET socket, bool tls);
	virtual ~Connection();
	static void Init();
	static void Final();
	virtual bool Connect();
	virtual bool Disconnect();
	bool Bind();
	bool Send(const char* buffer, int size);
	bool Recv(char* buffer, int size);
	int TryRecv(char* buffer, int size);
	char* ReadLine(char* buffer, int size, int* bytesRead);
	void ReadBuffer(char** buffer, int *bufLen);
	int WriteLine(const char* buffer);
	std::unique_ptr<Connection> Accept();
	void Cancel();
	const char* GetHost() const { return m_host.c_str(); }
	int GetPort() { return m_port; }
	bool GetTls() { return m_tls; }
	const char* GetCipher() const { return m_cipher.c_str(); }
	void SetCipher(const char* cipher) { m_cipher = cipher ? cipher : ""; }
	void SetTimeout(int timeout) { m_timeout = timeout; }
	void SetIPVersion(EIPVersion ipVersion) { m_ipVersion = ipVersion; }
	EStatus GetStatus() { return m_status; }
	void SetSuppressErrors(bool suppressErrors);
	bool GetSuppressErrors() { return m_suppressErrors; }
	const char* GetRemoteAddr();
	bool GetGracefull() { return m_gracefull; }
	void SetGracefull(bool gracefull) { m_gracefull = gracefull; }
	void SetForceClose(bool forceClose) { m_forceClose = forceClose; }
#ifndef DISABLE_TLS
	bool StartTls(bool isClient, const char* certFile, const char* keyFile);
	void SetCertVerifLevel(unsigned int level) { m_certVerifLevel = level; }
#endif
	int FetchTotalBytesRead();

protected:
	std::string m_host;
	std::string m_cipher;
	int m_port;
	bool m_tls;
	EIPVersion m_ipVersion = ipAuto;
	SOCKET m_socket = INVALID_SOCKET;
	CharBuffer m_readBuf;
	int m_bufAvail = 0;
	char* m_bufPtr = nullptr;
	std::atomic<EStatus> m_status{csDisconnected};
	int m_timeout = 60;
	bool m_suppressErrors = true;
	BString<100> m_remoteAddr;
	int m_totalBytesRead = 0;
	bool m_gracefull = false;
	bool m_forceClose = false;
#ifndef DISABLE_TLS
	unsigned int m_certVerifLevel;
#endif

	struct SockAddr
	{
		int ai_family;
		int ai_socktype;
		int ai_protocol;
		bool operator==(const SockAddr& rhs) const
			{ return memcmp(this, &rhs, sizeof(SockAddr)) == 0; }
	};

#ifndef DISABLE_TLS
	std::unique_ptr<TlsSocket> m_tlsSocket;
	bool m_tlsError = false;
#endif
#ifndef HAVE_GETADDRINFO
#ifndef HAVE_GETHOSTBYNAME_R
	static std::unique_ptr<Mutex> m_getHostByNameMutex;
#endif
#endif

	void ReportError(const char* msgPrefix, const char* msgArg, bool printErrCode, int errCode = 0,
		const char* errMsg = nullptr);
	virtual void PrintError(const char* errMsg);
	int GetLastNetworkError();
	bool DoConnect();
	bool DoDisconnect();
	bool InitSocketOpts(SOCKET socket);
	bool ConnectWithTimeout(void* address, int address_len);
#ifndef HAVE_GETADDRINFO
	in_addr_t ResolveHostAddr(const char* host);
#endif
#ifndef DISABLE_TLS
	int recv(SOCKET s, char* buf, int len, int flags);
	int send(SOCKET s, const char* buf, int len, int flags);
	void CloseTls();
#endif
};

#endif
