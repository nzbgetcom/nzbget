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

#include "FileSystem.h"
#include "Extension.h"
#include "WorkState.h"
#include "ExtensionLoader.h"

WorkState* g_WorkState;

BOOST_AUTO_TEST_CASE(ExtensionV1LoaderTest)
{
	Extension::Script extension;
	std::string name = "Extension.py";
	std::string displayName = "Extension";
	std::string rootDir = FileSystem::GetCurrentDirectory().Str();
	std::string location = rootDir + "/V1";
	std::string entry = location + "/Extension.py";

	extension.SetEntry(entry);
	extension.SetName(name);

	BOOST_REQUIRE(ExtensionLoader::V1::Load(extension, location.c_str(), rootDir.c_str()) == true);

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

	BOOST_REQUIRE(extension.GetCommands().size() == 3);

	auto command = extension.GetCommands()[0];
	BOOST_CHECK(command.name == "ConnectionTest");
	BOOST_CHECK(command.section.multi == false);
	BOOST_CHECK(command.section.name == "options");
	BOOST_CHECK(command.section.prefix == "");
	BOOST_CHECK(command.displayName == "ConnectionTest");
	BOOST_CHECK(command.action == "Send Test E-Mail");
	BOOST_CHECK(command.description == std::vector<std::string>({ "To check connection parameters click the button." }));

	auto command2 = extension.GetCommands()[1];
	BOOST_CHECK(command2.name == "Test");
	BOOST_CHECK(command2.section.multi == false);
	BOOST_CHECK(command2.section.name == "options");
	BOOST_CHECK(command2.section.prefix == "");
	BOOST_CHECK(command2.displayName == "Test");
	BOOST_CHECK(command2.action == "Send Test");
	BOOST_CHECK(command2.description == std::vector<std::string>({ "Test (0 1).", "description." }));

	auto command3 = extension.GetCommands()[2];
	BOOST_CHECK(command3.name == "TestFeed");
	BOOST_CHECK(command3.section.prefix == "Feed");
	BOOST_CHECK(command3.section.multi == true);
	BOOST_CHECK(command3.section.name == "FEEDS");
	BOOST_CHECK(command3.displayName == "TestFeed");
	BOOST_CHECK(command3.action == "Test Test");
	BOOST_CHECK(command3.description == std::vector<std::string>({ "Feed Test." }));

	BOOST_REQUIRE(extension.GetOptions().size() == 19);

	auto option = extension.GetOptions()[0];

	BOOST_CHECK(option.name == "FileList");
	BOOST_CHECK(option.displayName == "FileList");
	BOOST_CHECK(option.description == std::vector<std::string>({
		"Append list of files to the message.",
			"Add the list of downloaded files (the content of destination directory)."
		}));
	BOOST_CHECK(std::get<std::string>(option.value) == "yes");
	BOOST_CHECK(std::get<std::string>(option.select[0]) == "yes");
	BOOST_CHECK(std::get<std::string>(option.select[1]) == "no");

	auto option2 = extension.GetOptions()[1];
	BOOST_CHECK(option2.name == "Port");
	BOOST_CHECK(option2.displayName == "Port");
	BOOST_CHECK(option2.description == std::vector<std::string>({ "SMTP server port (1-65535)." }));
	BOOST_CHECK(std::get<double>(option2.value) == 25.);
	BOOST_CHECK(std::get<double>(option2.select[0]) == 1.);
	BOOST_CHECK(std::get<double>(option2.select[1]) == 65535.);

	auto option3 = extension.GetOptions()[2];
	BOOST_CHECK(option3.name == "SendMail");
	BOOST_CHECK(option3.displayName == "SendMail");
	BOOST_CHECK(option3.description == std::vector<std::string>({ "When to send the message." }));
	BOOST_CHECK(std::get<std::string>(option3.value) == "Always");
	BOOST_CHECK(std::get<std::string>(option3.select[0]) == "Always");
	BOOST_CHECK(std::get<std::string>(option3.select[1]) == "OnFailure");

	auto option4 = extension.GetOptions()[3];
	BOOST_CHECK(option4.name == "Encryption");
	BOOST_CHECK(option4.displayName == "Encryption");
	BOOST_CHECK(option4.description == std::vector<std::string>({
		"Secure communication using TLS/SSL.",
			" no    - plain text communication (insecure);",
			" yes   - switch to secure session using StartTLS command;",
			" force - start secure session on encrypted socket."
		}));
	BOOST_CHECK(std::get<std::string>(option4.value) == "yes");
	BOOST_CHECK(std::get<std::string>(option4.select[0]) == "yes");
	BOOST_CHECK(std::get<std::string>(option4.select[1]) == "no");
	BOOST_CHECK(std::get<std::string>(option4.select[2]) == "force");

	auto option5 = extension.GetOptions()[4];
	BOOST_CHECK(option5.name == "To");
	BOOST_CHECK(option5.displayName == "To");
	BOOST_CHECK(option5.description == std::vector<std::string>({
		"Email address you want this email to be sent to.",
			"Multiple addresses can be separated with comma."
		}));
	BOOST_CHECK(std::get<std::string>(option5.value) == "myaccount@gmail.com");
	BOOST_CHECK(option5.select.empty());

	auto option6 = extension.GetOptions()[5];
	BOOST_CHECK(option6.name == "BannedExtensions");
	BOOST_CHECK(option6.displayName == "BannedExtensions");
	BOOST_CHECK(option6.description == std::vector<std::string>({
		"Banned extensions.",
			"Extensions must be separated by a comma (eg: .wmv, .divx)."
		}));
	BOOST_CHECK(std::get<std::string>(option6.value) == "");
	BOOST_CHECK(option6.select.empty());

	auto option7 = extension.GetOptions()[6];
	BOOST_CHECK(option7.name == "MoviesFormat");
	BOOST_CHECK(option7.displayName == "MoviesFormat");
	BOOST_CHECK(option7.description == std::vector<std::string>({
		"Common specifiers (for movies, series and dated tv shows):",
			"{TEXT}          - lowercase the text."
		}));
	BOOST_CHECK(std::get<std::string>(option7.value) == "%t (%y)");
	BOOST_CHECK(option7.select.empty());

	auto option8 = extension.GetOptions()[7];
	BOOST_CHECK(option8.name == "outputVideoExtension");
	BOOST_CHECK(option8.displayName == "outputVideoExtension");
	BOOST_CHECK(option8.description == std::vector<std::string>({ "ffmpeg output settings." }));
	BOOST_CHECK(std::get<std::string>(option8.value) == ".mp4");
	BOOST_CHECK(option8.select.empty());

	auto option9 = extension.GetOptions()[8];
	BOOST_CHECK(option9.name == "outputVideoCodec");
	BOOST_CHECK(option9.displayName == "outputVideoCodec");
	BOOST_CHECK(option9.description.empty());
	BOOST_CHECK(std::get<std::string>(option9.value) == "libx264");
	BOOST_CHECK(option9.select.empty());

	auto option10 = extension.GetOptions()[9];
	BOOST_CHECK(option10.name == "outputDefault");
	BOOST_CHECK(option10.displayName == "outputDefault");
	BOOST_CHECK(option10.description == std::vector<std::string>({ "Output Default.", "description" }));
	BOOST_CHECK(std::get<std::string>(option10.value) == "None");
	BOOST_CHECK(std::get<std::string>(option10.select[0]) == "None");
	BOOST_CHECK(std::get<std::string>(option10.select[1]) == "iPad");
	BOOST_CHECK(std::get<std::string>(option10.select[2]) == "iPad-1080p");
	BOOST_CHECK(std::get<std::string>(option10.select[3]) == "iPad-720p");
	BOOST_CHECK(std::get<std::string>(option10.select[4]) == "Apple-TV2");
	BOOST_CHECK(std::get<std::string>(option10.select[5]) == "iPod");

	auto option11 = extension.GetOptions()[10];
	BOOST_CHECK(option11.name == "auto_update");
	BOOST_CHECK(option11.displayName == "auto_update");
	BOOST_CHECK(option11.description == std::vector<std::string>({
		"Auto Update nzbToMedia.",
		"description"
		}));
	BOOST_CHECK(std::get<std::string>(option11.value) == "0");
	BOOST_CHECK(std::get<std::string>(option11.select[0]) == "0");
	BOOST_CHECK(std::get<std::string>(option11.select[1]) == "1");

	auto option12 = extension.GetOptions()[11];
	BOOST_CHECK(option12.name == "niceness");
	BOOST_CHECK(option12.displayName == "niceness");
	BOOST_CHECK(option12.description == std::vector<std::string>({
		"Niceness for external extraction process.",
		"Set the Niceness value for the nice command (Linux). These range from -20 (most favorable to the process) to 19 (least favorable to the process)."
		}));
	BOOST_CHECK(std::get<std::string>(option12.value) == "10");
	BOOST_CHECK(option12.select.empty());

	auto option13 = extension.GetOptions()[12];
	BOOST_CHECK(option13.name == "ionice_class");
	BOOST_CHECK(option13.displayName == "ionice_class");
	BOOST_CHECK(option13.description == std::vector<std::string>({
		"ionice scheduling class.",
		"Set the ionice scheduling class (Linux). 0 for none, 1 for real time, 2 for best-effort, 3 for idle."
		}));
	BOOST_CHECK(std::get<std::string>(option13.value) == "2");
	BOOST_CHECK(std::get<std::string>(option13.select[0]) == "0");
	BOOST_CHECK(std::get<std::string>(option13.select[1]) == "1");
	BOOST_CHECK(std::get<std::string>(option13.select[2]) == "2");
	BOOST_CHECK(std::get<std::string>(option13.select[3]) == "3");

	auto option14 = extension.GetOptions()[13];
	BOOST_CHECK(option14.name == "ionice_class");
	BOOST_CHECK(option14.displayName == "ionice_class");
	BOOST_CHECK(option14.description == std::vector<std::string>({
		"ionice scheduling class.",
		"Set the ionice scheduling class (0, 1, 2, 3)."
		}));
	BOOST_CHECK(std::get<std::string>(option14.value) == "2");
	BOOST_CHECK(option14.select.empty());

	auto option15 = extension.GetOptions()[14];
	BOOST_CHECK(option15.name == "customPlexSection");
	BOOST_CHECK(option15.displayName == "customPlexSection");
	BOOST_CHECK(option15.description == std::vector<std::string>({
		"Custom Plex Section(s) you would like to update [Optional].",
		"Section Number(s) corresponding to your Plex library (comma separated)."
		}));
	BOOST_CHECK(std::get<std::string>(option15.value) == "");
	BOOST_CHECK(option15.select.empty());

	auto option16 = extension.GetOptions()[15];
	BOOST_CHECK(option16.name == "test2");
	BOOST_CHECK(option16.displayName == "test2");
	BOOST_CHECK(option16.description == std::vector<std::string>({
		"(Test2).",
		"description."
		}));
	BOOST_CHECK(std::get<std::string>(option16.value) == "");
	BOOST_CHECK(option16.select.empty());

	auto option17 = extension.GetOptions()[16];
	BOOST_CHECK(option17.section.name == "CATEGORIES");
	BOOST_CHECK(option17.section.multi == true);
	BOOST_CHECK(option17.section.prefix == "Category");
	BOOST_CHECK(option17.name == "Name");
	BOOST_CHECK(option17.displayName == "Name");
	BOOST_CHECK(option17.description == std::vector<std::string>({ "Name of the category to monitor." }));
	BOOST_CHECK(std::get<std::string>(option17.value) == "");
	BOOST_CHECK(option17.select.empty());

	auto option18 = extension.GetOptions()[17];
	BOOST_CHECK(option18.section.name == "CATEGORIES");
	BOOST_CHECK(option18.section.multi == true);
	BOOST_CHECK(option18.section.prefix == "Category");
	BOOST_CHECK(option18.name == "DownloadRate");
	BOOST_CHECK(option18.displayName == "DownloadRate");
	BOOST_CHECK(option18.description == std::vector<std::string>({ "Speed limit for that category (KB)." }));
	BOOST_CHECK(std::get<std::string>(option18.value) == "0");
	BOOST_CHECK(option18.select.empty());

	auto option19 = extension.GetOptions()[18];
	BOOST_CHECK(option19.section.name == "FEEDS");
	BOOST_CHECK(option19.section.multi == true);
	BOOST_CHECK(option19.section.prefix == "Feed");
	BOOST_CHECK(option19.name == "Name");
	BOOST_CHECK(option19.displayName == "Name");
	BOOST_CHECK(option19.description == std::vector<std::string>({ "Feed." }));
	BOOST_CHECK(std::get<std::string>(option19.value) == "");
	BOOST_CHECK(option19.select.empty());
}
