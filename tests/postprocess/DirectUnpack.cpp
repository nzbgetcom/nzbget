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
#include "FileSystem.h"
#include "PrePostProcessor.h"
#include "QueueScript.h"
#include "WorkState.h"
#include "HistoryCoordinator.h"
#include "DupeCoordinator.h"
#include "Util.h"
#include <chrono>

BOOST_AUTO_TEST_SUITE(PostprocessTest)

const fs::path CURR_DIR = fs::current_path();
const fs::path TEST_DATA_DIR = CURR_DIR / "rarrenamer";
const fs::path WORKING_DIR = TEST_DATA_DIR / "empty";
static const auto UNRAR_PATH = Util::ResolvePathFromEnv("unrar");

class DirectUnpackDownloadQueueMock final : public DownloadQueue
{
public:
	DirectUnpackDownloadQueueMock()
	{
		Init(this);
		Loaded();
	}
	~DirectUnpackDownloadQueueMock()
	{
		Final();
	}
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

bool WaitFor(const std::function<bool()>& condition, int timeoutMs = 5000)
{
	for (int elapsed = 0; elapsed < timeoutMs; elapsed += 20)
	{
		if (condition())
		{
			return true;
		}
		Util::Sleep(20);
	}
	return condition();
}

template<typename Func>
bool WithNzbInfo(int id, Func&& func)
{
	GuardedDownloadQueue queueGuard = DownloadQueue::Guard();
	for (const auto& nzb : *queueGuard->GetQueue())
	{
		NzbInfo* nzbInfo = nzb.get();
		if (nzbInfo->GetId() == id)
		{
			return func(nzbInfo);
		}
	}
	for (const auto& history : *queueGuard->GetHistory())
	{
		HistoryInfo* historyInfo = history.get();
		if (historyInfo->GetNzbInfo() && historyInfo->GetNzbInfo()->GetId() == id)
		{
			return func(historyInfo->GetNzbInfo());
		}
	}
	return false;
}

class ScopedTempDir final
{
public:
	ScopedTempDir()
	{
		const fs::path base = fs::temp_directory_path();
		const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
		m_path = base / ("nzbget-directunpack-" + std::to_string(nonce));
		BOOST_REQUIRE(base.is_absolute());
		BOOST_REQUIRE(fs::create_directory(m_path));
	}

	~ScopedTempDir()
	{
		std::error_code error;
		fs::remove_all(m_path, error);
	}

	const fs::path& Get() const { return m_path; }

private:
	fs::path m_path;
};

class ScopedPrePostProcessor final
{
public:
	ScopedPrePostProcessor()
	{
		m_processor.Start();
	}

	~ScopedPrePostProcessor()
	{
		m_processor.Stop();
		const bool stopped = WaitFor([this] { return !m_processor.IsRunning(); }, 12000);
		BOOST_CHECK_MESSAGE(stopped,
			"PrePostProcessor did not stop within the bounded wait");
		while (!stopped && m_processor.IsRunning())
		{
			Util::Sleep(20);
		}
	}

	PrePostProcessor* Get() { return &m_processor; }

private:
	PrePostProcessor m_processor;
};

class ScopedQueueScriptCoordinator final
{
public:
	ScopedQueueScriptCoordinator() : m_previous(g_QueueScriptCoordinator)
	{
		g_QueueScriptCoordinator = &m_coordinator;
	}

	~ScopedQueueScriptCoordinator()
	{
		g_QueueScriptCoordinator = m_previous;
	}

private:
	QueueScriptCoordinator* m_previous;
	QueueScriptCoordinator m_coordinator;
};

class ScopedPostProcessPause final
{
public:
	explicit ScopedPostProcessPause(bool pausePostProcess = false) : m_tempPause(g_WorkState->GetTempPausePostprocess())
	{
		g_WorkState->SetTempPausePostprocess(pausePostProcess);
	}

	~ScopedPostProcessPause()
	{
		g_WorkState->SetTempPausePostprocess(m_tempPause);
	}

	void Set(bool pausePostProcess)
	{
		g_WorkState->SetTempPausePostprocess(pausePostProcess);
	}

private:
	bool m_tempPause;
};

class ScopedPostProcessCoordinators final
{
public:
	ScopedPostProcessCoordinators() : m_historyPrevious(g_HistoryCoordinator), m_dupePrevious(g_DupeCoordinator)
	{
		g_HistoryCoordinator = &m_history;
		g_DupeCoordinator = &m_dupe;
	}

	~ScopedPostProcessCoordinators()
	{
		g_HistoryCoordinator = m_historyPrevious;
		g_DupeCoordinator = m_dupePrevious;
	}

private:
	HistoryCoordinator* m_historyPrevious;
	DupeCoordinator* m_dupePrevious;
	HistoryCoordinator m_history;
	DupeCoordinator m_dupe;
};

class RestoreOptionsGlobal final
{
public:
	RestoreOptionsGlobal() : m_previous(g_Options) {}
	~RestoreOptionsGlobal() { g_Options = m_previous; }

private:
	Options* m_previous;
};

// Shared by the PAR-failure/DirectUnpack tests below; unrarCmdOpt is only needed
// when a test actually runs a real unpack (pass nullptr to skip it).
Options::CmdOptList MakeParFailureTestOpts(const char* unrarCmdOpt)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("WriteLog=none");
	cmdOpts.push_back("NzbLog=no");
	if (unrarCmdOpt)
	{
		cmdOpts.push_back(unrarCmdOpt);
	}
	cmdOpts.push_back("ParRename=no");
	cmdOpts.push_back("RarRename=no");
	cmdOpts.push_back("RenameAfterUnpack=no");
	cmdOpts.push_back("DupeCheck=no");
	cmdOpts.push_back("KeepHistory=1");
	return cmdOpts;
}

int QueueParFailureTestNzb(DirectUnpackDownloadQueueMock& downloadQueue, const char* name, const fs::path& destDir,
	NzbInfo::EParStatus parStatus = NzbInfo::psNone, NzbInfo::EDirectUnpackStatus directStatus = NzbInfo::nsNone)
{
	auto nzbInfo = std::make_unique<NzbInfo>();
	NzbInfo* nzbPtr = nzbInfo.get();
	nzbInfo->SetName(name);
	nzbInfo->SetDestDir(destDir.string().c_str());
	nzbInfo->SetParStatus(parStatus);
	nzbInfo->SetDirectUnpackStatus(directStatus);
	downloadQueue.GetQueue()->Add(std::move(nzbInfo), false);
	return nzbPtr->GetId();
}

BOOST_AUTO_TEST_CASE(DirectUnpackSimpleTest)
{
	if (!UNRAR_PATH)
	{
		BOOST_TEST_MESSAGE("unrar not available - skipping test");
		BOOST_CHECK(true);
		return;
	}

	const std::string unrarCmd = std::string("UnrarCmd=") + fs::u8string(UNRAR_PATH.value());
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("WriteLog=none");
	cmdOpts.push_back("NzbLog=no");
	cmdOpts.push_back(unrarCmd.c_str());
	Options options(&cmdOpts, nullptr);

	DirectUnpackDownloadQueueMock downloadQueue;

	fs::remove_all(WORKING_DIR);
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
		BOOST_TEST_MESSAGE("unrar not available - skipping test");
		BOOST_CHECK(true);
		return;
	}

	const std::string unrarCmd = std::string("UnrarCmd=") + UNRAR_PATH->string();
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("WriteLog=none");
	cmdOpts.push_back("NzbLog=no");
	cmdOpts.push_back(unrarCmd.c_str());
	Options options(&cmdOpts, nullptr);

	DirectUnpackDownloadQueueMock downloadQueue;

	fs::remove_all(WORKING_DIR);
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

BOOST_AUTO_TEST_CASE(DirectUnpackParFailureStillFinalizesUnpack)
{
	if (!UNRAR_PATH)
	{
		BOOST_TEST_MESSAGE("unrar not available - skipping test");
		BOOST_CHECK(true);
		return;
	}

	const std::string unrarCmd = std::string("UnrarCmd=") + UNRAR_PATH->string();
	Options::CmdOptList cmdOpts = MakeParFailureTestOpts(unrarCmd.c_str());
	RestoreOptionsGlobal restoreOptions;
	Options options(&cmdOpts, nullptr);
	DirectUnpackDownloadQueueMock downloadQueue;
	ScopedQueueScriptCoordinator queueScripts;
	ScopedPostProcessPause postProcessPause(true);
	ScopedPostProcessCoordinators postProcessCoordinators;
	ScopedTempDir workingDir;
	const fs::path& workPath = workingDir.Get();
	for (const char* part : {"testfile3.part01.rar", "testfile3.part02.rar", "testfile3.part03.rar"})
	{
		BOOST_REQUIRE(fs::copy_file(TEST_DATA_DIR / part, workPath / part));
	}

	const int nzbId = QueueParFailureTestNzb(downloadQueue, "DirectUnpackParFailureStillFinalizesUnpack", workPath);

	BOOST_REQUIRE(WithNzbInfo(nzbId, [](NzbInfo* info)
	{
		DirectUnpack::StartJob(info);
		return true;
	}));
	BOOST_REQUIRE(WaitFor([&]
	{
		return WithNzbInfo(nzbId, [](NzbInfo* info) { return info->GetUnpackThread() != nullptr; });
	}));
	BOOST_REQUIRE(WithNzbInfo(nzbId, [](NzbInfo* info)
	{
		info->SetParStatus(NzbInfo::psFailure);
		return true;
	}));
	{
		ScopedPrePostProcessor processor;
		BOOST_REQUIRE(WithNzbInfo(nzbId, [&](NzbInfo* info)
		{
			processor.Get()->NzbDownloaded(&downloadQueue, info);
			return true;
		}));

		BOOST_REQUIRE(WaitFor([&]
		{
			return WithNzbInfo(nzbId, [](NzbInfo* info)
			{
				return info->GetDirectUnpackStatus() == NzbInfo::nsSuccess &&
					info->GetUnpackThread() == nullptr && info->GetPostInfo() &&
					!info->GetPostInfo()->GetExtractedArchives()->empty();
			});
		}));
		BOOST_REQUIRE(fs::exists(workPath / "_unpack/testfile3.dat"));
		postProcessPause.Set(false);
		BOOST_REQUIRE(WaitFor([&]
		{
			return WithNzbInfo(nzbId, [](NzbInfo* info) { return info->GetUnpackStatus() != NzbInfo::usNone; });
		}));
		{
			BOOST_CHECK(WithNzbInfo(nzbId, [](NzbInfo* info)
			{
				return info->GetUnpackStatus() == NzbInfo::usSuccess &&
					info->GetParStatus() == NzbInfo::psFailure &&
					!strcmp(info->MakeTextStatus(false), "FAILURE/PAR");
			}));
		}
		BOOST_CHECK(fs::exists(workPath / "testfile3.dat"));
		BOOST_CHECK(!fs::exists(workPath / "_unpack"));
	}
}

BOOST_AUTO_TEST_CASE(ParFailureWithoutSuccessfulDirectUnpackStillSkipsUnpack)
{
	for (NzbInfo::EDirectUnpackStatus directStatus : {NzbInfo::nsNone, NzbInfo::nsFailure})
	{
		Options::CmdOptList cmdOpts = MakeParFailureTestOpts(nullptr);
		RestoreOptionsGlobal restoreOptions;
		Options options(&cmdOpts, nullptr);
		DirectUnpackDownloadQueueMock downloadQueue;
		ScopedQueueScriptCoordinator queueScripts;
		ScopedPostProcessPause postProcessPause;
		ScopedPostProcessCoordinators postProcessCoordinators;
		ScopedTempDir workingDir;
		const int nzbId = QueueParFailureTestNzb(downloadQueue,
			"ParFailureWithoutSuccessfulDirectUnpackStillSkipsUnpack", workingDir.Get(),
			NzbInfo::psFailure, directStatus);

		ScopedPrePostProcessor processor;
		BOOST_REQUIRE(WithNzbInfo(nzbId, [&](NzbInfo* info)
		{
			processor.Get()->NzbDownloaded(&downloadQueue, info);
			return true;
		}));
		BOOST_REQUIRE(WaitFor([&]
		{
			return WithNzbInfo(nzbId, [](NzbInfo* info)
			{
				return info->GetPostInfo() && info->GetUnpackStatus() != NzbInfo::usNone;
			});
		}));
		BOOST_CHECK(WithNzbInfo(nzbId, [](NzbInfo* info) { return info->GetUnpackStatus() == NzbInfo::usSkipped; }));
	}
}

BOOST_AUTO_TEST_SUITE_END()
