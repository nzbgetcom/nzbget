/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2025-2026 Denis <denis@nzbget.com>
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
#include <string>
#include <fstream>
#include "ExtensionScriptsValidator.h"
#include "ExtensionManager.h"
#include "Options.h"
#include "Util.h"
#include "FileSystem.h"

BOOST_AUTO_TEST_SUITE(SystemHealthTest)

using namespace SystemHealth::ExtensionScripts;
using namespace SystemHealth;

struct TempScriptDir
{
	fs::path path;
	TempScriptDir()
	{
		path = fs::temp_directory_path() / fs::make_unique_filename();
		fs::create_directory(path);
	}
	~TempScriptDir()
	{
		fs::error_code ec;
		fs::remove_all(path, ec);
	}
};

void CreateTestExtension(const fs::path& dir, const std::string& name, const std::string& kind)
{
	std::ofstream script((dir / (name + ".py")).string());
	script << "#!/bin/sh\n"
		   << "### NZBGET " << kind << " SCRIPT ###\n"
		   << "echo test\n";
	script.close();
}

BOOST_AUTO_TEST_CASE(TestExtensionListValidatorWithoutExt)
{
	TempScriptDir tempDir;

	CreateTestExtension(tempDir.path, "QueueSort", "QUEUE");
	CreateTestExtension(tempDir.path, "Completion", "POST-PROCESSING");

	std::string scriptDirOpt = std::string("ScriptDir=") + tempDir.path.string();
	std::string extensionsOpt = std::string("Extensions=") + "QueueSort,Completion";

	Options::CmdOptList cmdOpts;
	cmdOpts.push_back(scriptDirOpt.c_str());
	cmdOpts.push_back(extensionsOpt.c_str());
	cmdOpts.push_back("NzbLog=no");

	Options options(&cmdOpts, nullptr);

	Options* oldOptions = g_Options;
	g_Options = &options;

	ExtensionManager::Manager manager;
	BOOST_REQUIRE(manager.LoadExtensions() == std::nullopt);

	ExtensionListValidator validator(options, manager);
	Status status = validator.Validate();
	BOOST_CHECK_MESSAGE(status.IsOk(), "Validation failed: " << status.GetMessage());

	g_Options = oldOptions;
}

BOOST_AUTO_TEST_CASE(TestExtensionListValidatorWithExt)
{
	TempScriptDir tempDir;

	CreateTestExtension(tempDir.path, "QueueSort", "QUEUE");
	CreateTestExtension(tempDir.path, "Completion", "POST-PROCESSING");

	std::string scriptDirOpt = std::string("ScriptDir=") + tempDir.path.string();
	std::string extensionsOpt = std::string("Extensions=") + "QueueSort.py,Completion.py";

	Options::CmdOptList cmdOpts;
	cmdOpts.push_back(scriptDirOpt.c_str());
	cmdOpts.push_back(extensionsOpt.c_str());
	cmdOpts.push_back("NzbLog=no");

	Options options(&cmdOpts, nullptr);

	Options* oldOptions = g_Options;
	g_Options = &options;

	ExtensionManager::Manager manager;
	BOOST_REQUIRE(manager.LoadExtensions() == std::nullopt);

	ExtensionListValidator validator(options, manager);
	Status status = validator.Validate();
	BOOST_CHECK_MESSAGE(status.IsOk(), "Validation failed: " << status.GetMessage());

	g_Options = oldOptions;
}

BOOST_AUTO_TEST_SUITE_END()
