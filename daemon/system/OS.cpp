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


#include "nzbget.h"

#include "OS.h"
#include "Util.h"
#include "Log.h"
#include "FileSystem.h"

namespace SystemInfo
{
	static const int BUFFER_SIZE = 128;

	OS::OS()
	{
		Init();
	}

	const std::string& OS::GetName() const
	{
		return m_name;
	}

	const std::string& OS::GetVersion() const
	{
		return m_version;
	}

#ifdef WIN32
	void OS::Init()
	{
		m_name = "Windows";

		int len = BUFFER_SIZE;
		char buildBuffer[BUFFER_SIZE];
		if (!Util::RegReadStr(
			HKEY_LOCAL_MACHINE,
			"SOFTWARE\\MICROSOFT\\Windows NT\\CurrentVersion",
			"CurrentBuild",
			buildBuffer,
			&len))
		{
			warn("Failed to get OS version. Couldn't read from Windows Registry");

			return;
		}

		long buildNum = std::atol(buildBuffer);
		if (buildNum == 0)
		{
			warn("Got invalid Windows version: %s", buildBuffer);

			return;
		}

		if (buildNum >= m_win11BuildVersion) m_version = "11";
		else if (buildNum >= m_win10BuildVersion) m_version = "10";
		else if (buildNum >= m_win8BuildVersion) m_version = "8";
		else if (buildNum >= m_winXPBuildVersion) m_version = "XP";
		else
		{
			warn("Unsupported Windows version");
			return;
		}

		len = BUFFER_SIZE;
		char updateBuffer[BUFFER_SIZE];
		if (Util::RegReadStr(
			HKEY_LOCAL_MACHINE,
			"SOFTWARE\\MICROSOFT\\Windows NT\\CurrentVersion",
			"DisplayVersion",
			updateBuffer,
			&len))
		{
			m_version += std::string(" ") + updateBuffer;
		}
		else
		{
			warn("Failed to get OS update version. Couldn't read from Windows Registry");
		}

		m_version += std::string(" (") + buildBuffer + ")";
	}
#endif	

#ifdef __linux__
#include <fstream>
	bool OS::IsRunningInDocker() const
	{
		return FileSystem::FileExists("/.dockerenv");
	}

	void OS::TrimQuotes(std::string& str) const
	{
		if (str.front() == '"')
		{
			str = str.substr(1);
		}

		if (str.back() == '"')
		{
			str = str.substr(0, str.size() - 1);
		}
	}

	void OS::Init()
	{
		InitOSInfoFromOSRelease();

		if (m_name.empty())
		{
			auto res = Util::Uname("-o");
			if (res.has_value())
			{
				m_name = std::move(res.value());
			}
			else
			{
				warn("Failed to get OS name. Couldn't read 'uname -o'");
			}
		}

		if (m_version.empty())
		{
			auto res = Util::Uname("-r");
			if (res.has_value())
			{
				m_version = std::move(res.value());
			}
			else
			{
				warn("Failed to get OS name. Couldn't read 'uname -o'");
			}
		}

		if (IsRunningInDocker())
		{
			m_version += " (Running in Docker)";
		}
	}

	void OS::InitOSInfoFromOSRelease()
	{
		std::ifstream osInfo("/etc/os-release");
		if (!osInfo.is_open())
		{
			return;
		}

		std::string line;
		while (std::getline(osInfo, line))
		{
			if (!m_name.empty() && !m_version.empty())
			{
				return;
			}

			// e.g NAME="Debian GNU/Linux"
			if (m_name.empty() && line.find("NAME=") == 0)
			{
				m_name = line.substr(line.find("=") + 1);

				Util::Trim(m_name);
				TrimQuotes(m_name);

				continue;
			}

			// e.g VERSION_ID="12"
			if (m_version.empty() && line.find("VERSION_ID=") == 0)
			{
				m_version = line.substr(line.find("=") + 1);

				Util::Trim(m_version);
				TrimQuotes(m_version);

				continue;
			}

			// e.g. BUILD_ID=rolling
			if (m_version.empty() && line.find("BUILD_ID=") == 0)
			{
				m_version = line.substr(line.find("=") + 1);
				Util::Trim(m_version);

				continue;
			}
		}
	}
#endif

#ifdef __APPLE__
	void OS::Init()
	{
		std::string cmd = std::string("sw_vers");
		auto pipe = Util::MakePipe(cmd);
		if (!pipe)
		{
			warn("Failed to get OS info. Couldn't read 'sw_vers'");
			return;
		}

		char buffer[BUFFER_SIZE];
		std::string result = "";
		while (!feof(pipe.get()))
		{
			if (fgets(buffer, BUFFER_SIZE, pipe.get()))
			{
				result += buffer;
			}
		};

		std::string productName = "ProductName:";
		size_t pos = result.find(productName);
		if (pos != std::string::npos)
		{
			size_t endPos = result.find("\n", pos);
			m_name = result.substr(pos + productName.size(), endPos - pos - productName.size());
			Util::Trim(m_name);
		}
		else
		{
			warn("Failed to get OS name. Couldn't find 'ProductName'");
		}

		std::string productVersion = "ProductVersion:";
		pos = result.find(productVersion);
		if (pos != std::string::npos)
		{
			size_t endPos = result.find("\n", pos);
			m_version = result.substr(pos + productVersion.size(), endPos - pos - productVersion.size());
			Util::Trim(m_version);
		}
		else
		{
			warn("Failed to get OS version. Couldn't find 'ProductVersion'");
		}
	}
#endif

#if __BSD__
	void OS::Init()
	{
		size_t len = BUFFER_SIZE;
		char buffer[len];
		if (sysctlbyname("kern.ostype", &buffer, &len, nullptr, 0) == 0)
		{
			m_name = buffer;
			Util::Trim(m_name);
		}
		else
		{
			warn("Failed to get OS name. Couldn't read 'kern.ostype'");
		}

		len = BUFFER_SIZE;

		if (sysctlbyname("kern.osrelease", &buffer, &len, nullptr, 0) == 0)
		{
			m_version = buffer;
			Util::Trim(m_version);
		}
		else
		{
			warn("Failed to get OS version. Couldn't to read 'kern.osrelease'");
		}
	}
#endif

}
