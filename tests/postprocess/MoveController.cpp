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
#include "FileSystem.h"
#include "Cleanup.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

BOOST_AUTO_TEST_SUITE(PostprocessTest)

class MoveControllerDownloadQueueMock final : public DownloadQueue
{
public:
	MoveControllerDownloadQueueMock()
	{
		Init(this);
		Loaded();
	}
	~MoveControllerDownloadQueueMock()
	{
		Final();
	}
	bool EditEntry(int ID, EEditAction action, const char* args) { return false; }
	bool EditList(IdList* idList, NameList* nameList, EMatchMode matchMode,
		EEditAction action, const char* args) { return false; }
	void HistoryChanged() {}
	void Save() {}
	void SaveChanged() {}
};

static Options::CmdOptList MakeMoveTestOptions()
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("WriteLog=none");
	cmdOpts.push_back("NzbLog=no");
	return cmdOpts;
}

static std::unique_ptr<NzbInfo> MakeMoveNzbInfo(const fs::path& src, const fs::path& dst)
{
	auto nzbInfo = std::make_unique<NzbInfo>();
	nzbInfo->SetName("MoveControllerTest");
	nzbInfo->SetDestDir(src.string().c_str());
	nzbInfo->SetFinalDir(dst.string().c_str());
	nzbInfo->EnterPostProcess();
	return nzbInfo;
}

static void RunMove(NzbInfo* nzbInfo)
{
	MoveController::StartJob(nzbInfo->GetPostInfo());

	for (int elapsed = 0; elapsed < 10000 && nzbInfo->GetMoveStatus() == NzbInfo::msNone; elapsed += 20)
	{
		Util::Sleep(20);
	}
	BOOST_CHECK(nzbInfo->GetMoveStatus() != NzbInfo::msNone);

	// Wait for the thread to finish and clean up
	while (nzbInfo->GetPostInfo()->GetWorking())
	{
		Util::Sleep(10);
	}
	delete nzbInfo->GetPostInfo()->GetPostThread();
	nzbInfo->GetPostInfo()->SetPostThread(nullptr);
}

BOOST_AUTO_TEST_CASE(UniqueFilenameNoCollision)
{
	const fs::path workDir = fs::temp_directory_path() / "nzbget_test_movecontroller_1";
	fs::remove_all(workDir);
	fs::create_directories(workDir);

	const fs::path target = workDir / "file.mkv";
	const fs::path result = fs::make_unique_filename(target);
	BOOST_CHECK(result == target);

	fs::remove_all(workDir);
}

BOOST_AUTO_TEST_CASE(UniqueFilenameOneCollision)
{
	const fs::path workDir = fs::temp_directory_path() / "nzbget_test_movecontroller_2";
	fs::remove_all(workDir);
	fs::create_directories(workDir);

	const fs::path target = workDir / "file.mkv";
	std::ofstream(target.string()) << "existing";
	BOOST_REQUIRE(fs::exists(target));

	const fs::path result = fs::make_unique_filename(target);
	BOOST_CHECK(result == workDir / "file (1).mkv");

	fs::remove_all(workDir);
}

BOOST_AUTO_TEST_CASE(UniqueFilenameMultipleCollisions)
{
	const fs::path workDir = fs::temp_directory_path() / "nzbget_test_movecontroller_3";
	fs::remove_all(workDir);
	fs::create_directories(workDir);

	const fs::path target = workDir / "file.mkv";
	std::ofstream(target.string()) << "existing";
	std::ofstream((workDir / "file (1).mkv").string()) << "collision1";
	BOOST_REQUIRE(fs::exists(target));
	BOOST_REQUIRE(fs::exists(workDir / "file (1).mkv"));

	const fs::path result = fs::make_unique_filename(target);
	BOOST_CHECK(result == workDir / "file (2).mkv");

	fs::remove_all(workDir);
}

BOOST_AUTO_TEST_CASE(UniqueFilenameNoExtension)
{
	const fs::path workDir = fs::temp_directory_path() / "nzbget_test_movecontroller_4";
	fs::remove_all(workDir);
	fs::create_directories(workDir);

	const fs::path target = workDir / "Makefile";
	std::ofstream(target.string()) << "existing";
	BOOST_REQUIRE(fs::exists(target));

	const fs::path result = fs::make_unique_filename(target);
	BOOST_CHECK(result == workDir / "Makefile (1)");

	fs::remove_all(workDir);
}

BOOST_AUTO_TEST_CASE(UniqueFilenameUtf8)
{
	const fs::path workDir = fs::temp_directory_path() / "nzbget_test_movecontroller_utf8";
	fs::remove_all(workDir);
	fs::create_directories(workDir);

	const fs::path target = workDir / fs::u8path("vidéo_déjà_vu.mkv");
	std::ofstream(target) << "existing";
	BOOST_REQUIRE(fs::exists(target));

	const fs::path result = fs::make_unique_filename(target);
	BOOST_CHECK(result == workDir / fs::u8path("vidéo_déjà_vu (1).mkv"));

	fs::remove_all(workDir);
}

BOOST_AUTO_TEST_CASE(MoveControllerCollisionKeepsBothAndRenamesRecord)
{
	Options::CmdOptList cmdOpts = MakeMoveTestOptions();
	Options options(&cmdOpts, nullptr);

	MoveControllerDownloadQueueMock downloadQueue;

	const fs::path workDir = fs::temp_directory_path() / "nzbget_test_movecontroller_move1";
	const fs::path src = workDir / "src";
	const fs::path dst = workDir / "dst";
	fs::remove_all(workDir);
	fs::create_directories(src);
	fs::create_directories(dst);

	std::ofstream((src / "video.mkv").string()) << "new data";
	std::ofstream((dst / "video.mkv").string()) << "old data";
	BOOST_REQUIRE(fs::exists(src / "video.mkv"));
	BOOST_REQUIRE(fs::exists(dst / "video.mkv"));

	auto nzbInfo = MakeMoveNzbInfo(src, dst);
	nzbInfo->GetCompletedFiles()->emplace_back(
		1, "video.mkv", "", CompletedFile::cfSuccess, 0, false, "", "");

	RunMove(nzbInfo.get());
	BOOST_CHECK_EQUAL(nzbInfo->GetMoveStatus(), NzbInfo::msSuccess);

	BOOST_CHECK(fs::exists(dst / "video.mkv"));
	BOOST_CHECK(fs::exists(dst / "video (1).mkv"));
	BOOST_CHECK(!fs::exists(src / "video.mkv"));

	{
		std::ifstream newFile(dst / "video (1).mkv");
		std::string content;
		std::getline(newFile, content);
		BOOST_CHECK(content == "new data");
	}

	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCompletedFiles()->at(0).GetFilename()),
		"video (1).mkv");
	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCompletedFiles()->at(0).GetOrigname()),
		"video.mkv");

	fs::remove_all(workDir);
}

BOOST_AUTO_TEST_CASE(MoveControllerNoCollisionMovesFile)
{
	Options::CmdOptList cmdOpts = MakeMoveTestOptions();
	Options options(&cmdOpts, nullptr);

	MoveControllerDownloadQueueMock downloadQueue;

	const fs::path workDir = fs::temp_directory_path() / "nzbget_test_movecontroller_move2";
	const fs::path src = workDir / "src";
	const fs::path dst = workDir / "dst";
	fs::remove_all(workDir);
	fs::create_directories(src);
	fs::create_directories(dst);

	std::ofstream((src / "video.mkv").string()) << "data";

	auto nzbInfo = MakeMoveNzbInfo(src, dst);

	RunMove(nzbInfo.get());
	BOOST_CHECK_EQUAL(nzbInfo->GetMoveStatus(), NzbInfo::msSuccess);

	BOOST_CHECK(fs::exists(dst / "video.mkv"));
	BOOST_CHECK(!fs::exists(src / "video.mkv"));
	BOOST_CHECK(nzbInfo->GetCompletedFiles()->empty());

	fs::remove_all(workDir);
}

BOOST_AUTO_TEST_CASE(MoveControllerSameInodeRemovesSource)
{
	Options::CmdOptList cmdOpts = MakeMoveTestOptions();
	Options options(&cmdOpts, nullptr);

	MoveControllerDownloadQueueMock downloadQueue;

	const fs::path workDir = fs::temp_directory_path() / "nzbget_test_movecontroller_move3";
	const fs::path src = workDir / "src";
	const fs::path dst = workDir / "dst";
	fs::remove_all(workDir);
	fs::create_directories(src);
	fs::create_directories(dst);

	fs::error_code ec;
	std::ofstream((dst / "video.mkv").string()) << "data";
	fs::create_hard_link(dst / "video.mkv", src / "video.mkv", ec);
	BOOST_REQUIRE(!ec);
	BOOST_CHECK(fs::equivalent(src / "video.mkv", dst / "video.mkv"));

	auto nzbInfo = MakeMoveNzbInfo(src, dst);

	RunMove(nzbInfo.get());
	BOOST_CHECK_EQUAL(nzbInfo->GetMoveStatus(), NzbInfo::msSuccess);

	BOOST_CHECK(fs::exists(dst / "video.mkv"));
	BOOST_CHECK(!fs::exists(src / "video.mkv"));

	fs::remove_all(workDir);
}

BOOST_AUTO_TEST_CASE(MoveControllerNestedDirectories)
{
	Options::CmdOptList cmdOpts = MakeMoveTestOptions();
	Options options(&cmdOpts, nullptr);

	MoveControllerDownloadQueueMock downloadQueue;

	const fs::path workDir = fs::temp_directory_path() / "nzbget_test_movecontroller_move4";
	const fs::path src = workDir / "src";
	const fs::path dst = workDir / "dst";
	fs::remove_all(workDir);
	fs::create_directories(src / "sub/dir");
	fs::create_directories(dst);

	std::ofstream((src / "sub/dir/inner.mkv").string()) << "data";

	auto nzbInfo = MakeMoveNzbInfo(src, dst);

	RunMove(nzbInfo.get());
	BOOST_CHECK_EQUAL(nzbInfo->GetMoveStatus(), NzbInfo::msSuccess);

	BOOST_CHECK(fs::exists(dst / "sub/dir/inner.mkv"));
	BOOST_CHECK(!fs::exists(src / "sub/dir/inner.mkv"));

	fs::remove_all(workDir);
}

BOOST_AUTO_TEST_CASE(MoveControllerSameDirectoryNoOp)
{
	Options::CmdOptList cmdOpts = MakeMoveTestOptions();
	Options options(&cmdOpts, nullptr);

	MoveControllerDownloadQueueMock downloadQueue;

	const fs::path workDir = fs::temp_directory_path() / "nzbget_test_movecontroller_move5";
	const fs::path src = workDir / "same";
	fs::remove_all(workDir);
	fs::create_directories(src);

	std::ofstream((src / "video.mkv").string()) << "keep me safe";
	BOOST_REQUIRE(fs::exists(src / "video.mkv"));

	auto nzbInfo = MakeMoveNzbInfo(src, src);

	RunMove(nzbInfo.get());
	BOOST_CHECK_EQUAL(nzbInfo->GetMoveStatus(), NzbInfo::msSuccess);

	// The file must survive — same-directory move is a no-op
	BOOST_CHECK(fs::exists(src / "video.mkv"));

	{
		std::ifstream f(src / "video.mkv");
		std::string content;
		std::getline(f, content);
		BOOST_CHECK_EQUAL(content, "keep me safe");
	}

	fs::remove_all(workDir);
}

BOOST_AUTO_TEST_CASE(MoveControllerCascadingCollision)
{
	Options::CmdOptList cmdOpts = MakeMoveTestOptions();
	Options options(&cmdOpts, nullptr);

	MoveControllerDownloadQueueMock downloadQueue;

	const fs::path workDir = fs::temp_directory_path() / "nzbget_test_movecontroller_cascade";
	const fs::path src = workDir / "src";
	const fs::path dst = workDir / "dst";
	fs::remove_all(workDir);
	fs::create_directories(src);
	fs::create_directories(dst);

	std::ofstream(src / "video.mkv") << "new data 2";
	std::ofstream(dst / "video.mkv") << "original data";
	std::ofstream(dst / "video (1).mkv") << "collision 1";
	BOOST_REQUIRE(fs::exists(src / "video.mkv"));
	BOOST_REQUIRE(fs::exists(dst / "video.mkv"));
	BOOST_REQUIRE(fs::exists(dst / "video (1).mkv"));

	auto nzbInfo = MakeMoveNzbInfo(src, dst);
	nzbInfo->GetCompletedFiles()->emplace_back(
		1, "video.mkv", "", CompletedFile::cfSuccess, 0, false, "", "");

	RunMove(nzbInfo.get());
	BOOST_CHECK_EQUAL(nzbInfo->GetMoveStatus(), NzbInfo::msSuccess);

	BOOST_CHECK(fs::exists(dst / "video.mkv"));
	BOOST_CHECK(fs::exists(dst / "video (1).mkv"));
	BOOST_CHECK(fs::exists(dst / "video (2).mkv"));
	BOOST_CHECK(!fs::exists(src / "video.mkv"));

	{
		std::ifstream newFile(dst / "video (2).mkv");
		std::string content;
		std::getline(newFile, content);
		BOOST_CHECK_EQUAL(content, "new data 2");
	}

	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCompletedFiles()->at(0).GetFilename()),
		"video (2).mkv");
	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCompletedFiles()->at(0).GetOrigname()),
		"video.mkv");

	fs::remove_all(workDir);
}

BOOST_AUTO_TEST_CASE(MoveControllerNestedDirectoryCollision)
{
	Options::CmdOptList cmdOpts = MakeMoveTestOptions();
	Options options(&cmdOpts, nullptr);

	MoveControllerDownloadQueueMock downloadQueue;

	const fs::path workDir = fs::temp_directory_path() / "nzbget_test_movecontroller_nested_collision";
	const fs::path src = workDir / "src";
	const fs::path dst = workDir / "dst";
	fs::remove_all(workDir);
	fs::create_directories(src / "sub/dir");
	fs::create_directories(dst / "sub/dir");

	std::ofstream(src / "sub/dir/inner.mkv") << "new nested data";
	std::ofstream(dst / "sub/dir/inner.mkv") << "existing nested data";
	BOOST_REQUIRE(fs::exists(src / "sub/dir/inner.mkv"));
	BOOST_REQUIRE(fs::exists(dst / "sub/dir/inner.mkv"));

	auto nzbInfo = MakeMoveNzbInfo(src, dst);
	nzbInfo->GetCompletedFiles()->emplace_back(
		1, "sub/dir/inner.mkv", "", CompletedFile::cfSuccess, 0, false, "", "");

	RunMove(nzbInfo.get());
	BOOST_CHECK_EQUAL(nzbInfo->GetMoveStatus(), NzbInfo::msSuccess);

	BOOST_CHECK(fs::exists(dst / "sub/dir/inner.mkv"));
	BOOST_CHECK(fs::exists(dst / "sub/dir/inner (1).mkv"));
	BOOST_CHECK(!fs::exists(src / "sub/dir/inner.mkv"));

	{
		std::ifstream newFile(dst / "sub/dir/inner (1).mkv");
		std::string content;
		std::getline(newFile, content);
		BOOST_CHECK_EQUAL(content, "new nested data");
	}

	const std::string expectedNew = fs::u8string(fs::path("sub") / "dir" / "inner (1).mkv");
	const std::string expectedOrig = fs::u8string(fs::path("sub") / "dir" / "inner.mkv");

	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCompletedFiles()->at(0).GetFilename()),
		expectedNew);
	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCompletedFiles()->at(0).GetOrigname()),
		expectedOrig);

	fs::remove_all(workDir);
}

BOOST_AUTO_TEST_CASE(MoveControllerNonExistentSourceFails)
{
	Options::CmdOptList cmdOpts = MakeMoveTestOptions();
	Options options(&cmdOpts, nullptr);

	MoveControllerDownloadQueueMock downloadQueue;

	const fs::path workDir = fs::temp_directory_path() / "nzbget_test_movecontroller_nonexistent";
	const fs::path src = workDir / "non_existent_src";
	const fs::path dst = workDir / "dst";
	fs::remove_all(workDir);

	auto nzbInfo = MakeMoveNzbInfo(src, dst);

	RunMove(nzbInfo.get());
	BOOST_CHECK_EQUAL(nzbInfo->GetMoveStatus(), NzbInfo::msFailure);

	fs::remove_all(workDir);
}

BOOST_AUTO_TEST_SUITE_END()
