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

#include <boost/test/unit_test.hpp>

#include "SystemInfo.h"
#include "FileSystem.h"
#include "Options.h"
#include "Log.h"
#include "DiskState.h"

Log* g_Log;
Options* g_Options;
DiskState* g_DiskState;

std::string GetToolsJsonStr(const std::vector<System::Tool>& tools)
{
	std::string json = "\"Tools\":[";

	for (size_t i = 0; i < tools.size(); ++i)
	{
		std::string path = tools[i].path;
		for (size_t j = 0; j < path.length(); ++j) {
			if (path[j] == '\\')
			{
				path.insert(j, "\\");
				++j;
			}
		}

		json += "{\"Name\":\"" + tools[i].name +
			"\",\"Version\":\"" + tools[i].version +
			"\",\"Path\":\"" + path +
			"\"}";

		if (i != tools.size() - 1)
		{
			json += ",";
		}
	}

	json += "]";
	return json;
}

std::string GetLibrariesJsonStr(const std::vector<System::Library>& libs)
{
	std::string json = "\"Libraries\":[";

	for (size_t i = 0; i < libs.size(); ++i)
	{
		json += "{\"Name\":\"" + libs[i].name +
			"\",\"Version\":\"" + libs[i].version +
			"\"}";

		if (i != libs.size() - 1)
		{
			json += ",";
		}
	}

	json += "]";
	return json;
}

std::string GetToolsXmlStr(const std::vector<System::Tool>& tools)
{
	std::string xml = "<Tools>";

	auto GetXmlVal = [](const std::string& val) -> std::string
		{
			if (val.empty())
			{
				return "<string/>";
			}

			return "<string>" + val + "</string>";
		};

	for (size_t i = 0; i < tools.size(); ++i)
	{
		xml += "<member><name>Name</name><value>" + GetXmlVal(tools[i].name) +
			"</value></member>" +
			"<member><name>Version</name><value>" + GetXmlVal(tools[i].version) +
			"</value></member>" +
			"<member><name>Path</name><value>" + GetXmlVal(tools[i].path) +
			"</value></member>";
	}

	xml += "</Tools>";
	return xml;
}

std::string GetLibrariesXmlStr(const std::vector<System::Library>& libs)
{
	std::string xml = "<Libraries>";

	for (size_t i = 0; i < libs.size(); ++i)
	{
		xml += "<member><name>Name</name><value><string>" + libs[i].name +
			"</string></value></member>" +
			"<member><name>Version</name><value><string>" + libs[i].version +
			"</string></value></member>";
	}

	xml += "</Libraries>";
	return xml;
}

std::string GetNetworkXmlStr(const System::Network& network)
{
	std::string res = "<Network>";
	res += network.publicIP.empty()
		? "<member><name>PublicIP</name><value><string/></value></member>"
		: "<member><name>PublicIP</name><value><string>" + network.publicIP + "</string></value></member>";
		
	res += network.privateIP.empty()
		? "<member><name>PrivateIP</name><value><string/></value></member>"
		: "<member><name>PrivateIP</name><value><string>" + network.privateIP + "</string></value></member>";

	res += "</Network>";
	return res;
}

BOOST_AUTO_TEST_CASE(SystemInfoTest)
{
	Log log;
	DiskState ds;
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("SevenZipCmd=7z");
	cmdOpts.push_back("UnrarCmd=unrar");
	Options options(&cmdOpts, nullptr);

	g_Log = &log;
	g_DiskState = &ds;
	g_Options = &options;

	auto sysInfo = std::make_unique<System::SystemInfo>();

	std::string jsonStrResult = System::ToJsonStr(*sysInfo);
	std::string xmlStrResult = System::ToXmlStr(*sysInfo);

	std::string jsonStrExpected = "{\"OS\":{\"Name\":\"" + sysInfo->GetOSInfo().GetName() +
		"\",\"Version\":\"" + sysInfo->GetOSInfo().GetVersion() +
		"\"},\"CPU\":{\"Model\":\"" + sysInfo->GetCPUInfo().GetModel() +
		"\",\"Arch\":\"" + sysInfo->GetCPUInfo().GetArch() +
		"\"},\"Network\":{\"PublicIP\":\"" + sysInfo->GetNetworkInfo().publicIP +
		"\",\"PrivateIP\":\"" + sysInfo->GetNetworkInfo().privateIP +
		"\"}," + GetToolsJsonStr(sysInfo->GetTools()) + "," +
		GetLibrariesJsonStr(sysInfo->GetLibraries()) + "}";

	std::string xmlStrExpected = "<value><struct><OS><member><name>Name</name><value><string>" + sysInfo->GetOSInfo().GetName() +
		"</string></value></member>" +
		"<member><name>Version</name><value><string>" + sysInfo->GetOSInfo().GetVersion() +
		"</string></value></member></OS>" +
		"<CPU><member><name>Model</name><value><string>" + sysInfo->GetCPUInfo().GetModel() +
		"</string></value></member>" +
		"<member><name>Arch</name><value><string>" + sysInfo->GetCPUInfo().GetArch() +
		"</string></value></member></CPU>" +
		GetNetworkXmlStr(sysInfo->GetNetworkInfo()) +
		GetToolsXmlStr(sysInfo->GetTools()) +
		GetLibrariesXmlStr(sysInfo->GetLibraries()) +
		"</struct></value>";

	BOOST_TEST_MESSAGE("EXPECTED JSON STR: ");
	BOOST_TEST_MESSAGE(jsonStrExpected);

	BOOST_TEST_MESSAGE("RESULT JSON STR: ");
	BOOST_TEST_MESSAGE(jsonStrResult);

	BOOST_TEST_MESSAGE("EXPECTED XML STR: ");
	BOOST_TEST_MESSAGE(xmlStrExpected);

	BOOST_TEST_MESSAGE("RESULT XML STR: ");
	BOOST_TEST_MESSAGE(xmlStrResult);

	BOOST_CHECK(jsonStrResult == jsonStrExpected);
	BOOST_CHECK(xmlStrResult == xmlStrExpected);

	xmlCleanupParser();
}
