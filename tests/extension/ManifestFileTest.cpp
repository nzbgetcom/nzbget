/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2023-2024 Denis <denis@nzbget.com>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"

#include <boost/test/unit_test.hpp>

#include <string>
#include "FileSystem.h"
#include "ManifestFile.h"

BOOST_AUTO_TEST_CASE(ManifestFileTest)
{
	ManifestFile::Manifest manifestFile;
	const char* noFilePath = "";
	BOOST_CHECK(ManifestFile::Load(manifestFile, noFilePath) == false);
	BOOST_CHECK(manifestFile.main.empty());

	std::string dir = FileSystem::GetCurrentDirectory().Str() + std::string("/manifest");
	std::string invalidFilePath = dir + "/invalid";

	BOOST_REQUIRE(ManifestFile::Load(manifestFile, invalidFilePath.c_str()) == false);
	BOOST_CHECK(manifestFile.main.empty());

	std::string validFilePath = dir + "/valid";
	BOOST_REQUIRE(ManifestFile::Load(manifestFile, validFilePath.c_str()) == true);

	BOOST_CHECK(manifestFile.main == "email.py");
	BOOST_CHECK(manifestFile.name == "email");
	BOOST_CHECK(manifestFile.kind == "POST-PROCESSING");
	BOOST_CHECK(manifestFile.displayName == "Email");
	BOOST_CHECK(manifestFile.version == "1.0.0");
	BOOST_CHECK(manifestFile.nzbgetMinVersion == "23.1");
	BOOST_CHECK(manifestFile.author == "Author's name");
	BOOST_CHECK(manifestFile.homepage == "https://github");
	BOOST_CHECK(manifestFile.license == "GNU");
	BOOST_CHECK(manifestFile.about == "About");
	BOOST_CHECK(manifestFile.description == std::vector<std::string>({ "Description" }));
	BOOST_CHECK(manifestFile.queueEvents == "NZB_ADDED, NZB_DOWNLOADED, FILE_DOWNLOADED");
	BOOST_CHECK(manifestFile.taskTime == "1:00:00");

	BOOST_CHECK(manifestFile.requirements.size() == 1);
	BOOST_CHECK(manifestFile.requirements == std::vector<std::string>({ "This script requires Python to be installed on your system." }));

	BOOST_REQUIRE(manifestFile.options.size() == 3);

	auto& option = manifestFile.options[0];
	BOOST_CHECK(option.section.multi == false);
	BOOST_CHECK(option.section.prefix == "");
	BOOST_CHECK(option.section.name == "options");
	BOOST_CHECK(option.name == "sendMail");
	BOOST_CHECK(option.displayName == "SendMail");
	BOOST_CHECK(option.description == std::vector<std::string>({ "When to send the message." }));
	BOOST_CHECK(std::get<std::string>(option.value) == "Always");
	BOOST_CHECK(std::get<std::string>(option.select[0]) == "Always");
	BOOST_CHECK(std::get<std::string>(option.select[1]) == "OnFailure");

	auto& option2 = manifestFile.options[1];
	BOOST_CHECK(option2.section.multi == false);
	BOOST_CHECK(option2.section.prefix == "");
	BOOST_CHECK(option2.section.name == "options");
	BOOST_CHECK(option2.name == "port");
	BOOST_CHECK(option2.displayName == "Port");
	BOOST_CHECK(option2.description == std::vector<std::string>({ "SMTP server port (1-65535)" }));
	BOOST_CHECK(std::get<double>(option2.value) == 25.);
	BOOST_CHECK(std::get<double>(option2.select[0]) == 1.);
	BOOST_CHECK(std::get<double>(option2.select[1]) == 65535.);

	auto& option3 = manifestFile.options[2];
	BOOST_CHECK(option3.section.multi == true);
	BOOST_CHECK(option3.section.prefix == "Category");
	BOOST_CHECK(option3.section.name == "Categories");
	BOOST_CHECK(option3.name == "Category");
	BOOST_CHECK(option3.displayName == "Category");
	BOOST_CHECK(option3.description == std::vector<std::string>({ "Categories section" }));
	BOOST_CHECK(std::get<double>(option2.value) == 25.);

	BOOST_REQUIRE(manifestFile.commands.size() == 2);

	auto& command = manifestFile.commands[0];
	BOOST_CHECK(command.section.multi == false);
	BOOST_CHECK(command.section.prefix == "");
	BOOST_CHECK(command.section.name == "options");
	BOOST_CHECK(command.name == "connectionTest");
	BOOST_CHECK(command.action == "Send");
	BOOST_CHECK(command.displayName == "ConnectionTest");
	BOOST_CHECK(command.description == std::vector<std::string>({ "To check connection parameters click the button." }));

	auto& command2 = manifestFile.commands[1];
	BOOST_CHECK(command2.section.multi == false);
	BOOST_CHECK(command2.section.prefix == "Feed");
	BOOST_CHECK(command2.section.name == "Feeds");
	BOOST_CHECK(command2.name == "connectionTestTask");
	BOOST_CHECK(command2.action == "SendToTask");
	BOOST_CHECK(command2.displayName == "ConnectionTestTask");
	BOOST_CHECK(command2.description == std::vector<std::string>({ "Feeds command" }));
}
