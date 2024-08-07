/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include "boost/test/unit_test.hpp"

#include "Options.h"

class OptionsExtenderMock : public Options::Extender
{
public:
	int m_newsServers;
	int m_feeds;
	int m_tasks;

	OptionsExtenderMock() : m_newsServers(0), m_feeds(0), m_tasks(0) {}

protected:
	void AddNewsServer(int id, bool active, const char* name, const char* host,
		int port, int ipVersion, const char* user, const char* pass, bool joinGroup, bool tls,
		const char* cipher, int maxConnections, int retention, 
		int level, int group, bool optional, unsigned int certVerificationfLevel) override
	{
		m_newsServers++;
	}

	void AddFeed(int id, const char* name, const char* url, int interval,
		const char* filter, bool backlog, bool pauseNzb, const char* category, int priority, const char* feedScript) override
	{
		m_feeds++;
	}

	void AddTask(int id, int hours, int minutes, int weekDaysBits, Options::ESchedulerCommand command, const char* param) override
	{
		m_tasks++;
	}
};

BOOST_AUTO_TEST_CASE(OptionsInitWithoutConfigurationFileTest)
{
	Options options(nullptr, nullptr);

	options.CheckDirs();

	BOOST_CHECK(options.GetConfigFilename() == nullptr);
#ifdef WIN32
	BOOST_CHECK(strcmp(options.GetTempDir(), "nzbget/tmp") == 0);
#else
	BOOST_CHECK(strcmp(options.GetTempDir(), "~/downloads/tmp") == 0);
#endif
}

BOOST_AUTO_TEST_CASE(PassingCommandLineOptions)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ControlUsername=my-user-name-1");
	cmdOpts.push_back("ControlUsername=my-user-name-2");

	Options options(&cmdOpts, nullptr);

	BOOST_TEST(options.GetConfigFilename() == nullptr);
	BOOST_TEST(strcmp(options.GetControlUsername(), "my-user-name-2") == 0);
}

BOOST_AUTO_TEST_CASE(CallingExtender)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("Server1.Host=news.mynewsserver.com");
	cmdOpts.push_back("Server1.Port=119");
	cmdOpts.push_back("Server1.Connections=4");

	cmdOpts.push_back("Server2.Host=news1.mynewsserver.com");
	cmdOpts.push_back("Server2.Port=563");
	cmdOpts.push_back("Server2.Connections=2");

	cmdOpts.push_back("Feed1.Url=http://my.feed.com");

	cmdOpts.push_back("Task1.Time=*:00");
	cmdOpts.push_back("Task1.WeekDays=1-7");
	cmdOpts.push_back("Task1.Command=pausedownload");

	OptionsExtenderMock extender;
	Options options(&cmdOpts, &extender);

	BOOST_TEST(extender.m_newsServers == 2);
	BOOST_TEST(extender.m_feeds == 1);
	BOOST_TEST(extender.m_tasks == 24);
}
