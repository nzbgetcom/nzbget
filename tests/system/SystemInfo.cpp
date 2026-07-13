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

#include <boost/test/unit_test.hpp>
#include "SystemInfo.h"

BOOST_AUTO_TEST_SUITE(SystemTest)

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

std::string BlankOutPublicIP(std::string str, const std::string& from, const std::string& to)
{
	size_t start = str.find(from);
	if (start == std::string::npos)
	{
		return str;
	}

	start += from.length();

	size_t end = str.find(to, start);
	if (end == std::string::npos)
	{
		return str;
	}

	str.erase(start, end - start);
	return str;
}

BOOST_AUTO_TEST_CASE(SystemInfoTest)
{
	auto sysInfo = std::make_unique<System::SystemInfo>();

	std::string jsonStrResult = System::ToJsonStr(*sysInfo);
	std::string xmlStrResult = System::ToXmlStr(*sysInfo);

	System::Network network = sysInfo->GetNetworkInfo();

	std::string jsonStrExpected = "{\"OS\":{\"Name\":\"" + sysInfo->GetOSInfo().GetName() +
		"\",\"Version\":\"" + sysInfo->GetOSInfo().GetVersion() +
		"\"},\"CPU\":{\"Model\":\"" + sysInfo->GetCPUInfo().GetModel() +
		"\",\"Arch\":\"" + sysInfo->GetCPUInfo().GetArch() +
		"\"},\"Network\":{\"PublicIP\":\"" + network.publicIP +
		"\",\"PrivateIP\":\"" + network.privateIP +
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
		GetNetworkXmlStr(network) +
		GetToolsXmlStr(sysInfo->GetTools()) +
		GetLibrariesXmlStr(sysInfo->GetLibraries()) +
		"</struct></value>";

	// ToJsonStr and ToXmlStr fetch the public IP themselves, and it may change
	// between fetches (e.g. rotating NAT egress IPs on CI runners),
	// so blank it out on both sides before comparing
	jsonStrResult = BlankOutPublicIP(std::move(jsonStrResult), "\"PublicIP\":\"", "\"");
	jsonStrExpected = BlankOutPublicIP(std::move(jsonStrExpected), "\"PublicIP\":\"", "\"");
	xmlStrResult = BlankOutPublicIP(std::move(xmlStrResult), "<name>PublicIP</name>", "</value>");
	xmlStrExpected = BlankOutPublicIP(std::move(xmlStrExpected), "<name>PublicIP</name>", "</value>");

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

BOOST_AUTO_TEST_SUITE_END()
