/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2024-2025 Denis <denis@nzbget.com>
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
#include "DupeMatcher.h"

namespace fs = boost::filesystem;

BOOST_AUTO_TEST_CASE(DupeMatcherTest)
{
	const fs::path testDataDir = fs::current_path() / "rarrenamer";
	const fs::path workingDir = testDataDir / "DupeMatcher";
	BOOST_REQUIRE(fs::create_directory(workingDir));

	const fs::path dupe1 = workingDir / "dupe1";
	BOOST_CHECK(fs::create_directories(dupe1));
	fs::copy_file(testDataDir / "parchecker", dupe1);

	const fs::path dupe2 = workingDir / "dupe2";
	BOOST_CHECK(fs::create_directories(dupe2));
	fs::copy_file(testDataDir / "parchecker", dupe2);
	fs::remove(dupe2 / "testfile.nfo");

	const fs::path rardupe1 = testDataDir / "/dupematcher1";
	const fs::path rardupe2 = testDataDir / "/dupematcher2";

	const fs::path nondupe = workingDir / "nondupe";
	BOOST_CHECK(fs::create_directories(nondupe));
	fs::copy_file(testDataDir / "parchecker", nondupe);
	fs::remove(nondupe / "testfile.dat");


	int64 expectedSize = fs::file_size(dupe1 / "/testfile.dat");

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
