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

#include "Extension.h"

Extension::Script GetExtension()
{
	Extension::Script script;
	ManifestFile::Option option;
	ManifestFile::Command command;

	script.SetEntry("/v1/main.py");
	script.SetLocation("/v1");
	script.SetRootDir("/");
	script.SetAuthor("Author");
	script.SetAbout("About");
	script.SetHomepage("Homepage");
	script.SetDescription({ "Description" });
	script.SetDisplayName("DisplayName");
	script.SetKind({ true, false, false, false });
	script.SetLicense("License");
	script.SetName("Name");
	script.SetRequirements({ "Requirements" });
	script.SetQueueEvents("QueueEvents");
	script.SetTaskTime("TaskTime");
	script.SetVersion("Version");

	option.description = { "description" };
	option.displayName = "displayName";
	option.name = "name";
	option.value = 5.;
	option.select = { 0., 10. };

	command.action = "action";
	command.name = "name";
	command.displayName = "displayName";
	command.description = { "description" };

	std::vector<ManifestFile::Option> options{ option };
	std::vector<ManifestFile::Command> commands{ command };

	script.SetOptions(std::move(options));
	script.SetCommands(std::move(commands));

	return script;
}

BOOST_AUTO_TEST_CASE(ToJsonStrTest)
{
	Extension::Script script = GetExtension();
	std::string result = Extension::ToJsonStr(script);
	std::string expected = "{\"Entry\":\"/v1/main.py\",\
\"Location\":\"/v1\",\
\"RootDir\":\"/\",\
\"Name\":\"Name\",\
\"DisplayName\":\"DisplayName\",\
\"About\":\"About\",\
\"Author\":\"Author\",\
\"Homepage\":\"Homepage\",\
\"License\":\"License\",\
\"Version\":\"Version\",\
\"PostScript\":true,\
\"ScanScript\":false,\
\"QueueScript\":false,\
\"SchedulerScript\":false,\
\"FeedScript\":false,\
\"QueueEvents\":\"QueueEvents\",\
\"TaskTime\":\"TaskTime\",\
\"Description\":[\"Description\"],\
\"Requirements\":[\"Requirements\"],\
\"Options\":[{\"Name\":\"name\",\"DisplayName\":\"displayName\",\"Value\":5E0,\"Description\":[\"description\"],\"Select\":[0E0,1E1]}],\
\"Commands\":[{\"Name\":\"name\",\"DisplayName\":\"displayName\",\"Action\":\"action\",\"Description\":[\"description\"]}]}";

	BOOST_CHECK(result == expected);
}

BOOST_AUTO_TEST_CASE(ToXmlStrTest)
{
	Extension::Script script = GetExtension();
	std::string result = Extension::ToXmlStr(script);
	std::string expected = "<value><struct>\
<member><name>Entry</name><value><string>/v1/main.py</string></value></member>\
<member><name>Location</name><value><string>/v1</string></value></member>\
<member><name>RootDir</name><value><string>/</string></value></member>\
<member><name>Name</name><value><string>Name</string></value></member>\
<member><name>DisplayName</name><value><string>DisplayName</string></value></member>\
<member><name>About</name><value><string>About</string></value></member>\
<member><name>Author</name><value><string>Author</string></value></member>\
<member><name>Homepage</name><value><string>Homepage</string></value></member>\
<member><name>License</name><value><string>License</string></value></member>\
<member><name>Version</name><value><string>Version</string></value></member>\
<member><name>PostScript</name><value><boolean>true</boolean></value></member>\
<member><name>ScanScript</name><value><boolean>false</boolean></value></member>\
<member><name>QueueScript</name><value><boolean>false</boolean></value></member>\
<member><name>SchedulerScript</name><value><boolean>false</boolean></value></member>\
<member><name>FeedScript</name><value><boolean>false</boolean></value></member>\
<member><name>QueueEvents</name><value><string>QueueEvents</string></value></member>\
<member><name>TaskTime</name><value><string>TaskTime</string></value></member>\
<Description>\
<member><name>Value</name><value><string>Description</string></value></member>\
</Description>\
<Requirements>\
<member><name>Value</name><value><string>Requirements</string></value></member>\
</Requirements>\
<Commands>\
<member><name>Name</name><value><string>name</string></value></member>\
<member><name>DisplayName</name><value><string>displayName</string></value></member>\
<member><name>Action</name><value><string>action</string></value></member>\
<Description>\
<member><name>Value</name><value><string>description</string></value></member>\
</Description>\
</Commands>\
<Options>\
<member><name>Name</name><value><string>name</string></value></member>\
<member><name>DisplayName</name><value><string>displayName</string></value></member>\
<member><name>Value</name><value><number>5.000000</number></value></member>\
<Description>\
<member><name>Value</name><value><string>description</string></value></member>\
</Description>\
<Select>\
<member><name>Value</name><value><number>0.000000</number></value></member>\
<member><name>Value</name><value><number>10.000000</number></value></member>\
</Select>\
</Options>\
</struct></value>";

	BOOST_CHECK(result == expected);
	xmlCleanupParser();
}
