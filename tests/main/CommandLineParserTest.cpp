/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include "CommandLineParser.h"

BOOST_AUTO_TEST_CASE(InitWithoutConfigurationFileTest)
{
	const char* argv[] = { "nzbget", "-n", "-p", nullptr };
	CommandLineParser commandLineParser(3, argv);

	BOOST_CHECK(commandLineParser.GetConfigFilename() == nullptr);
	BOOST_CHECK(commandLineParser.GetClientOperation() == CommandLineParser::opClientNoOperation);
}

BOOST_AUTO_TEST_CASE(InitializingWithtConfigurationFile)
{
	const char* argv[] = { "nzbget", "-c", "/home/user/nzbget.conf", "-p", nullptr };
	CommandLineParser commandLineParser(4, argv);

	BOOST_CHECK(commandLineParser.GetConfigFilename() != nullptr);
	BOOST_CHECK(strcmp(commandLineParser.GetConfigFilename(), "/home/user/nzbget.conf") == 0);
	BOOST_CHECK(commandLineParser.GetClientOperation() == CommandLineParser::opClientNoOperation);
}

BOOST_AUTO_TEST_CASE(ServerMode)
{
	const char* argv[] = { "nzbget", "-n", "-s", nullptr };
	CommandLineParser commandLineParser(3, argv);

	BOOST_CHECK(commandLineParser.GetServerMode() == true);
	BOOST_CHECK(commandLineParser.GetPauseDownload() == false);
}

BOOST_AUTO_TEST_CASE(PassingPause)
{
	const char* argv[] = { "nzbget", "-n", "-s", "-P", nullptr };
	CommandLineParser commandLineParser(4, argv);

	BOOST_CHECK(commandLineParser.GetPauseDownload() == true);
}

BOOST_AUTO_TEST_CASE(ExtraOption1)
{
	const char* argv[] = { "nzbget", "-n", "-o", "myoption1=yes", "-o", "myoption2=no", "-p", nullptr };
	CommandLineParser commandLineParser(7, argv);

	BOOST_CHECK(commandLineParser.GetOptionList()->size() == 2);
	BOOST_CHECK(strcmp(commandLineParser.GetOptionList()->at(0), "myoption1=yes") == 0);
	BOOST_CHECK(strcmp(commandLineParser.GetOptionList()->at(0), "myoption2=no") != 0);
	BOOST_CHECK(strcmp(commandLineParser.GetOptionList()->at(1), "myoption2=no") == 0);
}

BOOST_AUTO_TEST_CASE(ExtraOption2)
{
	const char* argv[] = { "nzbget", "-n", "-o", "myoption1=yes", "-o", "myoption2=no", "-o", "myoption1=no", "-p", nullptr };
	CommandLineParser commandLineParser(9, argv);

	BOOST_CHECK(commandLineParser.GetOptionList()->size() == 3);
	BOOST_CHECK(strcmp(commandLineParser.GetOptionList()->at(0), "myoption1=yes") == 0);
	BOOST_CHECK(strcmp(commandLineParser.GetOptionList()->at(1), "myoption2=no") == 0);
	BOOST_CHECK(strcmp(commandLineParser.GetOptionList()->at(2), "myoption1=no") == 0);
}

// TESTS: Add more tests for:
// - edit-command;
// - all other remote commands;
// - passing nzb-file in standalone mode;
