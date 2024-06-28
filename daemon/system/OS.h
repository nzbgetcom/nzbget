/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
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

#ifndef OS_H
#define OS_H

#include <string>
#include <optional>

namespace SystemInfo
{
	class OS final
	{
	public:
		OS();
		const std::string& GetName() const;
		const std::string& GetVersion() const;

	private:
		void Init();
		std::string m_name;
		std::string m_version;

#ifdef __linux__
		bool IsRunningInDocker() const;
		void TrimQuotes(std::string& str) const;
		void InitOSInfoFromOSRelease();
#endif

#ifdef WIN32
		const long m_win11BuildVersion = 22000;
		const long m_win10BuildVersion = 10240;
		const long m_win8BuildVersion = 9200;
		const long m_win7BuildVersion = 7600;
		const long m_winXPBuildVersion = 2600;
#endif
	};
}

#endif
