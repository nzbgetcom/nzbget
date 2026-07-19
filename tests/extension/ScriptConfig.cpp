/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Denis <denis@nzbget.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "nzbget.h"

#include <boost/test/unit_test.hpp>
#include <fstream>
#include <sstream>
#include "Options.h"
#include "ScriptConfig.h"

BOOST_AUTO_TEST_SUITE(ExtensionTest)

struct TempConfigFile
{
	fs::path path = fs::temp_directory_path() / fs::make_unique_filename();

	~TempConfigFile()
	{
		fs::error_code error;
		fs::remove(path, error);
	}
};

struct OptionsGuard
{
	Options* oldOptions = g_Options;

	~OptionsGuard()
	{
		g_Options = oldOptions;
	}
};

BOOST_AUTO_TEST_CASE(SaveConfigDoesNotAppendDuplicateOptionNames)
{
	TempConfigFile configFile;
	{
		std::ofstream output(configFile.path);
		output << "# existing config\n";
	}

	OptionsGuard optionsGuard;
	Options options("nzbget", configFile.path.string().c_str(), true, nullptr, nullptr);
	g_Options = &options;

	Options::OptEntries optEntries;
	optEntries.emplace_back("Extension.Option", "first");
	optEntries.emplace_back("extension.option", "second");
	ScriptConfig scriptConfig;

	BOOST_REQUIRE(scriptConfig.SaveConfig(&optEntries));
	std::string firstContents;
	{
		std::ifstream firstFile(configFile.path);
		std::stringstream contents;
		contents << firstFile.rdbuf();
		firstContents = contents.str();
	}
	BOOST_CHECK_EQUAL(firstContents, "# existing config\nExtension.Option=first\n");

	BOOST_REQUIRE(scriptConfig.SaveConfig(&optEntries));
	std::string secondContents;
	{
		std::ifstream secondFile(configFile.path);
		std::stringstream contents;
		contents << secondFile.rdbuf();
		secondContents = contents.str();
	}
	BOOST_CHECK_EQUAL(secondContents, firstContents);
}

BOOST_AUTO_TEST_SUITE_END()
