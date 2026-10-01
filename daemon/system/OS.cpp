/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2024-2026 Denis <denis@nzbget.com>
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
#include "Log.h"
#include "Util.h"

namespace System
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
			debug("Failed to get OS version: couldn't read CurrentBuild from Windows Registry");
			return;
		}

		long buildNum = std::atol(buildBuffer);
		if (buildNum == 0)
		{
			debug("Got invalid Windows build number: %s", buildBuffer);
			return;
		}

		if (buildNum >= m_win11BuildVersion) m_version = "11";
		else if (buildNum >= m_win10BuildVersion) m_version = "10";
		else if (buildNum >= m_win8BuildVersion) m_version = "8";
		else if (buildNum >= m_win7BuildVersion) m_version = "7";
		else if (buildNum >= m_winXPBuildVersion) m_version = "XP";
		else
		{
			debug("Unsupported Windows build number: %ld", buildNum);
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
			debug("Failed to get OS display version from Windows Registry");
		}

		m_version += std::string(" (") + buildBuffer + ")";
	}
#endif	

#ifdef __linux__
#include <fstream>
#include <sys/utsname.h>

	bool OS::IsRunningInDocker() const
	{
		return FileSystem::FileExists("/.dockerenv");
	}

	bool OS::IsRunningInContainer() const
	{
		if (std::getenv("container") != nullptr)
		{
			return true;
		}

		if (FileSystem::FileExists("/run/systemd/container") || FileSystem::FileExists("/run/.containerenv"))
		{
			return true;
		}

		return false;
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

		if (m_name.empty() || m_version.empty())
		{
			struct utsname uts;
			if (uname(&uts) == 0)
			{
				if (m_name.empty() && uts.sysname[0] != '\0')
				{
					m_name = uts.sysname;
				}

				if (m_version.empty() && uts.release[0] != '\0')
				{
					m_version = uts.release;
				}
			}
			else
			{
				debug("Failed to get OS info from uname(2)");
			}
		}

		if (IsRunningInDocker())
		{
			m_version += " (Running in Docker)";
		}
		else if (IsRunningInContainer())
		{
			m_version += " (Running in Container)";
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
			debug("Failed to get OS info: couldn't run 'sw_vers'");
			return;
		}

		char buffer[BUFFER_SIZE];
		std::string result;
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
			debug("Failed to get OS name: 'ProductName' not found in sw_vers output");
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
			debug("Failed to get OS version: 'ProductVersion' not found in sw_vers output");
		}
	}
#endif

#if __BSD__
	void OS::Init()
	{
		int mib[2];
		size_t len = BUFFER_SIZE;
		char buffer[BUFFER_SIZE];

		mib[0] = CTL_KERN;
		mib[1] = KERN_OSTYPE;
		if (sysctl(mib, 2, buffer, &len, nullptr, 0) != -1)
		{
			m_name = buffer;
			Util::Trim(m_name);
		}
		else
		{
			debug("Failed to get OS name from sysctl(2): KERN_OSTYPE");
		}

		len = BUFFER_SIZE;

		mib[1] = KERN_OSRELEASE;
		if (sysctl(mib, 2, buffer, &len, nullptr, 0) != -1)
		{
			m_version = buffer;
			Util::Trim(m_version);
		}
		else
		{
			debug("Failed to get OS version from sysctl(2): KERN_OSRELEASE");
		}
	}
#endif

}
