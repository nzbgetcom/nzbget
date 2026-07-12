/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 XBMC 4 Lyfe <273732874+xbmc4lyfe@users.noreply.github.com>
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

#ifndef CONNECTIONIDLEPOLICY_H
#define CONNECTIONIDLEPOLICY_H

#include <algorithm>
#include <ctime>
#include <limits>

class ConnectionIdlePolicy final
{
public:
	void ObserveInUse()
	{
		m_observed = true;
		m_hasInUseConnection = true;
	}

	void ObserveIdle(time_t inactiveSeconds)
	{
		m_observed = true;
		m_shortestInactiveSeconds = std::min(m_shortestInactiveSeconds, inactiveSeconds);
	}

	bool ShouldClose() const
	{
		return m_observed && !m_hasInUseConnection &&
			m_shortestInactiveSeconds > ConnectionHoldSeconds;
	}

private:
	static constexpr time_t ConnectionHoldSeconds = 5;
	bool m_observed = false;
	bool m_hasInUseConnection = false;
	time_t m_shortestInactiveSeconds = std::numeric_limits<time_t>::max();
};

#endif
