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

#include "CPU.h"
#include "Util.h"
#include "Log.h"
#include "FileSystem.h"

namespace SystemInfo
{
	static const int BUFFER_SIZE = 256;

	CPU::CPU()
	{
		Init();
	}

	const std::string& CPU::GetModel() const
	{
		return m_model;
	}

	const std::string& CPU::GetArch() const
	{
		return m_arch;
	}

	std::string CPU::GetCanonicalCPUArch(const std::string& arch) const
	{
		if (arch == "i386" || arch == "x86")
			return "i686";

		if (arch == "amd64" || arch == "AMD64")
			return "x86_64";

		if (arch.find("armv5") != std::string::npos ||
			arch.find("armv6") != std::string::npos)
			return "armel";

		if (arch.find("armv7") != std::string::npos ||
			arch.find("armv8") != std::string::npos)
			return "armhf";

		return arch;
	}

#ifdef WIN32
	void CPU::Init()
	{
		auto result = GetCPUModel();
		if (result.has_value())
		{
			m_model = std::move(result.value());
		}
		else
		{
			warn("Failed to get CPU model. Couldn't read Windows Registry");
		}

		result = GetCPUArch();
		if (result.has_value())
		{
			m_arch = std::move(result.value());
		}
		else
		{
			warn("Failed to get CPU arch. Couldn't read Windows Registry");
		}
	}

	std::optional<std::string> CPU::GetCPUModel() const
	{
		int len = BUFFER_SIZE;
		char buffer[BUFFER_SIZE];
		if (Util::RegReadStr(
			HKEY_LOCAL_MACHINE,
			"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
			"ProcessorNameString",
			buffer,
			&len))
		{
			std::string model{ buffer };
			Util::Trim(model);
			return model;
		}

		return std::nullopt;
	}

	std::optional<std::string> CPU::GetCPUArch() const
	{
		int len = BUFFER_SIZE;
		char buffer[BUFFER_SIZE];
		if (Util::RegReadStr(
			HKEY_LOCAL_MACHINE,
			"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment",
			"PROCESSOR_ARCHITECTURE",
			buffer,
			&len))
		{
			std::string arch{ buffer };
			Util::Trim(arch);
			return GetCanonicalCPUArch(arch);
		}

		return std::nullopt;
	}
#endif	

#ifdef __linux__
#include <fstream>
	void CPU::Init()
	{
		auto result = GetCPUArch();
		if (result.has_value())
		{
			m_arch = std::move(result.value());
		}

		result = GetCPUModelFromCPUInfo();
		if (result.has_value())
		{
			m_model = std::move(result.value());
			return;
		}

		warn("Failed to get CPU model from '/proc/cpuinfo'");

		result = GetCPUModelFromLSCPU();
		if (result.has_value())
		{
			m_model = std::move(result.value());
			return;
		}

		warn("Failed to get CPU model from 'lscpu'");
	}

	std::optional<std::string> CPU::GetCPUModelFromCPUInfo() const
	{
		std::ifstream cpuinfo("/proc/cpuinfo");
		if (!cpuinfo.is_open())
		{
			return std::nullopt;
		}

		std::string line;
		while (std::getline(cpuinfo, line))
		{
			if (line.find("model name") != std::string::npos ||
				line.find("Processor") != std::string::npos ||
				line.find("cpu model") != std::string::npos)
			{
				line = line.substr(line.find(":") + 1);
				Util::Trim(line);
				return line;
			}
		}

		return std::nullopt;
	}

	std::optional<std::string> CPU::GetCPUModelFromLSCPU() const
	{
		std::string cmd = "lscpu | grep \"Model name\"";
		auto pipe = Util::MakePipe(cmd);
		if (!pipe)
		{
			return std::nullopt;
		}

		char buffer[BUFFER_SIZE];
		if (fgets(buffer, BUFFER_SIZE, pipe.get()))
		{
			std::string model{ buffer };
			model = model.substr(model.find(":") + 1);
			Util::Trim(model);
			return model;
		}

		return std::nullopt;
	}
#endif

#if __BSD__
	void CPU::Init()
	{
		size_t len = BUFFER_SIZE;
		char model[BUFFER_SIZE];
		if (sysctlbyname("hw.model", &model, &len, nullptr, 0) == 0)
		{
			m_model = model;
			Util::Trim(m_model);
		}
		else
		{
			warn("Failed to get CPU model. Couldn't read 'hw.model'");
		}

		auto result = GetCPUArch();
		if (result.has_value())
		{
			m_arch = std::move(result.value());
		}
	}

#endif

#ifdef __APPLE__
	void CPU::Init()
	{
		size_t len = BUFFER_SIZE;
		char buffer[BUFFER_SIZE];
		if (sysctlbyname("machdep.cpu.brand_string", &buffer, &len, nullptr, 0) == 0)
		{
			m_model = buffer;
			Util::Trim(m_model);
		}
		else
		{
			warn("Failed to get CPU model. Couldn't read 'machdep.cpu.brand_string'");
		}

		auto result = GetCPUArch();
		if (result.has_value())
		{
			m_arch = std::move(result.value());
		}
	}
#endif

#ifndef WIN32
	std::optional<std::string> CPU::GetCPUArch() const
	{
		auto res = Util::Uname("-m");
		if (!res.has_value())
		{
			warn("Failed to get CPU arch from 'uname-m'");

			return std::nullopt;
		}

		return GetCanonicalCPUArch(res.value());
	}
#endif
}
