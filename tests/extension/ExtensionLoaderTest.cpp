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
#include "WorkState.h"
#include "ExtensionLoader.h"

WorkState* g_WorkState;

BOOST_AUTO_TEST_CASE(ExtensionV1LoaderTest)
{
	Extension::Script extension;
	std::string name = "Extension.py";
	std::string displayName = "Extension";
	std::string rootDir = std::filesystem::current_path().string();
	std::string location = rootDir + "/V1";
	std::string entry = location + "/Extension.py";

	extension.SetEntry(entry);
	extension.SetName(name);
	BOOST_TEST_MESSAGE(entry);
	BOOST_TEST_MESSAGE(location);
	BOOST_CHECK(ExtensionLoader::V1::Load(extension, location.c_str(), rootDir.c_str()) == true);

	BOOST_CHECK(extension.GetEntry() == entry);
	BOOST_CHECK(extension.GetLocation() == location);
	BOOST_CHECK(extension.GetRootDir() == rootDir);
	BOOST_CHECK(extension.GetName() == name);
	BOOST_CHECK(extension.GetDisplayName() == displayName);
	BOOST_CHECK(extension.GetHomepage() == std::string(""));
	BOOST_CHECK(extension.GetAbout() == std::string("About1.\nAbout2."));
	BOOST_CHECK(extension.GetDescription() == std::vector<std::string>({ "Description1", "Description2" }));
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

	BOOST_CHECK(extension.GetRequirements().size() == 1);
	BOOST_CHECK(extension.GetRequirements() == std::vector<std::string>({ "This script requires Python to be installed on your system." }));

	BOOST_CHECK(extension.GetCommands().size() == 1);

	auto command = extension.GetCommands()[0];
	BOOST_CHECK(command.name == "ConnectionTest");
	BOOST_CHECK(command.displayName == "ConnectionTest");
	BOOST_CHECK(command.action == "Send Test E-Mail");
	BOOST_CHECK(command.description == std::vector<std::string>({ "To check connection parameters click the button." }));

	BOOST_CHECK(extension.GetOptions().size() == 7);

	auto option = extension.GetOptions()[0];

	BOOST_CHECK(option.name == "FileList");
	BOOST_CHECK(option.displayName == "FileList");
	BOOST_CHECK(option.description == std::vector<std::string>({
		"Append list of files to the message.",
			"Add the list of downloaded files (the content of destination directory)."
		}));
	BOOST_CHECK(boost::variant2::get<std::string>(option.value) == "yes");
	BOOST_CHECK(boost::variant2::get<std::string>(option.select[0]) == "yes");
	BOOST_CHECK(boost::variant2::get<std::string>(option.select[1]) == "no");

	auto option2 = extension.GetOptions()[1];
	BOOST_CHECK(option2.name == "Port");
	BOOST_CHECK(option2.displayName == "Port");
	BOOST_CHECK(option2.description == std::vector<std::string>({ "SMTP server port (1-65535)." }));

	BOOST_CHECK(boost::variant2::get<double>(option2.value) == 25.);
	BOOST_CHECK(boost::variant2::get<double>(option2.select[0]) == 1.);
	BOOST_CHECK(boost::variant2::get<double>(option2.select[1]) == 65535.);

	auto option3 = extension.GetOptions()[2];
	BOOST_CHECK(option3.name == "SendMail");
	BOOST_CHECK(option3.displayName == "SendMail");
	BOOST_CHECK(option3.description == std::vector<std::string>({ "When to send the message." }));
	BOOST_CHECK(boost::variant2::get<std::string>(option3.value) == "Always");
	BOOST_CHECK(boost::variant2::get<std::string>(option3.select[0]) == "Always");
	BOOST_CHECK(boost::variant2::get<std::string>(option3.select[1]) == "OnFailure");

	auto option4 = extension.GetOptions()[3];
	BOOST_CHECK(option4.name == "Encryption");
	BOOST_CHECK(option4.displayName == "Encryption");
	BOOST_CHECK(option4.description == std::vector<std::string>({
		"Secure communication using TLS/SSL.",
			" no    - plain text communication (insecure);",
			" yes   - switch to secure session using StartTLS command;",
			" force - start secure session on encrypted socket."
		}));
	BOOST_CHECK(boost::variant2::get<std::string>(option4.value) == "yes");
	BOOST_CHECK(boost::variant2::get<std::string>(option4.select[0]) == "yes");
	BOOST_CHECK(boost::variant2::get<std::string>(option4.select[1]) == "no");
	BOOST_CHECK(boost::variant2::get<std::string>(option4.select[2]) == "force");

	auto option5 = extension.GetOptions()[4];
	BOOST_CHECK(option5.name == "To");
	BOOST_CHECK(option5.displayName == "To");
	BOOST_CHECK(option5.description == std::vector<std::string>({
		"Email address you want this email to be sent to.",
			"Multiple addresses can be separated with comma."
		}));
	BOOST_CHECK(boost::variant2::get<std::string>(option5.value) == "myaccount@gmail.com");
	BOOST_CHECK(option5.select.size() == 0);

	auto option6 = extension.GetOptions()[5];
	BOOST_CHECK(option6.name == "BannedExtensions");
	BOOST_CHECK(option6.displayName == "BannedExtensions");
	BOOST_CHECK(option6.description == std::vector<std::string>({
		"Banned extensions.",
			"Extensions must be separated by a comma (eg: .wmv, .divx)."
		}));
	BOOST_CHECK(boost::variant2::get<std::string>(option6.value) == "");
	BOOST_CHECK(option6.select.size() == 0);

	auto option7 = extension.GetOptions()[6];
	BOOST_CHECK(option7.name == "MoviesFormat");
	BOOST_CHECK(option7.displayName == "MoviesFormat");
	BOOST_CHECK(option7.description == std::vector<std::string>({
		"Common specifiers (for movies, series and dated tv shows):",
			"{TEXT}          - lowercase the text."
		}));
	BOOST_CHECK(boost::variant2::get<std::string>(option7.value) == "%t (%y)");
	BOOST_CHECK(option7.select.size() == 0);
}
