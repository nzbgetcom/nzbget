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
#include "Options.h"
#include "DownloadInfo.h"
#include "DirectRenamer.h"

namespace
{
	const fs::path CURR_DIR = fs::current_path();

	// DirectRenamer's hardlink-related members are protected so they can be exercised
	// directly here, without going through the full PAR2/threaded RenameFiles pipeline
	// (DirectParLoader/DirectParRepairer), which requires real PAR2 data.
	class TestDirectRenamer final : public DirectRenamer
	{
	public:
		void RenameCompleted(DownloadQueue*, NzbInfo*) override {}
		using DirectRenamer::HardLinkFiles;
	};

	void WriteFile(const fs::path& path, const std::string& content = "data")
	{
		std::ofstream f(path, std::ios::binary);
		f << content;
	}

	std::string ReadFileContents(const fs::path& path)
	{
		std::ifstream in(path, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	}

	std::unique_ptr<NzbInfo> MakeNzbInfo(const std::string& name, const fs::path& destDir)
	{
		auto nzbInfo = std::make_unique<NzbInfo>();
		nzbInfo->SetName(name.c_str());
		nzbInfo->SetDestDir(destDir.string().c_str());
		return nzbInfo;
	}

	// Owns the option strings and the Options instance (which sets the global g_Options
	// in its constructor and clears it in its destructor) for the lifetime of a test.
	struct TestEnv
	{
		std::vector<std::string> storage;
		Options::CmdOptList cmdOpts;
		std::unique_ptr<Options> options;

		TestEnv(const fs::path& interDir, const fs::path& destDir, bool hardLinking = true)
		{
			storage.push_back("InterDir=" + interDir.string());
			storage.push_back("DestDir=" + destDir.string());
			storage.push_back(hardLinking ? "HardLinking=yes" : "HardLinking=no");
			for (const std::string& s : storage)
			{
				cmdOpts.push_back(s.c_str());
			}
			options = std::make_unique<Options>(&cmdOpts, nullptr);
		}
	};
}

BOOST_AUTO_TEST_SUITE(QueueTest)

BOOST_AUTO_TEST_CASE(HardLinkFileInfoSuccessTest)
{
	const fs::path workDir = CURR_DIR / "DirectRenamer_FileInfoSuccess_work";
	const fs::path destDir = CURR_DIR / "DirectRenamer_FileInfoSuccess_dest";
	const std::string nzbName = "MyRelease";
	const fs::path finalDir = destDir / nzbName;

	fs::remove_all(workDir);
	fs::remove_all(destDir);
	fs::create_directories(workDir);

	const fs::path sourceFile = workDir / "file1.mkv";
	WriteFile(sourceFile);

	TestEnv env(workDir, destDir);

	auto nzbInfo = MakeNzbInfo(nzbName, workDir);

	auto fileInfo = std::make_unique<FileInfo>();
	fileInfo->SetFilename("file1.mkv");
	fileInfo->SetOutputFilename(sourceFile.string().c_str());
	fileInfo->SetParFile(false);
	FileInfo* fileInfoPtr = fileInfo.get();
	nzbInfo->GetFileList()->Add(std::move(fileInfo), false);

	TestDirectRenamer renamer;
	renamer.HardLinkFiles(nzbInfo.get());

	BOOST_CHECK(fs::exists(finalDir / "file1.mkv"));
	fs::error_code ec;
	BOOST_CHECK(fs::equivalent(sourceFile, finalDir / "file1.mkv", ec));
	BOOST_CHECK(!fileInfoPtr->GetHardLinkPath().empty());
	BOOST_CHECK_EQUAL(fileInfoPtr->GetHardLinkPath(), (finalDir / "file1.mkv").string());
	BOOST_CHECK(!nzbInfo->GetHardLinkPath().empty());

	fs::remove_all(workDir);
	fs::remove_all(destDir);
}

BOOST_AUTO_TEST_CASE(HardLinkFileInfoNotAssembledSkipsTest)
{
	const fs::path workDir = CURR_DIR / "DirectRenamer_FileInfoNotAssembled_work";
	const fs::path destDir = CURR_DIR / "DirectRenamer_FileInfoNotAssembled_dest";
	const std::string nzbName = "MyRelease";
	const fs::path finalDir = destDir / nzbName;

	fs::remove_all(workDir);
	fs::remove_all(destDir);
	fs::create_directories(workDir);

	TestEnv env(workDir, destDir);

	auto nzbInfo = MakeNzbInfo(nzbName, workDir);

	auto fileInfo = std::make_unique<FileInfo>();
	fileInfo->SetFilename("file1.mkv");
	// Output file was never written - article assembly still in progress.
	fileInfo->SetOutputFilename((workDir / "file1.mkv").string().c_str());
	fileInfo->SetParFile(false);
	FileInfo* fileInfoPtr = fileInfo.get();
	nzbInfo->GetFileList()->Add(std::move(fileInfo), false);

	TestDirectRenamer renamer;
	renamer.HardLinkFiles(nzbInfo.get());

	BOOST_CHECK(!fs::exists(finalDir / "file1.mkv"));
	BOOST_CHECK(fileInfoPtr->GetHardLinkPath().empty());

	fs::remove_all(workDir);
	fs::remove_all(destDir);
}

BOOST_AUTO_TEST_CASE(HardLinkFileInfoAlreadyLinkedSkipsTest)
{
	const fs::path workDir = CURR_DIR / "DirectRenamer_FileInfoAlreadyLinked_work";
	const fs::path destDir = CURR_DIR / "DirectRenamer_FileInfoAlreadyLinked_dest";
	const std::string nzbName = "MyRelease";
	const fs::path finalDir = destDir / nzbName;

	fs::remove_all(workDir);
	fs::remove_all(destDir);
	fs::create_directories(workDir);

	const fs::path sourceFile = workDir / "file1.mkv";
	WriteFile(sourceFile);

	TestEnv env(workDir, destDir);

	auto nzbInfo = MakeNzbInfo(nzbName, workDir);

	auto fileInfo = std::make_unique<FileInfo>();
	fileInfo->SetFilename("file1.mkv");
	fileInfo->SetOutputFilename(sourceFile.string().c_str());
	fileInfo->SetParFile(false);
	const std::string sentinel = "already-linked-sentinel";
	fileInfo->SetHardLinkPath(sentinel);
	FileInfo* fileInfoPtr = fileInfo.get();
	nzbInfo->GetFileList()->Add(std::move(fileInfo), false);

	TestDirectRenamer renamer;
	renamer.HardLinkFiles(nzbInfo.get());

	// Already-linked files must not be reprocessed: the sentinel path is left untouched
	// and nothing gets created at the final directory.
	BOOST_CHECK_EQUAL(fileInfoPtr->GetHardLinkPath(), sentinel);
	BOOST_CHECK(!fs::exists(finalDir / "file1.mkv"));

	fs::remove_all(workDir);
	fs::remove_all(destDir);
}

BOOST_AUTO_TEST_CASE(HardLinkFileInfoParFileSkippedTest)
{
	const fs::path workDir = CURR_DIR / "DirectRenamer_FileInfoParSkipped_work";
	const fs::path destDir = CURR_DIR / "DirectRenamer_FileInfoParSkipped_dest";
	const std::string nzbName = "MyRelease";
	const fs::path finalDir = destDir / nzbName;

	fs::remove_all(workDir);
	fs::remove_all(destDir);
	fs::create_directories(workDir);

	const fs::path sourceFile = workDir / "set.vol000+01.par2";
	WriteFile(sourceFile);

	TestEnv env(workDir, destDir);

	auto nzbInfo = MakeNzbInfo(nzbName, workDir);

	auto fileInfo = std::make_unique<FileInfo>();
	fileInfo->SetFilename("set.vol000+01.par2");
	fileInfo->SetOutputFilename(sourceFile.string().c_str());
	fileInfo->SetParFile(true);
	FileInfo* fileInfoPtr = fileInfo.get();
	nzbInfo->GetFileList()->Add(std::move(fileInfo), false);

	TestDirectRenamer renamer;
	renamer.HardLinkFiles(nzbInfo.get());

	BOOST_CHECK(!fs::exists(finalDir / "set.vol000+01.par2"));
	BOOST_CHECK(fileInfoPtr->GetHardLinkPath().empty());

	fs::remove_all(workDir);
	fs::remove_all(destDir);
}

BOOST_AUTO_TEST_CASE(HardLinkDisabledIsNoOpTest)
{
	const fs::path workDir = CURR_DIR / "DirectRenamer_Disabled_work";
	const fs::path destDir = CURR_DIR / "DirectRenamer_Disabled_dest";
	const std::string nzbName = "MyRelease";
	const fs::path finalDir = destDir / nzbName;

	fs::remove_all(workDir);
	fs::remove_all(destDir);
	fs::create_directories(workDir);

	const fs::path sourceFile = workDir / "file1.mkv";
	WriteFile(sourceFile);

	TestEnv env(workDir, destDir, /*hardLinking=*/false);

	auto nzbInfo = MakeNzbInfo(nzbName, workDir);

	auto fileInfo = std::make_unique<FileInfo>();
	fileInfo->SetFilename("file1.mkv");
	fileInfo->SetOutputFilename(sourceFile.string().c_str());
	fileInfo->SetParFile(false);
	nzbInfo->GetFileList()->Add(std::move(fileInfo), false);

	TestDirectRenamer renamer;
	renamer.HardLinkFiles(nzbInfo.get());

	BOOST_CHECK(!fs::exists(finalDir));

	fs::remove_all(workDir);
	fs::remove_all(destDir);
}

BOOST_AUTO_TEST_CASE(HardLinkCompletedFileSuccessTest)
{
	const fs::path workDir = CURR_DIR / "DirectRenamer_CompletedSuccess_work";
	const fs::path destDir = CURR_DIR / "DirectRenamer_CompletedSuccess_dest";
	const std::string nzbName = "MyRelease";
	const fs::path finalDir = destDir / nzbName;

	fs::remove_all(workDir);
	fs::remove_all(destDir);
	fs::create_directories(workDir);

	const fs::path sourceFile = workDir / "file1.mkv";
	WriteFile(sourceFile);

	TestEnv env(workDir, destDir);

	auto nzbInfo = MakeNzbInfo(nzbName, workDir);
	nzbInfo->GetCompletedFiles()->emplace_back(
		1, "file1.mkv", "file1.mkv", CompletedFile::cfSuccess, 0, false, "", "");

	TestDirectRenamer renamer;
	renamer.HardLinkFiles(nzbInfo.get());

	BOOST_CHECK(fs::exists(finalDir / "file1.mkv"));
	fs::error_code ec;
	BOOST_CHECK(fs::equivalent(sourceFile, finalDir / "file1.mkv", ec));

	fs::remove_all(workDir);
	fs::remove_all(destDir);
}

BOOST_AUTO_TEST_CASE(HardLinkCompletedFileNotFoundSkipsTest)
{
	const fs::path workDir = CURR_DIR / "DirectRenamer_CompletedNotFound_work";
	const fs::path destDir = CURR_DIR / "DirectRenamer_CompletedNotFound_dest";
	const std::string nzbName = "MyRelease";
	const fs::path finalDir = destDir / nzbName;

	fs::remove_all(workDir);
	fs::remove_all(destDir);
	fs::create_directories(workDir);

	TestEnv env(workDir, destDir);

	auto nzbInfo = MakeNzbInfo(nzbName, workDir);
	// No file actually written to workDir for "file1.mkv".
	nzbInfo->GetCompletedFiles()->emplace_back(
		1, "file1.mkv", "file1.mkv", CompletedFile::cfSuccess, 0, false, "", "");

	TestDirectRenamer renamer;
	renamer.HardLinkFiles(nzbInfo.get());

	BOOST_CHECK(!fs::exists(finalDir / "file1.mkv"));

	fs::remove_all(workDir);
	fs::remove_all(destDir);
}

// Regression test for the collision-handling fix: hardlinking must never delete a
// pre-existing, unrelated file at the destination (e.g. a name collision between two
// different downloads, or a stale leftover from a previous run). It should skip and
// leave both the source and the conflicting destination file intact.
BOOST_AUTO_TEST_CASE(HardLinkCollisionDoesNotDeleteExistingFileTest)
{
	const fs::path workDir = CURR_DIR / "DirectRenamer_Collision_work";
	const fs::path destDir = CURR_DIR / "DirectRenamer_Collision_dest";
	const std::string nzbName = "MyRelease";
	const fs::path finalDir = destDir / nzbName;

	fs::remove_all(workDir);
	fs::remove_all(destDir);
	fs::create_directories(workDir);
	fs::create_directories(finalDir);

	const fs::path sourceFile = workDir / "file1.mkv";
	WriteFile(sourceFile, "new-content");

	const fs::path conflictingFile = finalDir / "file1.mkv";
	WriteFile(conflictingFile, "pre-existing-unrelated-content");

	TestEnv env(workDir, destDir);

	auto nzbInfo = MakeNzbInfo(nzbName, workDir);

	auto fileInfo = std::make_unique<FileInfo>();
	fileInfo->SetFilename("file1.mkv");
	fileInfo->SetOutputFilename(sourceFile.string().c_str());
	fileInfo->SetParFile(false);
	FileInfo* fileInfoPtr = fileInfo.get();
	nzbInfo->GetFileList()->Add(std::move(fileInfo), false);

	TestDirectRenamer renamer;
	renamer.HardLinkFiles(nzbInfo.get());

	// The pre-existing, unrelated file at the destination must survive untouched.
	BOOST_REQUIRE(fs::exists(conflictingFile));
	BOOST_CHECK_EQUAL(ReadFileContents(conflictingFile), "pre-existing-unrelated-content");

	// The source file must also remain intact (not consumed/removed).
	BOOST_CHECK(fs::exists(sourceFile));

	// No hardlink was recorded, since linking was skipped.
	BOOST_CHECK(fileInfoPtr->GetHardLinkPath().empty());

	fs::remove_all(workDir);
	fs::remove_all(destDir);
}

// Re-running hardlinking against a final path that already IS the same hardlinked file
// (e.g. a retry after an earlier partial run) must be a safe no-op, not an error.
BOOST_AUTO_TEST_CASE(HardLinkAlreadyEquivalentIsIdempotentTest)
{
	const fs::path workDir = CURR_DIR / "DirectRenamer_Idempotent_work";
	const fs::path destDir = CURR_DIR / "DirectRenamer_Idempotent_dest";
	const std::string nzbName = "MyRelease";
	const fs::path finalDir = destDir / nzbName;

	fs::remove_all(workDir);
	fs::remove_all(destDir);
	fs::create_directories(workDir);
	fs::create_directories(finalDir);

	const fs::path sourceFile = workDir / "file1.mkv";
	WriteFile(sourceFile);

	fs::error_code ec;
	fs::create_hard_link(sourceFile, finalDir / "file1.mkv", ec);
	BOOST_REQUIRE_MESSAGE(!ec, "create_hard_link failed: " << ec.message());

	TestEnv env(workDir, destDir);

	auto nzbInfo = MakeNzbInfo(nzbName, workDir);

	auto fileInfo = std::make_unique<FileInfo>();
	fileInfo->SetFilename("file1.mkv");
	fileInfo->SetOutputFilename(sourceFile.string().c_str());
	fileInfo->SetParFile(false);
	FileInfo* fileInfoPtr = fileInfo.get();
	nzbInfo->GetFileList()->Add(std::move(fileInfo), false);

	TestDirectRenamer renamer;
	renamer.HardLinkFiles(nzbInfo.get());

	BOOST_CHECK(fs::exists(finalDir / "file1.mkv"));
	BOOST_CHECK(!fileInfoPtr->GetHardLinkPath().empty());

	fs::remove_all(workDir);
	fs::remove_all(destDir);
}

BOOST_AUTO_TEST_SUITE_END()
