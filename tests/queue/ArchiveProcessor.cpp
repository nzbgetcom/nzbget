/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Denis <denis@nzbget.com>
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
#include <fstream>
#include <sstream>
#include "ArchiveProcessor.h"
#include "FileSystem.h"
#include "Options.h"
#include "Util.h"

BOOST_AUTO_TEST_SUITE(QueueTest)

void RunArchiveTest(const fs::path& archivePath, const fs::path& destDir)
{
	fs::error_code ec;

	fs::path processedDir = destDir.parent_path() / "_processed";
	fs::path brokenDir = destDir.parent_path() / "_broken";

	Incoming::Config config = {
		destDir.parent_path() / "unpack",
		processedDir,
		brokenDir,
		Options::ENzbDirArchiveAction::Delete
	};

	Incoming::ArchiveProcessor processor(config);
	auto result = processor.Process(archivePath, destDir);

	BOOST_REQUIRE(result.has_value());
	BOOST_REQUIRE_EQUAL(result->size(), 1);

	fs::path expectedPath = destDir / "test.nzb";
	BOOST_CHECK_EQUAL(result->at(0), expectedPath);
	BOOST_CHECK(fs::exists(expectedPath, ec));

	fs::remove_all(destDir.parent_path(), ec);
}

BOOST_AUTO_TEST_CASE(ArchiveCategoryPreservationTest)
{
	if (!Util::ResolvePathFromEnv("7z"))
	{
		BOOST_TEST_MESSAGE("This test requires a working '7z' executable.");
		return;
	}

	fs::error_code ec;

	fs::path baseDir = fs::temp_directory_path(ec) / fs::make_unique_filename();
	BOOST_REQUIRE_MESSAGE(!ec, "Failed to create temp directory");

	fs::path categoryDir = baseDir / "nzbdir" / "TV";
	fs::create_directories(categoryDir, ec);
	BOOST_REQUIRE_MESSAGE(!ec, "Failed to create subdirectories");

	fs::path nzbSource = baseDir / "test.nzb";
	{
		std::ofstream ofs(fs::u8string(nzbSource));
		BOOST_REQUIRE(ofs);
		ofs << "<?xml version=\"1.0\"?>\n"
		       "<nzb xmlns=\"http://www.newzbin.com/DTD/2003/nzb\">\n"
		       "</nzb>\n";
	}

	fs::path archivePath = categoryDir / "test.nzb.gz";
	{
		std::ostringstream cmd;
		cmd << "cd \"" << fs::u8string(baseDir) << "\" && "
		    << "7z a -tgzip \"" << fs::u8string(archivePath) << "\" \"test.nzb\" >/dev/null";
		int ret = std::system(cmd.str().c_str());
		BOOST_REQUIRE_MESSAGE(ret == 0, "Failed to create archive with 7z");
	}
	BOOST_REQUIRE(fs::exists(archivePath, ec));

	RunArchiveTest(archivePath, categoryDir);
}

BOOST_AUTO_TEST_CASE(ArchiveSubdirEscapeTest)
{
	if (!Util::ResolvePathFromEnv("7z"))
	{
		BOOST_TEST_MESSAGE("This test requires a working '7z' executable.");
		return;
	}

	fs::error_code ec;

	fs::path baseDir = fs::temp_directory_path(ec) / fs::make_unique_filename();
	BOOST_REQUIRE_MESSAGE(!ec, "Failed to create temp directory");

	fs::path nzbDir = baseDir / "nzbdir";
	fs::path outsideDir = baseDir / "tmp";

	fs::create_directories(nzbDir, ec);
	fs::create_directories(outsideDir, ec);
	BOOST_REQUIRE_MESSAGE(!ec, "Failed to create subdirectories");

	fs::path nzbSource = baseDir / "test.nzb";
	{
		std::ofstream ofs(fs::u8string(nzbSource));
		BOOST_REQUIRE(ofs);
		ofs << "<?xml version=\"1.0\"?>\n"
		       "<nzb xmlns=\"http://www.newzbin.com/DTD/2003/nzb\">\n"
		       "</nzb>\n";
	}

	fs::path archivePath = outsideDir / "test.nzb.gz";
	{
		std::ostringstream cmd;
		cmd << "cd \"" << fs::u8string(baseDir) << "\" && "
		    << "7z a -tgzip \"" << fs::u8string(archivePath) << "\" \"test.nzb\" >/dev/null";
		int ret = std::system(cmd.str().c_str());
		BOOST_REQUIRE_MESSAGE(ret == 0, "Failed to create archive with 7z");
	}
	BOOST_REQUIRE(fs::exists(archivePath, ec));

	RunArchiveTest(archivePath, nzbDir);
}

BOOST_AUTO_TEST_SUITE_END()
