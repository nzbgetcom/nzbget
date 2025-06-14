/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
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

#include "nzbget.h"

#include <regex>
#include <boost/version.hpp>

#include "SystemInfo.h"
#include "Options.h"
#include "FileSystem.h"
#include "Log.h"
#include "Json.h"
#include "Xml.h"

#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#endif
#ifdef HAVE_NCURSES_NCURSES_H
#include <ncurses/ncurses.h>
#endif
#ifndef DISABLE_PARCHECK
#include <par2/version.h>
#endif

namespace System
{
	constexpr size_t BUFFER_SIZE = 512;

	SystemInfo::SystemInfo()
	{
		InitLibsInfo();
	}

	const std::vector<Library>& SystemInfo::GetLibraries() const
	{
		return m_libraries;
	}

	void SystemInfo::InitLibsInfo()
	{
		m_libraries.reserve(5);
		m_libraries.push_back({ "LibXML2", LIBXML_DOTTED_VERSION });

#if defined(HAVE_NCURSES_H) || defined(HAVE_NCURSES_NCURSES_H)
		m_libraries.push_back({ "ncurses", NCURSES_VERSION });
#endif

#ifndef DISABLE_GZIP
		m_libraries.push_back({ "Gzip", ZLIB_VERSION });
#endif

#ifndef DISABLE_TLS
#ifdef LIBRESSL_VERSION_TEXT
		std::string str = LIBRESSL_VERSION_TEXT;
		m_libraries.push_back({ "LibreSSL", str.substr(str.find(" ") + 1) });
#elif defined(OPENSSL_FULL_VERSION_STR)
		m_libraries.push_back({ "OpenSSL", OPENSSL_FULL_VERSION_STR });
#else 
		std::string str = OPENSSL_VERSION_TEXT;
		m_libraries.push_back({ "OpenSSL", str.substr(str.find(" ") + 1) });
#endif
#endif

#ifndef DISABLE_PARCHECK 
		m_libraries.push_back({ "Par2-turbo", PAR2_TURBO_LIB_VERSION });
#endif

#ifdef BOOST_LIB_VERSION
		m_libraries.push_back({ "Boost", BOOST_LIB_VERSION });
#endif

		m_libraries.shrink_to_fit();
	}

	const CPU& SystemInfo::GetCPUInfo() const
	{
		return m_cpu;
	}

	const OS& SystemInfo::GetOSInfo() const
	{
		return m_os;
	}

	Network SystemInfo::GetNetworkInfo() const
	{
		return GetNetwork();
	}

	std::vector<Tool> SystemInfo::GetTools() const
	{
		std::vector<Tool> tools{
			GetPython(),
			GetSevenZip(),
			GetUnrar(),
		};

		return tools;
	}

	Tool SystemInfo::GetPython() const
	{
		Tool tool;

		tool.name = "Python";

		auto result = Util::FindShellOverriddenExecutor(".py", g_Options->GetShellOverride());
		if (result.has_value())
		{
			if (FileSystem::FileExists(result.value().c_str()))
			{
				tool.path = std::move(result.value());
				Util::Trim(tool.path);
			}
			else
			{
				detail("Failed to find Python: '%s' doesn't exist", result.value().c_str());
				return tool;
			}
		}
		else
		{
			result = FindPython();
			if (!result.has_value())
			{
				detail("Failed to find Python");
				return tool;
			}

			tool.path = std::move(result.value());
			Util::Trim(tool.path);
		}

		result = GetPythonVersion(tool.path);
		if (!result.has_value())
		{
			detail("Failed to get Python version");
			return tool;
		}

		tool.version = std::move(result.value());

		return tool;
	}

	Tool SystemInfo::GetUnrar() const
	{
		Tool tool;

		tool.name = "UnRAR";
		tool.path = GetToolPath(g_Options->GetUnrarCmd());
		tool.version = GetUnpackerVersion(tool.path, "UNRAR");

		return tool;
	}

	Tool SystemInfo::GetSevenZip() const
	{
		Tool tool;

		tool.name = "7-Zip";
		tool.path = GetToolPath(g_Options->GetSevenZipCmd());
		tool.version = GetUnpackerVersion(tool.path, tool.name.c_str());

		return tool;
	}

	std::string SystemInfo::GetToolPath(const char* cmd) const
	{
		if (Util::EmptyStr(cmd))
		{
			return "";
		}

		std::string path = FileSystem::ExtractFilePathFromCmd(cmd);

		auto result = FileSystem::GetRealPath(path);

		if (result.has_value() && FileSystem::FileExists(result.value().c_str()))
		{
			return result.value();
		}

		std::string fullCmd = Util::FIND_CMD + std::string(cmd);

		auto pipe = Util::MakePipe(fullCmd);
		if (!pipe)
		{
			return "";
		}

		char buffer[BUFFER_SIZE];
		if (fgets(buffer, BUFFER_SIZE, pipe.get()))
		{
			std::string resultPath{ buffer };
			Util::Trim(resultPath);
			return resultPath;
		}

		return "";
	}

	std::string SystemInfo::GetUnpackerVersion(const std::string& path, const char* marker) const
	{
		if (path.empty())
		{
			return "";
		}

		std::string cmd = FileSystem::EscapePathForShell(path);

		auto pipe = Util::MakePipe(cmd);
		if (!pipe)
		{
			return "";
		}

		std::string version;
		char buffer[BUFFER_SIZE];
		while (!feof(pipe.get()))
		{
			if (fgets(buffer, BUFFER_SIZE, pipe.get()) != nullptr)
			{
				if (strstr(buffer, marker))
				{
					version = ParseUnpackerVersion(buffer);
					break;
				}
			}
		}

		return version;
	}

	std::optional<std::string> SystemInfo::FindPython() const
	{
		auto result = Util::FindPython();
		if (!result.has_value())
		{
			return std::nullopt;
		}

		std::string cmd = Util::FIND_CMD + result.value();

		auto pipe = Util::MakePipe(cmd);
		if (!pipe)
		{
			return std::nullopt;
		}

		char buffer[BUFFER_SIZE];
		if (fgets(buffer, BUFFER_SIZE, pipe.get()))
		{
			std::string path{ buffer };
			Util::Trim(path);
			return path;
		}

		return std::nullopt;
	}

	std::optional<std::string> SystemInfo::GetPythonVersion(const std::string path) const
	{
		std::string cmd = FileSystem::EscapePathForShell(path) + " --version 2>&1";
		{
			auto pipe = Util::MakePipe(cmd);
			if (!pipe)
			{
				return "";
			}

			char buffer[BUFFER_SIZE];
			if (fgets(buffer, BUFFER_SIZE, pipe.get()))
			{
				// e.g. Python 3.12.3
				std::string version{ buffer };
				Util::Trim(version);
				size_t pos = version.find("Python");
				if (pos != std::string::npos)
				{
					version = version.substr(pos + sizeof("Python"));
					Util::Trim(version);
					return version;
				}
			}
		}

		return std::nullopt;
	}

	std::string SystemInfo::ParseUnpackerVersion(const std::string& line) const
	{
		// e.g. 7-Zip (a) 19.00 (x64) : Copyright (c) 1999-2018 Igor Pavlov : 2019-02-21
		// e.g. UNRAR 5.70 x64 freeware      Copyright (c) 1993-2019 Alexander Roshal
		static const std::regex pattern(R"([0-9]+\.[0-9]+)"); // float number
		std::smatch match;
		if (std::regex_search(line, match, pattern))
		{
			return match[0].str();
		}

		return "";
	}

	std::string SystemInfo::ParseParVersion(const std::string& line) const
	{
		// e.g. par2cmdline-turbo version 1.1.1
		if (size_t pos = line.find_last_of(" "); pos != std::string::npos)
		{
			return line.substr(pos);
		}

		return "";
	}

	std::ostream& operator<<(std::ostream& os, const SystemInfo& sysinfo)
	{
		os << "OS: ";
		os << sysinfo.GetOSInfo().GetName() << " ";
		os << sysinfo.GetOSInfo().GetVersion() << "\n";
		os << "CPU: ";
		os << sysinfo.GetCPUInfo().GetModel() << "\n";
		os << "Arch: ";
		os << sysinfo.GetCPUInfo().GetArch() << "\n";

		for (const auto& tool : sysinfo.GetTools())
		{
			os << tool.name << " " << tool.version << " ";
			os << tool.path << "\n";
		}

		for (const auto& lib : sysinfo.GetLibraries())
		{
			os << lib.name << " " << lib.version << "\n";
		}

		return os;
	}

	std::string ToJsonStr(const SystemInfo& sysInfo)
	{
		Json::JsonObject json;
		Json::JsonObject osJson;
		Json::JsonObject networkJson;
		Json::JsonObject cpuJson;
		Json::JsonArray toolsJson;
		Json::JsonArray librariesJson;

		const auto& os = sysInfo.GetOSInfo();
		const auto& network = sysInfo.GetNetworkInfo();
		const auto& cpu = sysInfo.GetCPUInfo();
		const auto& tools = sysInfo.GetTools();
		const auto& libraries = sysInfo.GetLibraries();

		osJson["Name"] = os.GetName();
		osJson["Version"] = os.GetVersion();
		networkJson["PublicIP"] = network.publicIP;
		networkJson["PrivateIP"] = network.privateIP;
		cpuJson["Model"] = cpu.GetModel();
		cpuJson["Arch"] = cpu.GetArch();

		for (const auto& tool : tools)
		{
			Json::JsonObject toolJson;
			toolJson["Name"] = tool.name;
			toolJson["Version"] = tool.version;
			toolJson["Path"] = tool.path;
			toolsJson.push_back(std::move(toolJson));
		}

		for (const auto& library : libraries)
		{
			Json::JsonObject libraryJson;
			libraryJson["Name"] = library.name;
			libraryJson["Version"] = library.version;
			librariesJson.push_back(std::move(libraryJson));
		}

		json["OS"] = std::move(osJson);
		json["CPU"] = std::move(cpuJson);
		json["Network"] = std::move(networkJson);
		json["Tools"] = std::move(toolsJson);
		json["Libraries"] = std::move(librariesJson);

		return Json::Serialize(json);
	}

	std::string ToXmlStr(const SystemInfo& sysInfo)
	{
		xmlNodePtr rootNode = xmlNewNode(nullptr, BAD_CAST "value");
		xmlNodePtr structNode = xmlNewNode(nullptr, BAD_CAST "struct");
		xmlNodePtr osNode = xmlNewNode(nullptr, BAD_CAST "OS");
		xmlNodePtr cpuNode = xmlNewNode(nullptr, BAD_CAST "CPU");
		xmlNodePtr networkNode = xmlNewNode(nullptr, BAD_CAST "Network");
		xmlNodePtr toolsNode = xmlNewNode(nullptr, BAD_CAST "Tools");
		xmlNodePtr librariesNode = xmlNewNode(nullptr, BAD_CAST "Libraries");

		const auto& os = sysInfo.GetOSInfo();
		const auto& network = sysInfo.GetNetworkInfo();
		const auto& cpu = sysInfo.GetCPUInfo();
		const auto& tools = sysInfo.GetTools();
		const auto& libraries = sysInfo.GetLibraries();

		Xml::AddNewNode(osNode, "Name", "string", os.GetName().c_str());
		Xml::AddNewNode(osNode, "Version", "string", os.GetVersion().c_str());
		Xml::AddNewNode(networkNode, "PublicIP", "string", network.publicIP.c_str());
		Xml::AddNewNode(networkNode, "PrivateIP", "string", network.privateIP.c_str());
		Xml::AddNewNode(cpuNode, "Model", "string", cpu.GetModel().c_str());
		Xml::AddNewNode(cpuNode, "Arch", "string", cpu.GetArch().c_str());

		for (const auto& tool : tools)
		{
			Xml::AddNewNode(toolsNode, "Name", "string", tool.name.c_str());
			Xml::AddNewNode(toolsNode, "Version", "string", tool.version.c_str());
			Xml::AddNewNode(toolsNode, "Path", "string", tool.path.c_str());
		}

		for (const auto& library : libraries)
		{
			Xml::AddNewNode(librariesNode, "Name", "string", library.name.c_str());
			Xml::AddNewNode(librariesNode, "Version", "string", library.version.c_str());
		}

		xmlAddChild(structNode, osNode);
		xmlAddChild(structNode, cpuNode);
		xmlAddChild(structNode, networkNode);
		xmlAddChild(structNode, toolsNode);
		xmlAddChild(structNode, librariesNode);
		xmlAddChild(rootNode, structNode);

		std::string result = Xml::Serialize(rootNode);

		xmlFreeNode(rootNode);

		return result;
	}

}
