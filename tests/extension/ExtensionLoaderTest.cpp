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
#include "Extension.h"
#include "ExtensionLoader.h"

BOOST_AUTO_TEST_CASE(ExtensionV1LoaderTest)
{
	Extension::Script script;
	std::string name = "Extension.py";
	std::string displayName = "Extension";
	std::string location = std::filesystem::current_path().string() + "/V1/Extension.py";

	script.SetLocation(location.c_str());
	script.SetName(name.c_str());
	script.SetDisplayName(displayName.c_str());

	BOOST_CHECK(ExtensionLoader::V1::Load(script) == true);

	BOOST_CHECK(script.GetName() == name);
	BOOST_CHECK(script.GetDisplayName() == displayName);
	BOOST_CHECK(script.GetLocation() == location);
	BOOST_CHECK(script.GetDescription() == std::string("Description1\nDescription2\n"));
	BOOST_CHECK(script.GetTaskTime() == std::string("*;*:00;*:30"));
	BOOST_CHECK(script.GetQueueEvents() == std::string("NZB_ADDED, NZB_DOWNLOADED, FILE_DOWNLOADED"));
	BOOST_CHECK(script.GetPostScript() == true);
	BOOST_CHECK(script.GetQueueScript() == true);
	BOOST_CHECK(script.GetFeedScript() == false);
	BOOST_CHECK(script.GetScanScript() == false);
	BOOST_CHECK(script.GetSchedulerScript() == false);
	BOOST_CHECK(script.GetAuthor() == std::string(""));
	BOOST_CHECK(script.GetLicense() == std::string(""));
	BOOST_CHECK(script.GetVersion() == std::string(""));

	auto command = script.GetCommands()[0];
	BOOST_CHECK(command.name == std::string("ConnectionTest"));
	BOOST_CHECK(command.displayName == std::string("ConnectionTest"));
	BOOST_CHECK(command.action == std::string("Send Test E-Mail"));
	BOOST_CHECK(command.description == std::string("To check connection parameters click the button.\n"));

	auto option = script.GetOptions()[0];
	BOOST_CHECK(option.name == std::string("FileList"));
	BOOST_CHECK(option.displayName == std::string("FileList"));
	BOOST_CHECK(option.description == std::string("Append list of files to the message.\nAdd the list of downloaded files (the content of destination directory).\n"));
	BOOST_CHECK(option.value == std::string("yes"));
	BOOST_CHECK(option.select[0] == std::string("yes"));
	BOOST_CHECK(option.select[1] == std::string("no"));

	auto option2 = script.GetOptions()[1];
	BOOST_CHECK(option2.name == std::string("Port"));
	BOOST_CHECK(option2.displayName == std::string("Port"));
	BOOST_CHECK(option2.description == std::string("SMTP server port (1-65535).\n"));
	BOOST_CHECK(option2.value == std::string("25"));
	BOOST_CHECK(option2.select.size() == 0);

	auto option3 = script.GetOptions()[2];
	BOOST_CHECK(option3.name == std::string("SendMail"));
	BOOST_CHECK(option3.displayName == std::string("SendMail"));
	BOOST_CHECK(option3.description == std::string("When to send the message.\n"));
	BOOST_CHECK(option3.value == std::string("Always"));
	BOOST_CHECK(option3.select[0] == std::string("Always"));
	BOOST_CHECK(option3.select[1] == std::string("OnFailure"));

	auto option4 = script.GetOptions()[3];
	BOOST_CHECK(option4.name == std::string("Encryption"));
	BOOST_CHECK(option4.displayName == std::string("Encryption"));
	BOOST_CHECK(option4.description == std::string("Secure communication using TLS/SSL.\n no    - plain text communication (insecure);\n yes   - switch to secure session using StartTLS command;\n force - start secure session on encrypted socket.\n"));
	BOOST_CHECK(option4.value == std::string("yes"));
	BOOST_CHECK(option4.select[0] == std::string("yes"));
	BOOST_CHECK(option4.select[1] == std::string("no"));
	BOOST_CHECK(option4.select[2] == std::string("force"));
}
