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
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "nzbget.h"

#include <boost/test/unit_test.hpp>

#include <filesystem>
#include "Extension.h"

BOOST_AUTO_TEST_CASE(ExtensionTest)
{
	Extension::Script script;
	ManifestFile::Option option;
	ManifestFile::Command command;

	script.SetAuthor("Author");
	script.SetDescription("Description");
	script.SetDisplayName("DisplayName");
	script.SetKind({ true, false, false, false });
	script.SetLicense("License");
	script.SetName("Name");
	script.SetQueueEvents("QueueEvents");
	script.SetTaskTime("TaskTime");
	script.SetVersion("Version");

	option.description = "description";
	option.displayName = "displayName";
	option.name = "name";
	option.value = "value";
	option.select = { "value", "value2" };
	option.type = "string";

	command.action = "action";
	command.name = "name";
	command.displayName = "displayName";
	command.description = "description";

	std::vector<ManifestFile::Option> options{ option };
	std::vector<ManifestFile::Command> commands{ command };

	script.SetOptions(std::move(options));
	script.SetCommands(std::move(commands));

	std::string result = Extension::ToJson(script);
	std::string expected = "{\"Location\":\"\",\
\"Name\":\"Name\",\
\"DisplayName\":\"DisplayName\",\
\"Description\":\"Description\",\
\"Author\":\"Author\",\
\"License\":\"License\",\
\"Version\":\"Version\",\
\"PostScript\":true,\
\"ScanScript\":false,\
\"QueueScript\":false,\
\"SchedulerScript\":false,\
\"FeedScript\":false,\
\"QueueEvents\":\"QueueEvents\",\
\"TaskTime\":\"TaskTime\",\
\"Options\":[{\"Type\":\"string\",\"Name\":\"name\",\"DisplayName\":\"displayName\",\"Description\":\"description\",\"Value\":\"value\",\"Select\":[\"value\",\"value2\"]}],\
\"Commands\":[{\"Name\":\"name\",\"DisplayName\":\"displayName\",\"Description\":\"description\",\"Action\":\"action\"}]}";

	BOOST_CHECK(result == expected);
}
