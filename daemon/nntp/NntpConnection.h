/*
 *  This file is part of nzbget. See <http://nzbget.com>.
 *
 *  Copyright (C) 2023 nzbget.com <nzbget@nzbget.com>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef NNTPCONNECTION_H
#define NNTPCONNECTION_H

#include "NString.h"
#include "NewsServer.h"
#include "Connection.h"

class NntpConnection : public Connection
{
public:
	NntpConnection(NewsServer* newsServer);
	virtual bool Connect();
	virtual bool Disconnect();
	NewsServer* GetNewsServer() { return m_newsServer; }
	const char* Request(const char* req);
	const char* JoinGroup(const char* grp);
	bool GetAuthError() { return m_authError; }

private:
	NewsServer* m_newsServer;
	CString m_activeGroup;
	CharBuffer m_lineBuf;
	bool m_authError = false;

	void Clear();
	void ReportErrorAnswer(const char* msgPrefix, const char* answer);
	bool Authenticate();
	bool AuthInfoUser(int recur);
	bool AuthInfoPass(int recur);
};

#endif
