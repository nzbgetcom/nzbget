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

char* (*g_EnvironmentVariables)[] = nullptr;
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
		return "Extension2, Extension1, email; Extension1; Extension1";
	}

	const char* GetExtensions() const override
	{
		return "Extension2, Extension3, Extension1, email";
	}

	const char* GetConfigFilename() const override
	{
		return configPath.c_str();
	}
};

BOOST_AUTO_TEST_CASE(LoadExtesionsTest)
{
	MockOptions options;

	std::vector<std::string> correctOrder = { "Extension2", "Extension1", "email" };
	ExtensionManager::Manager manager;

	BOOST_REQUIRE(manager.LoadExtensions(options) == boost::none);
	BOOST_CHECK(manager.GetExtensions().size() == 4);

	for (size_t i = 0; i < manager.GetExtensions().size(); ++i)
	{
		if (i < correctOrder.size())
		{
			BOOST_CHECK(correctOrder[i] == manager.GetExtensions()[i]->GetName());
		}
	}
}

BOOST_AUTO_TEST_CASE(ShouldNotDeleteExtensionIfExtensionIsBusyTest)
{
	MockOptions options;
	ExtensionManager::Manager manager;

	BOOST_REQUIRE(manager.LoadExtensions(options) == boost::none);

	const auto busyExt = manager.GetExtensions()[0];

	auto error = manager.DeleteExtension(busyExt->GetName());

	BOOST_CHECK(error.has_value() == true);
	BOOST_CHECK(error.get() == "Failed to delete: " + std::string(busyExt->GetName()) + " is executing");
}
