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
	Extension::Script extension;
	std::string name = "Extension.py";
	std::string displayName = "Extension";
	std::string location = std::filesystem::current_path().string() + "/V1/Extension.py";

	extension.SetLocation(location.c_str());
	extension.SetName(name.c_str());
	extension.SetDisplayName(displayName.c_str());

	BOOST_CHECK(ExtensionLoader::V1::Load(extension) == true);

	BOOST_CHECK(extension.GetName() == name);
	BOOST_CHECK(extension.GetDisplayName() == displayName);
	BOOST_CHECK(extension.GetLocation() == location);
	BOOST_CHECK(extension.GetHomepage() == std::string("https://github"));
	BOOST_CHECK(extension.GetAbout() == std::string("About1.\nAbout2."));
	BOOST_CHECK(extension.GetDescription() == std::string("Description1\nDescription2"));
	BOOST_CHECK(extension.GetTaskTime() == std::string("*;*:00;*:30"));
	BOOST_CHECK(extension.GetQueueEvents() == std::string("NZB_ADDED, NZB_DOWNLOADED, FILE_DOWNLOADED"));
	BOOST_CHECK(extension.GetPostScript() == true);
	BOOST_CHECK(extension.GetQueueScript() == true);
	BOOST_CHECK(extension.GetFeedScript() == false);
	BOOST_CHECK(extension.GetScanScript() == false);
	BOOST_CHECK(extension.GetSchedulerScript() == false);
	BOOST_CHECK(extension.GetAuthor() == std::string(""));
	BOOST_CHECK(extension.GetLicense() == std::string(""));
	BOOST_CHECK(extension.GetVersion() == std::string(""));

	BOOST_CHECK(extension.GetCommands().size() == 1);

	auto command = extension.GetCommands()[0];
	BOOST_CHECK(command.name == std::string("ConnectionTest"));
	BOOST_CHECK(command.displayName == std::string("ConnectionTest"));
	BOOST_CHECK(command.action == std::string("Send Test E-Mail"));
	BOOST_CHECK(command.description == std::string("To check connection parameters click the button."));

	BOOST_CHECK(extension.GetOptions().size() == 6);

	auto option = extension.GetOptions()[0];
	BOOST_CHECK(option.type == std::string("string"));
	BOOST_CHECK(option.name == std::string("FileList"));
	BOOST_CHECK(option.displayName == std::string("FileList"));
	BOOST_CHECK(option.description == std::string("Append list of files to the message.\nAdd the list of downloaded files (the content of destination directory)."));
	BOOST_CHECK(option.value == std::string("yes"));
	BOOST_CHECK(option.select[0] == std::string("yes"));
	BOOST_CHECK(option.select[1] == std::string("no"));

	auto option2 = extension.GetOptions()[1];
	BOOST_CHECK(option2.type == std::string("number"));
	BOOST_CHECK(option2.name == std::string("Port"));
	BOOST_CHECK(option2.displayName == std::string("Port"));
	BOOST_CHECK(option2.description == std::string("SMTP server port (1-65535)."));
	BOOST_CHECK(option2.value == std::string("25"));
	BOOST_CHECK(option2.select[0] == std::string("1"));
	BOOST_CHECK(option2.select[1] == std::string("65535"));

	auto option3 = extension.GetOptions()[2];
	BOOST_CHECK(option3.type == std::string("string"));
	BOOST_CHECK(option3.name == std::string("SendMail"));
	BOOST_CHECK(option3.displayName == std::string("SendMail"));
	BOOST_CHECK(option3.description == std::string("When to send the message."));
	BOOST_CHECK(option3.value == std::string("Always"));
	BOOST_CHECK(option3.select[0] == std::string("Always"));
	BOOST_CHECK(option3.select[1] == std::string("OnFailure"));

	auto option4 = extension.GetOptions()[3];
	BOOST_CHECK(option4.type == std::string("string"));
	BOOST_CHECK(option4.name == std::string("Encryption"));
	BOOST_CHECK(option4.displayName == std::string("Encryption"));
	BOOST_CHECK(option4.description == std::string("Secure communication using TLS/SSL.\n no    - plain text communication (insecure);\n yes   - switch to secure session using StartTLS command;\n force - start secure session on encrypted socket."));
	BOOST_CHECK(option4.value == std::string("yes"));
	BOOST_CHECK(option4.select[0] == std::string("yes"));
	BOOST_CHECK(option4.select[1] == std::string("no"));
	BOOST_CHECK(option4.select[2] == std::string("force"));

	auto option5 = extension.GetOptions()[4];
	BOOST_CHECK(option5.type == std::string("string"));
	BOOST_CHECK(option5.name == std::string("To"));
	BOOST_CHECK(option5.displayName == std::string("To"));
	BOOST_CHECK(option5.description == std::string("Email address you want this email to be sent to.\nMultiple addresses can be separated with comma."));
	BOOST_CHECK(option5.value == std::string("myaccount@gmail.com"));
	BOOST_CHECK(option5.select.size() == 0);

	auto option6 = extension.GetOptions()[5];
	BOOST_CHECK(option6.type == std::string("string"));
	BOOST_CHECK(option6.name == std::string("BannedExtensions"));
	BOOST_CHECK(option6.displayName == std::string("BannedExtensions"));
	BOOST_CHECK(option6.description == std::string("Banned extensions.\nExtensions must be separated by a comma (eg: .wmv, .divx)."));
	BOOST_CHECK(option6.value == std::string(""));
	BOOST_CHECK(option6.select.size() == 0);
}
