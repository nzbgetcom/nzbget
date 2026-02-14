/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2017-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2024-2026 Denis <denis@nzbget.com>
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

#include "DirectUnpack.h"
#include "Log.h"
#include "Options.h"
#include "DiskState.h"

BOOST_AUTO_TEST_SUITE(PostprocessTest)

namespace fs = boost::filesystem;

const fs::path CURR_DIR = fs::current_path();
const fs::path TEST_DATA_DIR = CURR_DIR / "rarrenamer";
const fs::path WORKING_DIR = TEST_DATA_DIR / "empty";
static const char* UNRAR_PATH = std::getenv("unrar");

class DirectUnpackDownloadQueueMock final : public DownloadQueue
{
public:
	DirectUnpackDownloadQueueMock() { Init(this); }
	bool EditEntry(int ID, EEditAction action, const char* args) { return false; };
	bool EditList(
		IdList* idList, 
		NameList* nameList, 
		EMatchMode matchMode,
		EEditAction action, 
		const char* args) { return false; }
	void HistoryChanged() {}
	void Save() {};
	void SaveChanged() {}
};

BOOST_AUTO_TEST_CASE(DirectUnpackSimpleTest)
{
	if (!UNRAR_PATH)
	{
		BOOST_TEST_MESSAGE("This test requires a working 'unrar' executable.");
		BOOST_TEST_MESSAGE("The 'unrar' command was not found in your system's PATH.");
		return;
	}

	const std::string unrarCmd = std::string("UnrarCmd=") + UNRAR_PATH;
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("WriteLog=none");
	cmdOpts.push_back("NzbLog=no");
	cmdOpts.push_back(unrarCmd.c_str());
	Options options(&cmdOpts, nullptr);

	DirectUnpackDownloadQueueMock downloadQueue;

	BOOST_REQUIRE(fs::create_directory(WORKING_DIR));

	const fs::path part01 = TEST_DATA_DIR / "testfile3.part01.rar";
	const fs::path part01Dest = WORKING_DIR / "testfile3.part01.rar";
	BOOST_CHECK(fs::copy_file(part01, part01Dest));

	const fs::path part02 = TEST_DATA_DIR / "testfile3.part02.rar";
	const fs::path part02Dest = WORKING_DIR / "testfile3.part02.rar";
	BOOST_CHECK(fs::copy_file(part02, part02Dest));

	const fs::path part03 = TEST_DATA_DIR / "testfile3.part03.rar";
	const fs::path part03Dest = WORKING_DIR / "testfile3.part03.rar";
	BOOST_CHECK(fs::copy_file(part03, part03Dest));

	std::unique_ptr<NzbInfo> nzbInfo = std::make_unique<NzbInfo>();
	NzbInfo* nzbPtr = nzbInfo.get();
	nzbInfo->SetName("DirectUnpackSimpleTest");
	nzbInfo->SetDestDir(WORKING_DIR.string().c_str());
	downloadQueue.GetQueue()->Add(std::move(nzbInfo), false);

	DirectUnpack::StartJob(nzbPtr);

	while (true)
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		if (nzbPtr->GetUnpackThread())
		{
			DirectUnpack* directUnpack = static_cast<DirectUnpack*>(nzbPtr->GetUnpackThread());
			directUnpack->NzbDownloaded(downloadQueue, nzbPtr);
			break;
		}
		Util::Sleep(50);
	}

	while (nzbPtr->GetDirectUnpackStatus() == NzbInfo::nsRunning)
	{
		Util::Sleep(20);
	}

	const fs::path resultFile = WORKING_DIR / "_unpack/testfile3.dat";
	BOOST_CHECK_EQUAL(nzbPtr->GetDirectUnpackStatus(), NzbInfo::nsSuccess);
	BOOST_CHECK(fs::exists(resultFile));
	BOOST_REQUIRE(fs::remove_all(WORKING_DIR));
}

BOOST_AUTO_TEST_CASE(DirectUnpackTwoArchives)
{
	if (!UNRAR_PATH)
	{
		BOOST_TEST_MESSAGE("This test requires a working 'unrar' executable.");
		BOOST_TEST_MESSAGE("The 'unrar' command was not found in your system's PATH.");
		return;
	}

	const std::string unrarCmd = std::string("UnrarCmd=") + UNRAR_PATH;
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("WriteLog=none");
	cmdOpts.push_back("NzbLog=no");
	cmdOpts.push_back(unrarCmd.c_str());
	Options options(&cmdOpts, nullptr);

	DirectUnpackDownloadQueueMock downloadQueue;

	BOOST_REQUIRE(FileSystem::CreateDirectory(WORKING_DIR.string().c_str()));

	const fs::path part01 = TEST_DATA_DIR / "testfile3.part01.rar";
	const fs::path part01Dest = WORKING_DIR / "testfile3.part01.rar";
	BOOST_CHECK(fs::copy_file(part01, part01Dest));

	const fs::path part02 = TEST_DATA_DIR / "testfile3.part02.rar";
	const fs::path part02Dest = WORKING_DIR / "testfile3.part02.rar";
	BOOST_CHECK(fs::copy_file(part02, part02Dest));

	const fs::path part03 = TEST_DATA_DIR / "testfile3.part03.rar";
	const fs::path part03Dest = WORKING_DIR / "testfile3.part03.rar";
	BOOST_CHECK(fs::copy_file(part03, part03Dest));

	const fs::path testfile5Part1 = TEST_DATA_DIR / "testfile5.part01.rar";
	const fs::path testfile5Part1Dest = WORKING_DIR / "testfile5.part01.rar";
	BOOST_CHECK(fs::copy_file(testfile5Part1, testfile5Part1Dest));

	const fs::path testfile5Part2 = TEST_DATA_DIR / "testfile5.part02.rar";
	const fs::path testfile5Part2Dest = WORKING_DIR / "testfile5.part02.rar";
	BOOST_CHECK(fs::copy_file(testfile5Part2, testfile5Part2Dest));

	const fs::path testfile5Part3 = TEST_DATA_DIR / "testfile5.part03.rar";
	const fs::path testfile5Part3Dest = WORKING_DIR / "testfile5.part03.rar";
	BOOST_CHECK(fs::copy_file(testfile5Part3, testfile5Part3Dest));

	std::unique_ptr<NzbInfo> nzbInfo = std::make_unique<NzbInfo>();
	NzbInfo* nzbPtr = nzbInfo.get();
	nzbInfo->SetName("DirectUnpackTwoArchives");
	nzbInfo->SetDestDir(WORKING_DIR.string().c_str());
	downloadQueue.GetQueue()->Add(std::move(nzbInfo), false);

	DirectUnpack::StartJob(nzbPtr);

	while (true)
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		if (nzbPtr->GetUnpackThread())
		{
			DirectUnpack* directUnpack = static_cast<DirectUnpack*>(nzbPtr->GetUnpackThread());
			directUnpack->NzbDownloaded(downloadQueue, nzbPtr);
			break;
		}
		Util::Sleep(50);
	}

	while (nzbPtr->GetDirectUnpackStatus() == NzbInfo::nsRunning)
	{
		Util::Sleep(20);
	}

	const fs::path resultFile1 = WORKING_DIR / "_unpack/testfile3.dat";
	const fs::path resultFile2 = WORKING_DIR / "_unpack/testfile5.dat";
	BOOST_CHECK_EQUAL(nzbPtr->GetDirectUnpackStatus(), NzbInfo::nsSuccess);
	BOOST_CHECK(fs::exists(resultFile1));
	BOOST_CHECK(fs::exists(resultFile2));
	BOOST_REQUIRE(fs::remove_all(WORKING_DIR));
}

BOOST_AUTO_TEST_SUITE_END()
