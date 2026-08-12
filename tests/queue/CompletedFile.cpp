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
#include "DownloadInfo.h"

BOOST_AUTO_TEST_SUITE(QueueTest)

BOOST_AUTO_TEST_CASE(CompletedFileSameFilenameTest)
{
	CompletedFile record(1, "DISORDER_2025-FLT/2c0837e5fa42c8cfb5d5e583168a2af4.mkv",
		"", CompletedFile::cfSuccess, 0, false, "", "");

	BOOST_CHECK(record.SameFilename("2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));
	BOOST_CHECK(record.SameFilename("DISORDER_2025-FLT/2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));
	BOOST_CHECK(record.SameFilename("DISORDER_2025-FLT\\2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));
	BOOST_CHECK(record.SameFilename("2C0837E5FA42C8CFB5D5E583168A2AF4.MKV"));
	BOOST_CHECK(!record.SameFilename("other.mkv"));
	BOOST_CHECK(!record.SameFilename("OTHER_DIR/2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));

	CompletedFile bareRecord(2, "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv",
		"", CompletedFile::cfSuccess, 0, false, "", "");
	BOOST_CHECK(bareRecord.SameFilename("5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"));
	BOOST_CHECK(bareRecord.SameFilename("Some.Dir/5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"));
	BOOST_CHECK(!bareRecord.SameFilename("5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mp4"));
}

BOOST_AUTO_TEST_CASE(RenameCompletedFileMatchesPathQualifiedRecordTest)
{
	NzbInfo nzbInfo;
	nzbInfo.GetCompletedFiles()->emplace_back(
		1, "parent/2c0837e5fa42c8cfb5d5e583168a2af4.mkv", "",
		CompletedFile::cfSuccess, 0, false, "", "");

	bool updated = nzbInfo.RenameCompletedFile(
		"2c0837e5fa42c8cfb5d5e583168a2af4.mkv", "Some.Release.mkv");

	BOOST_CHECK(updated);
	BOOST_CHECK_EQUAL(std::string(nzbInfo.GetCompletedFiles()->at(0).GetFilename()),
		"parent/Some.Release.mkv");
	BOOST_CHECK_EQUAL(std::string(nzbInfo.GetCompletedFiles()->at(0).GetOrigname()),
		"parent/2c0837e5fa42c8cfb5d5e583168a2af4.mkv");
}

BOOST_AUTO_TEST_SUITE_END()
