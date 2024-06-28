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

#include <iostream>
#include <fstream>
#include <string>

#include "ExtensionManager.h"
#include "FileSystem.h"
#include "Options.h"
#include "Log.h"
#include "DiskState.h"

char* (*g_EnvironmentVariables)[] = nullptr;
Log* g_Log;
Options* g_Options;
DiskState* g_DiskState;

static std::string currentPath = FileSystem::GetCurrentDirectory().Str();
static std::string testDir = "ScriptDir=" + currentPath + "/scripts";
static std::string extensions = std::string("Extensions=") + "Extension2, Extension1, email; Extension1; Extension1";
static std::string scriptOrder = std::string("ScriptOrder=") + "Extension2, Extension1, email; Extension1; Extension1";

BOOST_AUTO_TEST_CASE(LoadExtesionsTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back(testDir.c_str());
	cmdOpts.push_back(extensions.c_str());
	cmdOpts.push_back(scriptOrder.c_str());
	cmdOpts.push_back("NzbLog=no");
	Options options(&cmdOpts, nullptr);
	g_Options = &options;

	std::vector<std::string> correctOrder = { "Extension2", "Extension1", "email" };
	ExtensionManager::Manager manager;

	BOOST_REQUIRE(manager.LoadExtensions() == std::nullopt);
	BOOST_REQUIRE(manager.GetExtensions().size() == 4);

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
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back(testDir.c_str());
	cmdOpts.push_back(extensions.c_str());
	cmdOpts.push_back(scriptOrder.c_str());
	cmdOpts.push_back("NzbLog=no");
	Options options(&cmdOpts, nullptr);
	g_Options = &options;
	ExtensionManager::Manager manager;

	BOOST_REQUIRE(manager.LoadExtensions() == std::nullopt);

	const auto busyExt = manager.GetExtensions()[0];

	auto error = manager.DeleteExtension(busyExt->GetName());

	BOOST_CHECK(error.has_value() == true);
	BOOST_CHECK(error.value() == "Failed to delete: " + std::string(busyExt->GetName()) + " is executing");
}
