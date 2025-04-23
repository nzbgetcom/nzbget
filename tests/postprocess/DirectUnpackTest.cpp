/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2017-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include "FileSystem.h"
#include "DirectUnpack.h"
#include "Log.h"
#include "Options.h"
#include "DiskState.h"

const std::string CURR_DIR = FileSystem::GetCurrentDirectory().Str();
const std::string TEST_DATA_DIR = CURR_DIR + "/rarrenamer";
const std::string WORKING_DIR = TEST_DATA_DIR + "/empty";
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

	BOOST_REQUIRE(FileSystem::CreateDirectory(WORKING_DIR.c_str()));

	const std::string part01 = TEST_DATA_DIR + "/testfile3.part01.rar";
	const std::string part01Dest = WORKING_DIR + "/testfile3.part01.rar";
	BOOST_CHECK(
		FileSystem::CopyFile(part01.c_str(), part01Dest.c_str())
	);

	const std::string part02 = TEST_DATA_DIR + "/testfile3.part02.rar";
	const std::string part02Dest = WORKING_DIR + "/testfile3.part02.rar";
	BOOST_CHECK(
		FileSystem::CopyFile(part02.c_str(), part02Dest.c_str())
	);

	const std::string part03 = TEST_DATA_DIR + "/testfile3.part03.rar";
	const std::string part03Dest = WORKING_DIR + "/testfile3.part03.rar";
	BOOST_CHECK(
		FileSystem::CopyFile(part03.c_str(), part03Dest.c_str())
	);

	std::unique_ptr<NzbInfo> nzbInfo = std::make_unique<NzbInfo>();
	NzbInfo* nzbPtr = nzbInfo.get();
	nzbInfo->SetName("DirectUnpackSimpleTest");
	nzbInfo->SetDestDir(WORKING_DIR.c_str());
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

	CString errMsg;
	const std::string resultFile = WORKING_DIR + "/_unpack/testfile3.dat";
	BOOST_CHECK_EQUAL(nzbPtr->GetDirectUnpackStatus(), NzbInfo::nsSuccess);
	BOOST_CHECK(FileSystem::FileExists(resultFile.c_str()));
	BOOST_REQUIRE(FileSystem::DeleteDirectoryWithContent(WORKING_DIR.c_str(), errMsg));
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

	BOOST_REQUIRE(FileSystem::CreateDirectory(WORKING_DIR.c_str()));

	const std::string part01 = TEST_DATA_DIR + "/testfile3.part01.rar";
	const std::string part01Dest = WORKING_DIR + "/testfile3.part01.rar";
	BOOST_CHECK(
		FileSystem::CopyFile(part01.c_str(), part01Dest.c_str())
	);

	const std::string part02 = TEST_DATA_DIR + "/testfile3.part02.rar";
	const std::string part02Dest = WORKING_DIR + "/testfile3.part02.rar";
	BOOST_CHECK(
		FileSystem::CopyFile(part02.c_str(), part02Dest.c_str())
	);

	const std::string part03 = TEST_DATA_DIR + "/testfile3.part03.rar";
	const std::string part03Dest = WORKING_DIR + "/testfile3.part03.rar";
	BOOST_CHECK(
		FileSystem::CopyFile(part03.c_str(), part03Dest.c_str())
	);

	const std::string testfile5Part1 = TEST_DATA_DIR + "/testfile5.part01.rar";
	const std::string testfile5Part1Dest = WORKING_DIR + "/testfile5.part01.rar";
	BOOST_CHECK(
		FileSystem::CopyFile(testfile5Part1.c_str(), testfile5Part1Dest.c_str())
	);

	const std::string testfile5Part2 = TEST_DATA_DIR + "/testfile5.part02.rar";
	const std::string testfile5Part2Dest = WORKING_DIR + "/testfile5.part02.rar";
	BOOST_CHECK(
		FileSystem::CopyFile(testfile5Part2.c_str(), testfile5Part2Dest.c_str())
	);

	const std::string testfile5Part3 = TEST_DATA_DIR + "/testfile5.part03.rar";
	const std::string testfile5Part3Dest = WORKING_DIR + "/testfile5.part03.rar";
	BOOST_CHECK(
		FileSystem::CopyFile(testfile5Part3.c_str(), testfile5Part3Dest.c_str())
	);

	std::unique_ptr<NzbInfo> nzbInfo = std::make_unique<NzbInfo>();
	NzbInfo* nzbPtr = nzbInfo.get();
	nzbInfo->SetName("DirectUnpackTwoArchives");
	nzbInfo->SetDestDir(WORKING_DIR.c_str());
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

	CString errMsg;
	const std::string resultFile1 = WORKING_DIR + "/_unpack/testfile3.dat";
	const std::string resultFile2 = WORKING_DIR + "/_unpack/testfile5.dat";
	BOOST_CHECK_EQUAL(nzbPtr->GetDirectUnpackStatus(), NzbInfo::nsSuccess);
	BOOST_CHECK(FileSystem::FileExists(resultFile1.c_str()));
	BOOST_CHECK(FileSystem::FileExists(resultFile2.c_str()));
	BOOST_REQUIRE(FileSystem::DeleteDirectoryWithContent(WORKING_DIR.c_str(), errMsg));
}
