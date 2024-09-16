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

#include <boost/test/unit_test.hpp>
#include <filesystem>

#include "DupeMatcher.h"

BOOST_AUTO_TEST_CASE(DupeMatcherTest)
{
	const std::string testDataDir = std::filesystem::current_path().string() + "/rarrenamer";
	const std::string workingDir = testDataDir + "/DupeMatcher";
	std::filesystem::create_directory(workingDir);

	CString errmsg;

	// prepare directories

	std::string dupe1(workingDir + "/dupe1");
	BOOST_CHECK(FileSystem::ForceDirectories(dupe1.c_str(), errmsg));
	std::filesystem::copy(dupe1, testDataDir + "/parchecker");

	std::string dupe2(workingDir + "/dupe2");
	BOOST_CHECK(FileSystem::ForceDirectories(dupe2.c_str(), errmsg));
	std::filesystem::copy(dupe2, testDataDir + "/parchecker");
	FileSystem::DeleteFile((dupe2 + "/testfile.nfo").c_str());

	std::string rardupe1(testDataDir + "/dupematcher1");
	std::string rardupe2(testDataDir + "/dupematcher2");

	std::string nondupe(workingDir + "/nondupe");
	BOOST_CHECK(FileSystem::ForceDirectories(nondupe.c_str(), errmsg));
	std::filesystem::copy(nondupe, testDataDir + "/parchecker");
	remove((nondupe + "/testfile.dat").c_str());

	// now test
	int64 expectedSize = FileSystem::FileSize((dupe1 + "/testfile.dat").c_str());

	BOOST_TEST_MESSAGE("This test requires working unrar 5 in search path");

	DupeMatcher dupe1Matcher(dupe1.c_str(), expectedSize);
	BOOST_CHECK(dupe1Matcher.Prepare());
	BOOST_CHECK(dupe1Matcher.MatchDupeContent(dupe2.c_str()));
	BOOST_CHECK(dupe1Matcher.MatchDupeContent(rardupe1.c_str()));
	BOOST_CHECK(dupe1Matcher.MatchDupeContent(rardupe2.c_str()));
	BOOST_CHECK(dupe1Matcher.MatchDupeContent(nondupe.c_str()) == false);
	
	DupeMatcher dupe2Matcher(dupe2.c_str(), expectedSize);
	BOOST_CHECK(dupe2Matcher.Prepare());
	BOOST_CHECK(dupe2Matcher.MatchDupeContent(dupe1.c_str()));
	BOOST_CHECK(dupe2Matcher.MatchDupeContent(rardupe1.c_str()));
	BOOST_CHECK(dupe2Matcher.MatchDupeContent(rardupe2.c_str()));
	BOOST_CHECK(dupe2Matcher.MatchDupeContent(nondupe.c_str()) == false);

	DupeMatcher nonDupeMatcher(nondupe.c_str(), expectedSize);
	BOOST_CHECK(nonDupeMatcher.Prepare() == false);
	BOOST_CHECK(nonDupeMatcher.MatchDupeContent(dupe1.c_str()) == false);
	BOOST_CHECK(nonDupeMatcher.MatchDupeContent(dupe2.c_str()) == false);
	BOOST_CHECK(nonDupeMatcher.MatchDupeContent(rardupe1.c_str()) == false);
	BOOST_CHECK(nonDupeMatcher.MatchDupeContent(rardupe2.c_str()) == false);

	DupeMatcher rardupe1matcher(rardupe1.c_str(), expectedSize);
	BOOST_CHECK(rardupe1matcher.Prepare());
	BOOST_CHECK(rardupe1matcher.MatchDupeContent(dupe1.c_str()));
	BOOST_CHECK(rardupe1matcher.MatchDupeContent(dupe2.c_str()));		    
	BOOST_CHECK(rardupe1matcher.MatchDupeContent(rardupe2.c_str()));
	BOOST_CHECK(rardupe1matcher.MatchDupeContent(nondupe.c_str()) == false);

	DupeMatcher rardupe2matcher(rardupe2.c_str(), expectedSize);
	BOOST_CHECK(rardupe2matcher.Prepare());
	BOOST_CHECK(rardupe2matcher.MatchDupeContent(rardupe1.c_str()));
	BOOST_CHECK(rardupe2matcher.MatchDupeContent(dupe1.c_str()));
	BOOST_CHECK(rardupe2matcher.MatchDupeContent(dupe2.c_str()));
	BOOST_CHECK(rardupe2matcher.MatchDupeContent(nondupe.c_str()) == false);
}
