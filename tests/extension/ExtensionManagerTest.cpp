/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2023 Denis <denis@nzbget.com>
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

#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>

#include "ExtensionManager.h"
#include "Options.h"
#include "Log.h"
#include "DiskState.h"

Log* g_Log;
Options* g_Options;
DiskState* g_DiskState;

static std::string nzbgetConf = "Extensions=EMail, Logger\n\
ScriptOrder=EMail, Logger\n\
EMail:SendMail=Always\n\
EMail:Port=25\n\
Logger:SendMail=Always\n\
Logger:Port=25\n\
Category1.Extensions=EMail\n\
Feed1.Extensions=EMail\n";
static std::string currentPath = std::filesystem::current_path().string();
static std::string testDir = currentPath + "/scripts";
static std::string configPath = currentPath + "/nzbget.conf";

class MockOptions : public IOptions {
public:
	const char* GetScriptDir() const override
	{
		return testDir.c_str();
	}

	const char* GetScriptOrder() const override
	{
		return "Extension2, Extension1, email;";
	}

	const char* GetExtensions() const override
	{
		return "Extension2, Extension3, Extension1, email;";
	}

	const char* GetConfigFilename() const override
	{
		return configPath.c_str();
	}
};

class MockOptions2 : public MockOptions {
public:
	const char* GetScriptOrder() const override
	{
		return "Email, Logger";
	}

	const char* GetExtensions() const override
	{
		return "Email, Logger";
	}
};

BOOST_AUTO_TEST_CASE(LoadExtesionsTest)
{
	MockOptions options;

	std::vector<std::string> order = { "Extension2", "Extension1", "email" };
	ExtensionManager::Manager manager;

	BOOST_REQUIRE(manager.LoadExtensions(options) == true);
	BOOST_CHECK(manager.GetExtensions().size() == 4);

	for (size_t i = 0; i < manager.GetExtensions().size(); ++i)
	{
		if (i < order.size())
		{
			BOOST_CHECK(order[i] == manager.GetExtensions()[i].GetName());
		}
	}
}

BOOST_AUTO_TEST_CASE(DeleteExtensionTest)
{
	MockOptions2 options;
	ExtensionManager::Manager manager;

	std::fstream fs(options.GetConfigFilename(), std::ios::out);

	BOOST_REQUIRE(fs.is_open());

	fs.write(nzbgetConf.c_str(), nzbgetConf.size());
	fs.close();
	fs.clear();

	BOOST_REQUIRE(manager.LoadExtensions(options) == true);
	BOOST_REQUIRE(manager.GetExtensions().size() == 4);

	BOOST_REQUIRE(manager.DeleteExtension("email", false, options) == true);
	auto extIt = std::find_if(
		std::begin(manager.GetExtensions()), 
		std::end(manager.GetExtensions()),
		[](const ExtensionManager::Extension& ext) { return ext.GetName() == "email"; }
	);
	BOOST_REQUIRE(extIt == std::end(manager.GetExtensions()));
	BOOST_REQUIRE(manager.GetExtensions().size() == 3);

	BOOST_REQUIRE(std::fstream(std::string(options.GetScriptDir()) + "/Email").is_open() == false);

	fs.open(options.GetConfigFilename(), std::ios::in);
	std::string line;
	std::getline(fs, line);
	BOOST_TEST_MESSAGE(line);
	BOOST_CHECK(line == "Extensions=Logger");
	std::getline(fs, line);
	BOOST_TEST_MESSAGE(line);
	BOOST_CHECK(line == "ScriptOrder=Logger");
	std::getline(fs, line);
	BOOST_TEST_MESSAGE(line);
	BOOST_CHECK(line == "EMail:SendMail=Always");
	std::getline(fs, line);
	BOOST_TEST_MESSAGE(line);
	BOOST_CHECK(line == "EMail:Port=25");
	std::getline(fs, line);
	BOOST_TEST_MESSAGE(line);
	BOOST_CHECK(line == "Logger:SendMail=Always");
	std::getline(fs, line);
	BOOST_TEST_MESSAGE(line);
	BOOST_CHECK(line == "Logger:Port=25");
	std::getline(fs, line);
	BOOST_TEST_MESSAGE(line);
	BOOST_CHECK(line == "Category1.Extensions=");
	std::getline(fs, line);
	BOOST_TEST_MESSAGE(line);
	BOOST_CHECK(line == "Feed1.Extensions=");
}
