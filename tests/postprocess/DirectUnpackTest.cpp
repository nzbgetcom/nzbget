/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2017-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include "FileSystem.h"
#include "DirectUnpack.h"
#include "Log.h"
#include "Options.h"
#include "DiskState.h"

class DirectUnpackDownloadQueueMock : public DownloadQueue
{
public:
	DirectUnpackDownloadQueueMock() { Init(this); }
	virtual bool EditEntry(int ID, EEditAction action, const char* args) { return false; };
	virtual bool EditList(IdList* idList, NameList* nameList, EMatchMode matchMode,
		EEditAction action, const char* args) { return false; }
	virtual void HistoryChanged() {}
	virtual void Save() {};
	virtual void SaveChanged() {}
};

BOOST_AUTO_TEST_CASE(DirectUnpackSimpleTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("WriteLog=none");
	cmdOpts.push_back("NzbLog=no");
	Options options(&cmdOpts, nullptr);

	DirectUnpackDownloadQueueMock downloadQueue;

	BOOST_TEST_MESSAGE("This test requires working unrar 5 in search path");

	std::string currDir = FileSystem::GetCurrentDirectory().Str();
	std::string testDataDir = currDir + "/rarrenamer";
	std::string workingDir = testDataDir + "/empty";
	
	BOOST_REQUIRE(FileSystem::CreateDirectory(workingDir.c_str()));

	BOOST_REQUIRE(
		FileSystem::CopyFile(
			(testDataDir + "/testfile3.part01.rar").c_str(),
			(workingDir + "/testfile3.part01.rar").c_str()
		)
	);

	BOOST_REQUIRE(
		FileSystem::CopyFile(
			(testDataDir + "/testfile3.part02.rar").c_str(),
			(workingDir + "/testfile3.part02.rar").c_str()
		)
	);
	BOOST_REQUIRE(
		FileSystem::CopyFile(
			(testDataDir + "/testfile3.part03.rar").c_str(),
			(workingDir + "/testfile3.part03.rar").c_str()
		)
	);

	std::unique_ptr<NzbInfo> nzbInfo = std::make_unique<NzbInfo>();
	NzbInfo* nzbPtr = nzbInfo.get();
	nzbInfo->SetName("test");
	nzbInfo->SetDestDir(workingDir.c_str());
	downloadQueue.GetQueue()->Add(std::move(nzbInfo), false);
	
	DirectUnpack::StartJob(nzbPtr);

	while (true)
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		if (nzbPtr->GetUnpackThread())
		{
			((DirectUnpack*)nzbPtr->GetUnpackThread())->NzbDownloaded(downloadQueue, nzbPtr);
			break;
		}
		Util::Sleep(50);
	}

	while (nzbPtr->GetDirectUnpackStatus() == NzbInfo::nsRunning)
	{
		Util::Sleep(20);
	}

	BOOST_CHECK(nzbPtr->GetDirectUnpackStatus() == NzbInfo::nsSuccess);
	BOOST_CHECK(FileSystem::FileExists((workingDir + "/_unpack/testfile3.dat").c_str()));
	BOOST_REQUIRE(FileSystem::RemoveDirectory(workingDir.c_str()));
}

BOOST_AUTO_TEST_CASE(DirectUnpackTwoArchives)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("WriteLog=none");
	cmdOpts.push_back("NzbLog=no");
	Options options(&cmdOpts, nullptr);

	DirectUnpackDownloadQueueMock downloadQueue;

	BOOST_TEST_MESSAGE("This test requires working unrar 5 in search path");

	std::string currDir = FileSystem::GetCurrentDirectory().Str();
	std::string testDataDir = currDir + "/rarrenamer";
	std::string workingDir = testDataDir + "/empty";

	BOOST_REQUIRE(FileSystem::CreateDirectory(workingDir.c_str()));

	FileSystem::CopyFile(
		(testDataDir + "/testfile3.part01.rar").c_str(),
		(workingDir + "/testfile3.part01.rar").c_str()
	);
	FileSystem::CopyFile(
		(testDataDir + "/testfile3.part02.rar").c_str(),
		(workingDir + "/testfile3.part02.rar").c_str()
	);
	FileSystem::CopyFile(
		(testDataDir + "/testfile3.part03.rar").c_str(),
		(workingDir + "/testfile3.part03.rar").c_str()
	);

	FileSystem::CopyFile(
		(testDataDir + "/testfile5.part01.rar").c_str(),
		(workingDir + "/testfile5.part01.rar").c_str()
	);
	FileSystem::CopyFile(
		(testDataDir + "/testfile5.part02.rar").c_str(),
		(workingDir + "/testfile5.part02.rar").c_str()
	);
	FileSystem::CopyFile(
		(testDataDir + "/testfile5.part03.rar").c_str(),
		(workingDir + "/testfile5.part03.rar").c_str()
	);

	std::unique_ptr<NzbInfo> nzbInfo = std::make_unique<NzbInfo>();
	NzbInfo* nzbPtr = nzbInfo.get();
	nzbInfo->SetName("test");
	nzbInfo->SetDestDir(workingDir.c_str());
	downloadQueue.GetQueue()->Add(std::move(nzbInfo), false);

	DirectUnpack::StartJob(nzbPtr);

	while (true)
	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		if (nzbPtr->GetUnpackThread())
		{
			((DirectUnpack*)nzbPtr->GetUnpackThread())->NzbDownloaded(downloadQueue, nzbPtr);
			break;
		}
		Util::Sleep(50);
	}

	while (nzbPtr->GetDirectUnpackStatus() == NzbInfo::nsRunning)
	{
		Util::Sleep(20);
	}

	BOOST_CHECK(nzbPtr->GetDirectUnpackStatus() == NzbInfo::nsSuccess);
	BOOST_CHECK(FileSystem::FileExists((workingDir + "/_unpack/testfile3.dat").c_str()));
	BOOST_CHECK(FileSystem::FileExists((workingDir + "/_unpack/testfile5.dat").c_str()));
	BOOST_REQUIRE(FileSystem::RemoveDirectory(workingDir.c_str()));
}
