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
	// one line only, carrying the last entry's value - Options::SetOption
	// resolves duplicates the same way, so the file matches what nzbget loads
	BOOST_CHECK_EQUAL(firstContents, "# existing config\nExtension.Option=second\n");

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

BOOST_AUTO_TEST_CASE(SaveConfigRemovesDuplicateLinesFromDamagedConfig)
{
	TempConfigFile configFile;
	{
		std::ofstream output(configFile.path);
		output << "# existing config\n"
			<< "Server1.Active=yes\n"
			<< "Server2.Active=yes\n"
			<< "Server2.Active=yes\n"
			<< "server2.active=yes\n"
			<< "Server2.Host=news.example.com\n";
	}

	OptionsGuard optionsGuard;
	Options options("nzbget", configFile.path.string().c_str(), true, nullptr, nullptr);
	g_Options = &options;

	Options::OptEntries optEntries;
	optEntries.emplace_back("Server1.Active", "yes");
	optEntries.emplace_back("Server2.Active", "no");
	optEntries.emplace_back("Server2.Host", "news.example.com");
	ScriptConfig scriptConfig;

	BOOST_REQUIRE(scriptConfig.SaveConfig(&optEntries));
	std::string contents;
	{
		std::ifstream file(configFile.path);
		std::stringstream buffer;
		buffer << file.rdbuf();
		contents = buffer.str();
	}
	BOOST_CHECK_EQUAL(contents,
		"# existing config\n"
		"Server1.Active=yes\n"
		"Server2.Active=no\n"
		"Server2.Host=news.example.com\n");
}

// When a config file carries duplicate lines for one option, Options::SetOption
// overwrites the same entry while parsing, so the LAST line is the value nzbget
// actually runs on. Collapsing the duplicates must preserve that value,
// otherwise saving a damaged config silently changes settings (issue #588).
BOOST_AUTO_TEST_CASE(SaveConfigKeepsLastValueWhenConfigHasDuplicateLines)
{
	TempConfigFile configFile;
	{
		std::ofstream output(configFile.path);
		output << "# existing config\n"
			<< "Server2.Active=yes\n"
			<< "Server2.Name=xsusenet\n"
			<< "server2.active=no\n";
	}

	OptionsGuard optionsGuard;
	Options options("nzbget", configFile.path.string().c_str(), true, nullptr, nullptr);
	g_Options = &options;

	// as loadconfig reports it: raw file lines, duplicates included
	Options::OptEntries optEntries;
	optEntries.emplace_back("Server2.Active", "yes");
	optEntries.emplace_back("Server2.Name", "xsusenet");
	optEntries.emplace_back("server2.active", "no");
	ScriptConfig scriptConfig;

	BOOST_REQUIRE(scriptConfig.SaveConfig(&optEntries));
	std::string contents;
	{
		std::ifstream file(configFile.path);
		std::stringstream buffer;
		buffer << file.rdbuf();
		contents = buffer.str();
	}
	// one line per option, keeping the first occurrence's name and position
	// but the last occurrence's value - exactly what Options::SetOption does
	BOOST_CHECK_EQUAL(contents,
		"# existing config\n"
		"Server2.Active=no\n"
		"Server2.Name=xsusenet\n");
}

BOOST_AUTO_TEST_SUITE_END()
