/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2023-2025 Denis <denis@nzbget.com>
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
#include "Options.h"

namespace fs = boost::filesystem;

const fs::path CURRENT_PATH = fs::current_path();
const fs::path SCRIPTS_DIR = CURRENT_PATH / "scripts";
const std::string SCRIPTS_DIR_OPT = "ScriptDir=" + SCRIPTS_DIR.string();
const std::string EXTENSIONS = std::string("Extensions=") + "Extension2, Extension1, email; Extension1; Extension1";
const std::string ORDER = std::string("ScriptOrder=") + "Extension2, Extension1, email; Extension1; Extension1";

BOOST_AUTO_TEST_CASE(LoadExtensionsTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back(SCRIPTS_DIR_OPT.c_str());
	cmdOpts.push_back(EXTENSIONS.c_str());
	cmdOpts.push_back(ORDER.c_str());
	cmdOpts.push_back("NzbLog=no");
	Options options(&cmdOpts, nullptr);
	g_Options = &options;

	std::vector<std::string> correctOrder = { "Extension2", "Extension1", "email" };
	ExtensionManager::Manager manager;

	BOOST_REQUIRE(manager.LoadExtensions() == std::nullopt);
	size_t idx = 0;
	manager.ForEach([&](auto script)
		{
			if (idx < correctOrder.size())
			{
				BOOST_CHECK_EQUAL(correctOrder[idx], script->GetName());
			}
			++idx;
		}
	);
	BOOST_CHECK_EQUAL(idx, 4);
}

BOOST_AUTO_TEST_CASE(ShouldNotDeleteExtensionIfExtensionIsBusyTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back(SCRIPTS_DIR_OPT.c_str());
	cmdOpts.push_back(EXTENSIONS.c_str());
	cmdOpts.push_back(ORDER.c_str());
	cmdOpts.push_back("NzbLog=no");
	Options options(&cmdOpts, nullptr);
	g_Options = &options;
	ExtensionManager::Manager manager;

	BOOST_REQUIRE(manager.LoadExtensions() == std::nullopt);

	const auto busyExt = manager.FindIf([](auto script) { return true; });
	auto error = manager.DeleteExtension((*busyExt)->GetName());

	BOOST_CHECK_EQUAL(error.has_value(), true);
	BOOST_CHECK_EQUAL(error.value(), std::string("Deletion failed: Extension '") + (*busyExt)->GetName() + "' is currently in use");
}

BOOST_AUTO_TEST_CASE(DeleteExtensionTest)
{
	const fs::path workingDir = CURRENT_PATH / "DeleteExtensionTest_dir";
	const std::string scriptDirOpt = "ScriptDir=" + workingDir.string();

	BOOST_REQUIRE(fs::create_directory(workingDir));
	fs::copy(SCRIPTS_DIR, workingDir, fs::copy_options::recursive);

	Options::CmdOptList cmdOpts;
	cmdOpts.push_back(scriptDirOpt.c_str());
	cmdOpts.push_back(EXTENSIONS.c_str());
	cmdOpts.push_back(ORDER.c_str());
	cmdOpts.push_back("NzbLog=no");
	Options options(&cmdOpts, nullptr);
	g_Options = &options;
	ExtensionManager::Manager manager;

	BOOST_REQUIRE(manager.LoadExtensions() == std::nullopt);

	auto res = manager.DeleteExtension("email");
	const auto found = manager.FindIf([](auto script) 
		{ 
			return std::string("email") == script->GetName(); 
		}
	);

	BOOST_CHECK_EQUAL(res.has_value(), false);
	BOOST_CHECK_EQUAL(found.has_value(), false);
	BOOST_CHECK_EQUAL(fs::exists("Email"), false);

	fs::remove_all(workingDir);
}
