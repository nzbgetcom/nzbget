/*
 *  This file if part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2025-2026 Denis <denis@nzbget.com>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#ifndef NEWSSERVER_H
#define NEWSSERVER_H

#include <string>

class NewsServer
{
public:
	NewsServer(int id, bool active, const char* name, const char* host, int port, int ipVersion,
		const char* user, const char* pass, bool joinGroup,
		bool tls, const char* cipher, int maxConnections, int retention,
		int level, int group, bool optional, unsigned int certVerificationfLevel);
	int GetId() const { return m_id; }
	int GetStateId() const { return m_stateId; }
	void SetStateId(int stateId) { m_stateId = stateId; }
	bool GetActive() const { return m_active; }
	void SetActive(bool active) { m_active = active; }
	const char* GetName() const { return m_name.c_str(); }
	int GetGroup() const { return m_group; }
	const char* GetHost() const { return m_host.c_str(); }
	int GetPort() const { return m_port; }
	int GetIpVersion() const { return m_ipVersion; }
	const char* GetUser() const { return m_user.c_str(); }
	const char* GetPassword() const { return m_password.c_str(); }
	int GetMaxConnections() const { return m_maxConnections; }
	int GetLevel() const { return m_level; }
	int GetNormLevel() const { return m_normLevel; }
	void SetNormLevel(int level) { m_normLevel = level; }
	int GetJoinGroup() const { return m_joinGroup; }
	bool GetTls() const { return m_tls; }
	const char* GetCipher() const { return m_cipher.c_str(); }
	int GetRetention() const { return m_retention; }
	bool GetOptional() const { return m_optional; }
	time_t GetBlockTime() const { return m_blockTime; }
	void SetBlockTime(time_t blockTime) { m_blockTime = blockTime; }
	unsigned int GetCertVerificationLevel() const { return m_certVerificationfLevel; }

private:
	std::string m_name;
	std::string m_host;
	std::string m_user;
	std::string m_password;
	std::string m_cipher;
	std::atomic<time_t> m_blockTime = 0;
	int m_id;
	int m_stateId = 0;
	int m_port;
	int m_ipVersion;
	int m_maxConnections;
	int m_retention;
	int m_level;
	int m_normLevel;
	int m_group;
	unsigned int m_certVerificationfLevel;
	std::atomic<bool> m_active;
	bool m_joinGroup;
	bool m_tls;
	bool m_optional = false;
};

typedef std::vector<std::unique_ptr<NewsServer>> Servers;

#endif
