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
#include "Cleanup.h"
#include "DownloadInfo.h"
#include "FileSystem.h"

namespace
{
	class TestMoveController final : public MoveController
	{
	public:
		using MoveController::RemoveStaleHardlinks;
		using MoveController::MoveFiles;
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
}

BOOST_AUTO_TEST_SUITE(PostprocessTest)

BOOST_AUTO_TEST_CASE(RemoveStaleHardlinksPreservesUnrelatedFilesTest)
{
	const fs::path testDir = fs::current_path() / "Cleanup_StaleHardlinks_test";
	const fs::path oldHardLinkDir = testDir / "old_hardlink_dir";
	const fs::path destDir = testDir / "dest_dir";
	const fs::path sourceFile = testDir / "source_file.mkv";

	fs::remove_all(testDir);
	fs::create_directories(oldHardLinkDir);
	fs::create_directories(destDir);

	WriteFile(sourceFile, "media-content");
	WriteFile(destDir / "source_file.mkv", "media-content"); // File in destDir

	const fs::path hardlinkedFile = oldHardLinkDir / "source_file.mkv";
	fs::error_code ec;
	fs::create_hard_link(sourceFile, hardlinkedFile, ec);
	BOOST_REQUIRE_MESSAGE(!ec, "create_hard_link failed: " << ec.message());

	const fs::path unrelatedFile = oldHardLinkDir / "unrelated.txt";
	WriteFile(unrelatedFile, "do-not-delete-me");

	NzbInfo nzbInfo;
	nzbInfo.SetHardLinkPath(oldHardLinkDir.string());

	auto fileInfo = std::make_unique<FileInfo>();
	fileInfo->SetFilename("source_file.mkv");
	fileInfo->SetHardLinkPath(hardlinkedFile.string());
	nzbInfo.GetFileList()->Add(std::move(fileInfo), false);

	TestMoveController controller;
	controller.RemoveStaleHardlinks(nzbInfo, destDir.string());

	// The hardlinked file should be cleaned up.
	BOOST_CHECK(!fs::exists(hardlinkedFile));

	// The unrelated file in the old hardlink directory MUST be preserved!
	BOOST_REQUIRE(fs::exists(unrelatedFile));
	BOOST_CHECK_EQUAL(ReadFileContents(unrelatedFile), "do-not-delete-me");

	fs::remove_all(testDir);
}

BOOST_AUTO_TEST_CASE(MoveFilesPreservesAndMovesDotfilesTest)
{
	const fs::path testDir = fs::current_path() / "Cleanup_Dotfiles_test";
	const fs::path srcDir = testDir / "inter_dir";
	const fs::path destDir = testDir / "dest_dir";

	fs::remove_all(testDir);
	fs::create_directories(srcDir / ".hidden_subdir");
	fs::create_directories(destDir);

	WriteFile(srcDir / "file.mkv", "normal-file");
	WriteFile(srcDir / ".DS_Store", "ds-store-data");
	WriteFile(srcDir / ".hidden_subdir" / ".dotfile", "hidden-data");

	TestMoveController controller;
	bool ok = controller.MoveFiles(srcDir, destDir);

	BOOST_CHECK(ok);

	// Normal file moved
	BOOST_CHECK(fs::exists(destDir / "file.mkv"));
	BOOST_CHECK(!fs::exists(srcDir / "file.mkv"));

	// Dotfiles/dot-dirs MUST be moved to destDir and NOT skipped/destroyed!
	BOOST_CHECK(fs::exists(destDir / ".DS_Store"));
	BOOST_CHECK_EQUAL(ReadFileContents(destDir / ".DS_Store"), "ds-store-data");

	BOOST_CHECK(fs::exists(destDir / ".hidden_subdir" / ".dotfile"));
	BOOST_CHECK_EQUAL(ReadFileContents(destDir / ".hidden_subdir" / ".dotfile"), "hidden-data");

	fs::remove_all(testDir);
}

BOOST_AUTO_TEST_SUITE_END()
