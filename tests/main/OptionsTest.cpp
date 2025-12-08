/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2025 Denis <denis@nzbget.com>
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

#include "boost/test/unit_test.hpp"

#include <vector>
#include <iostream>
#include "Options.h"
#include "FeedInfo.h"

std::ostream operator<<(std::ostream& os, FeedInfo::CategorySource categorySource)
{
	return os << categorySource;
}

class OptionsExtenderMock : public Options::Extender
{
public:
	int m_newsServers;
	int m_feeds;
	int m_tasks;
	std::vector<FeedInfo::CategorySource> m_categorySources;

	OptionsExtenderMock() : m_newsServers(0), m_feeds(0), m_tasks(0) {}

protected:
	void AddNewsServer(int id, bool active, const char* name, const char* host,
		int port, int ipVersion, const char* user, const char* pass, bool joinGroup, bool tls,
		const char* cipher, int maxConnections, int retention, 
		int level, int group, bool optional, unsigned int certVerificationfLevel) override
	{
		m_newsServers++;
	}

	void AddFeed(
		int id, 
		const char* name, 
		const char* url, 
		int interval,
		const char* filter, 
		bool backlog, 
		bool pauseNzb, 
		const char* category,
		FeedInfo::CategorySource categorySource, 
		int priority, 
		const char* feedScript
	) override
	{
		m_feeds++;
		m_categorySources.push_back(categorySource);
	}

	void AddTask(int id, int hours, int minutes, int weekDaysBits, Options::ESchedulerCommand command, const char* param) override
	{
		m_tasks++;
	}
};

BOOST_AUTO_TEST_CASE(OptionsInitWithoutConfigurationFileTest)
{
	Options options(nullptr, nullptr);

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

	BOOST_CHECK_EQUAL(extender.m_newsServers, 2);
	BOOST_CHECK_EQUAL(extender.m_feeds, 1);
	BOOST_CHECK_EQUAL(extender.m_tasks, 24);
	BOOST_CHECK_EQUAL(extender.m_categorySources[0], FeedInfo::CategorySource::NZBFile);
}

BOOST_AUTO_TEST_CASE(ParseCategorySourceTest)
{
	Options::CmdOptList cmdOpts;

	cmdOpts.push_back("Feed1.Url=http://my.feed.com");
	cmdOpts.push_back("Feed1.CategorySource=Auto");

	cmdOpts.push_back("Feed2.Url=http://my.feed2.com");
	cmdOpts.push_back("Feed2.CategorySource=NZBFile");

	cmdOpts.push_back("Feed3.Url=http://my.feed3.com");
	cmdOpts.push_back("Feed3.CategorySource=FeedFile");

	cmdOpts.push_back("Feed4.Url=http://my.feed4.com");
	cmdOpts.push_back("Feed4.CategorySource=auto");

	cmdOpts.push_back("Feed5.Url=http://my.feed5.com");
	cmdOpts.push_back("Feed5.CategorySource=nzbfile");

	cmdOpts.push_back("Feed6.Url=http://my.feed6.com");
	cmdOpts.push_back("Feed6.CategorySource=feedfile");

	cmdOpts.push_back("Feed7.Url=http://my.feed7.com");
	cmdOpts.push_back("Feed7.CategorySource=auto and nzbfile and feedfile");

	OptionsExtenderMock extender;
	Options options(&cmdOpts, &extender);

	BOOST_CHECK_EQUAL(extender.m_categorySources[0], FeedInfo::CategorySource::Auto);
	BOOST_CHECK_EQUAL(extender.m_categorySources[1], FeedInfo::CategorySource::NZBFile);
	BOOST_CHECK_EQUAL(extender.m_categorySources[2], FeedInfo::CategorySource::FeedFile);
	BOOST_CHECK_EQUAL(extender.m_categorySources[3], FeedInfo::CategorySource::Auto);
	BOOST_CHECK_EQUAL(extender.m_categorySources[4], FeedInfo::CategorySource::NZBFile);
	BOOST_CHECK_EQUAL(extender.m_categorySources[5], FeedInfo::CategorySource::FeedFile);
	BOOST_CHECK_EQUAL(extender.m_categorySources[6], FeedInfo::CategorySource::Auto);
}

BOOST_AUTO_TEST_CASE(ParsePathsTest)
{
	Options::CmdOptList cmdOpts;

	cmdOpts.push_back("MainDir=/usr/etc/nzbget");
	cmdOpts.push_back("DestDir=/usr/etc/nzbget/dst");
	cmdOpts.push_back("InterDir=/usr/etc/nzbget/inter");
	cmdOpts.push_back("TempDir=/usr/etc/nzbget/tmp");
	cmdOpts.push_back("WebDir=/usr/etc/nzbget/webui");
	cmdOpts.push_back("NzbDir=/usr/etc/nzbget/nzb");
	cmdOpts.push_back("QueueDir=/usr/etc/nzbget/queue");
	cmdOpts.push_back("LockFile=/usr/etc/nzbget/nzbget.lock");
	cmdOpts.push_back("LogFile=/usr/etc/nzbget/nzbget.log");
	cmdOpts.push_back("ConfigTemplate=/usr/etc/nzbget/nzbget.template.conf");
	cmdOpts.push_back("CertStore=/usr/etc/nzbget/cacert.pem");
	cmdOpts.push_back("SecureCert=/usr/etc/nzbget/cert.pem");
	cmdOpts.push_back("SecureKey=/usr/etc/nzbget/key.pem");
	cmdOpts.push_back("UnpackPassFile=/usr/etc/nzbget/unpackpass");
	cmdOpts.push_back("UnrarCmd=/usr/bin/unrar");
	cmdOpts.push_back("SevenZipCmd=/usr/bin/7z");
	cmdOpts.push_back("ScriptDir=/usr/etc/nzbget/scripts;/usr/etc/nzbget/scripts2;");

	OptionsExtenderMock extender;
	Options options(&cmdOpts, &extender);

	BOOST_CHECK_EQUAL(options.GetMainDir(), "/usr/etc/nzbget");
	BOOST_CHECK_EQUAL(options.GetDestDir(), "/usr/etc/nzbget/dst");
	BOOST_CHECK_EQUAL(options.GetInterDir(), "/usr/etc/nzbget/inter");
	BOOST_CHECK_EQUAL(options.GetTempDir(), "/usr/etc/nzbget/tmp");
	BOOST_CHECK_EQUAL(options.GetWebDir(), "/usr/etc/nzbget/webui");
	BOOST_CHECK_EQUAL(options.GetNzbDir(), "/usr/etc/nzbget/nzb");
	BOOST_CHECK_EQUAL(options.GetQueueDir(), "/usr/etc/nzbget/queue");
	BOOST_CHECK_EQUAL(options.GetLockFile(), "/usr/etc/nzbget/nzbget.lock");
	BOOST_CHECK_EQUAL(options.GetLogFile(), "/usr/etc/nzbget/nzbget.log");
	BOOST_CHECK_EQUAL(options.GetConfigTemplate(), "/usr/etc/nzbget/nzbget.template.conf");
	BOOST_CHECK_EQUAL(options.GetCertStore(), "/usr/etc/nzbget/cacert.pem");
	BOOST_CHECK_EQUAL(options.GetSecureCert(), "/usr/etc/nzbget/cert.pem");
	BOOST_CHECK_EQUAL(options.GetSecureKey(), "/usr/etc/nzbget/key.pem");
	BOOST_CHECK_EQUAL(options.GetUnpackPassFilePath(), "/usr/etc/nzbget/unpackpass");
	BOOST_CHECK_EQUAL(options.GetUnrarPath(), "/usr/bin/unrar");
	BOOST_CHECK_EQUAL(options.GetSevenZipPath(), "/usr/bin/7z");

	const std::vector<boost::filesystem::path>& scriptPaths = options.GetScriptDirPaths();

	BOOST_REQUIRE_EQUAL(scriptPaths.size(), 2);
	BOOST_CHECK_EQUAL(scriptPaths[0], "/usr/etc/nzbget/scripts");
	BOOST_CHECK_EQUAL(scriptPaths[1], "/usr/etc/nzbget/scripts2");
}
