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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"

#include <boost/test/unit_test.hpp>

#include <filesystem>
#include <string>
#include "ManifestFile.h"

BOOST_AUTO_TEST_CASE(ManifestFileTest)
{
	ManifestFile::Manifest manifestFile;
	const char* noFilePath = "";
	BOOST_CHECK(ManifestFile::Load(manifestFile, noFilePath) == false);
	BOOST_CHECK(manifestFile.main.empty());

	std::string dir = std::filesystem::current_path().string() + "/manifest";
	std::string invalidFilePath = dir + "/invalid";

	BOOST_CHECK(ManifestFile::Load(manifestFile, invalidFilePath.c_str()) == false);
	BOOST_CHECK(manifestFile.main.empty());

	std::string validFilePath = dir + "/valid";
	BOOST_CHECK(ManifestFile::Load(manifestFile, validFilePath.c_str()) == true);

	BOOST_CHECK(manifestFile.main == "email.py");
	BOOST_CHECK(manifestFile.name == "email");
	BOOST_CHECK(manifestFile.kind == "POST-PROCESSING");
	BOOST_CHECK(manifestFile.displayName == "Email");
	BOOST_CHECK(manifestFile.version == "1.0.0");
	BOOST_CHECK(manifestFile.author == "Author's name");
	BOOST_CHECK(manifestFile.license == "GNU");
	BOOST_CHECK(manifestFile.description == "Description");
	BOOST_CHECK(manifestFile.queueEvents == "NZB_ADDED, NZB_DOWNLOADED, FILE_DOWNLOADED");
	BOOST_CHECK(manifestFile.taskTime == "1:00:00");
	auto& option = manifestFile.options[0];
	BOOST_CHECK(option.name == "sendMail");
	BOOST_CHECK(option.displayName == "SendMail");
	BOOST_CHECK(option.value == "Always");
	BOOST_CHECK(option.description == "When to send the message.");
	BOOST_CHECK(option.select[0] == "Always");
	BOOST_CHECK(option.select[1] == "OnFailure");
	auto& command = manifestFile.commands[0];
	BOOST_CHECK(command.name == "connectionTest");
	BOOST_CHECK(command.action == "Send");
	BOOST_CHECK(command.displayName == "ConnectionTest");
	BOOST_CHECK(command.description == "To check connection parameters click the button.");
}
