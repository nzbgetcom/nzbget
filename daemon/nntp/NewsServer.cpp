/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"
#include "NewsServer.h"

NewsServer::NewsServer(int id, bool active, const char* name, const char* host, int port, int ipVersion,
	const char* user, const char* pass, bool joinGroup, bool tls, const char* cipher,
	int maxConnections, int retention, int level, int group, bool optional, unsigned int certVerificationfLevel) :
		m_id(id), m_active(active), m_name(name), m_host(host ? host : ""), m_port(port), m_ipVersion(ipVersion),
		m_user(user ? user : ""), m_password(pass ? pass : ""), m_joinGroup(joinGroup), m_tls(tls),
		m_cipher(cipher ? cipher : ""), m_maxConnections(maxConnections), m_retention(retention),
		m_level(level), m_normLevel(level), m_group(group), m_optional(optional), m_certVerificationfLevel(certVerificationfLevel)
{
	if (m_name.Empty())
	{
		m_name.Format("server%i", id);
	}
}

void NewsServer::DelayConnect()
{
	auto deadline = std::chrono::steady_clock::time_point::max();

	{
		std::lock_guard<std::mutex> lock(m_connectMutex);

		auto now = std::chrono::steady_clock::now();
		if (m_lastConnectTime == std::chrono::steady_clock::time_point::min())
		{
			m_lastConnectTime = now;
			return;
		}

		if (now >= m_lastConnectTime + m_connectionDelay)
		{
			m_lastConnectTime = now;
			return;
		}

		m_lastConnectTime += m_connectionDelay;
		deadline = m_lastConnectTime;
	}

	if (deadline != std::chrono::steady_clock::time_point::max())
	{
		std::this_thread::sleep_until(deadline);
	}
}
