/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include "FileSystem.h"

BOOST_AUTO_TEST_SUITE(UtilTest)

#ifdef WIN32
BOOST_AUTO_TEST_CASE(FileSystemTest)
{
	BOOST_CHECK(!strcmp(FileSystem::MakeCanonicalPath("C:\\Program Files\\NZBGet"), "C:\\Program Files\\NZBGet"));
	BOOST_CHECK(!strcmp(FileSystem::MakeCanonicalPath("C:\\Program Files\\NZBGet\\"), "C:\\Program Files\\NZBGet\\"));
	BOOST_CHECK(!strcmp(FileSystem::MakeCanonicalPath("C:\\\\Program Files\\\\NZBGet"), "C:\\Program Files\\NZBGet"));
	BOOST_CHECK(!strcmp(FileSystem::MakeCanonicalPath("C:\\Program Files\\NZBGet\\scripts\\.."), "C:\\Program Files\\NZBGet\\"));
	BOOST_CHECK(!strcmp(FileSystem::MakeCanonicalPath("C:\\Program Files\\NZBGet\\scripts\\email\\..\\.."), "C:\\Program Files\\NZBGet\\"));
	BOOST_CHECK(!strcmp(FileSystem::MakeCanonicalPath("C:\\Program Files\\NZBGet\\scripts\\email\\..\\..\\"), "C:\\Program Files\\NZBGet\\"));
	BOOST_CHECK(!strcmp(FileSystem::MakeCanonicalPath("C:\\Program Files\\NZBGet\\."), "C:\\Program Files\\NZBGet\\"));
	BOOST_CHECK(!strcmp(FileSystem::MakeCanonicalPath("\\\\server\\Program Files\\NZBGet\\scripts\\email\\..\\..\\"), "\\\\server\\Program Files\\NZBGet\\"));
}

BOOST_AUTO_TEST_CASE(ExtractFilePathCmdTest)
{
	BOOST_CHECK(FileSystem::ExtractFilePathFromCmd("C:\\Program Files\\NZBGet\\unrar.exe") == "C:\\Program Files\\NZBGet\\unrar.exe");
	BOOST_CHECK(FileSystem::ExtractFilePathFromCmd("C:\\Program Files\\NZBGet\\unrar.exe -ai") == "C:\\Program Files\\NZBGet\\unrar.exe");
	BOOST_CHECK(FileSystem::ExtractFilePathFromCmd("") == "");
}

BOOST_AUTO_TEST_CASE(EscapePathForShellTest)
{
	BOOST_CHECK(FileSystem::EscapePathForShell("C:\\Program Files\\NZBGet\\unrar.exe") == "\"C:\\Program Files\\NZBGet\\unrar.exe\"");
	BOOST_CHECK(FileSystem::EscapePathForShell("") == "");
}
#else

BOOST_AUTO_TEST_CASE(ExtractFilePathCmdTest)
{
	BOOST_CHECK(FileSystem::ExtractFilePathFromCmd("/usr/nzbget/unrar") == "/usr/nzbget/unrar");
	BOOST_CHECK(FileSystem::ExtractFilePathFromCmd("/usr/nzbget/unrar -ai") == "/usr/nzbget/unrar");
	BOOST_CHECK(FileSystem::ExtractFilePathFromCmd("") == "");
}

BOOST_AUTO_TEST_CASE(EscapePathForShellTest)
{
	BOOST_CHECK(FileSystem::EscapePathForShell("/usr/my dir/nzbget/unrar") == "\"/usr/my dir/nzbget/unrar\"");
	BOOST_CHECK(FileSystem::EscapePathForShell("") == "");
}

BOOST_AUTO_TEST_CASE(DeleteDirectoryWithContentDoesNotFollowNestedSymlink)
{
	const fs::path outsideDir = fs::temp_directory_path() / fs::make_unique_filename("nzbget-delete-outside-%%%%-%%%%");
	const fs::path scratchDir = fs::temp_directory_path() / fs::make_unique_filename("nzbget-delete-scratch-%%%%-%%%%");
	const fs::path sentinel = outsideDir / "sentinel";
	const fs::path escape = scratchDir / "escape";

	BOOST_REQUIRE(fs::create_directory(outsideDir));
	BOOST_REQUIRE(fs::create_directory(scratchDir));
	BOOST_REQUIRE(FileSystem::SaveBufferIntoFile(sentinel.string().c_str(), "safe", 4));
	fs::create_directory_symlink(outsideDir, escape);

	CString errmsg;
	const bool deleted = FileSystem::DeleteDirectoryWithContent(scratchDir.string().c_str(), errmsg);
	const bool sentinelSurvived = fs::exists(sentinel);
	const bool scratchRemoved = !fs::exists(scratchDir) && !fs::is_symlink(scratchDir);

	fs::remove_all(scratchDir);
	fs::remove_all(outsideDir);

	BOOST_CHECK_MESSAGE(deleted, errmsg.Str());
	BOOST_CHECK(sentinelSurvived);
	BOOST_CHECK(scratchRemoved);
}

BOOST_AUTO_TEST_CASE(DeleteDirectoryWithContentDoesNotFollowRootSymlink)
{
	const fs::path outsideDir = fs::temp_directory_path() / fs::make_unique_filename("nzbget-delete-root-outside-%%%%-%%%%");
	const fs::path rootLink = fs::temp_directory_path() / fs::make_unique_filename("nzbget-delete-root-link-%%%%-%%%%");
	const fs::path sentinel = outsideDir / "sentinel";

	BOOST_REQUIRE(fs::create_directory(outsideDir));
	BOOST_REQUIRE(FileSystem::SaveBufferIntoFile(sentinel.string().c_str(), "safe", 4));
	fs::create_directory_symlink(outsideDir, rootLink);

	CString errmsg;
	const bool deleted = FileSystem::DeleteDirectoryWithContent(rootLink.string().c_str(), errmsg);
	const bool sentinelSurvived = fs::exists(sentinel);
	const bool rootLinkRemoved = !fs::exists(rootLink) && !fs::is_symlink(rootLink);

	fs::remove(rootLink);
	fs::remove_all(outsideDir);

	BOOST_CHECK_MESSAGE(deleted, errmsg.Str());
	BOOST_CHECK(sentinelSurvived);
	BOOST_CHECK(rootLinkRemoved);
}

BOOST_AUTO_TEST_CASE(CreateDirectoryExclusiveRejectsSymlink)
{
	const fs::path outsideDir = fs::temp_directory_path() / fs::make_unique_filename("nzbget-create-outside-%%%%-%%%%");
	const fs::path dirLink = fs::temp_directory_path() / fs::make_unique_filename("nzbget-create-link-%%%%-%%%%");

	BOOST_REQUIRE(fs::create_directory(outsideDir));
	fs::create_directory_symlink(outsideDir, dirLink);

	const bool created = FileSystem::CreateDirectoryExclusive(dirLink.string().c_str());
	const bool linkStillPresent = fs::is_symlink(dirLink);
	const bool targetStillPresent = fs::is_directory(outsideDir);

	fs::remove(dirLink);
	fs::remove_all(outsideDir);

	BOOST_CHECK(!created);
	BOOST_CHECK(linkStillPresent);
	BOOST_CHECK(targetStillPresent);
}

#endif

BOOST_AUTO_TEST_CASE(CreateDirectoryExclusiveCreatesOnce)
{
	const fs::path newDir = fs::temp_directory_path() / fs::make_unique_filename("nzbget-create-exclusive-%%%%-%%%%");

	const bool firstCreate = FileSystem::CreateDirectoryExclusive(newDir.string().c_str());
	const bool secondCreate = FileSystem::CreateDirectoryExclusive(newDir.string().c_str());
	const bool directoryExists = fs::is_directory(newDir);

	fs::remove_all(newDir);

	BOOST_CHECK(firstCreate);
	BOOST_CHECK(!secondCreate);
	BOOST_CHECK(directoryExists);
}

BOOST_AUTO_TEST_CASE(SplitPathAndFilenameTest)
{
	{
		std::string fullPath = "/path/to/filename.txt";
		std::pair<std::string, std::string> result = FileSystem::SplitPathAndFilename(fullPath);
		BOOST_TEST(result.first == "/path/to");
		BOOST_TEST(result.second == "filename.txt");
	}

	{
		std::string fullPath = "/path/to/";
		std::pair<std::string, std::string> result = FileSystem::SplitPathAndFilename(fullPath);
		BOOST_TEST(result.first == "/path/to");
		BOOST_TEST(result.second == "");
	}

	{
		std::string fullPath = "C:\\path\\to\\filename.txt";
		std::pair<std::string, std::string> result = FileSystem::SplitPathAndFilename(fullPath);
		BOOST_TEST(result.first == "C:\\path\\to");
		BOOST_TEST(result.second == "filename.txt");
	}

	{
		std::string fullPath = "C:\\path\\to\\";
		std::pair<std::string, std::string> result = FileSystem::SplitPathAndFilename(fullPath);
		BOOST_TEST(result.first == "C:\\path\\to");
		BOOST_TEST(result.second == "");
	}

	{
		std::string fullPath = "/path\\to/filename.txt";
		std::pair<std::string, std::string> result = FileSystem::SplitPathAndFilename(fullPath);
		BOOST_TEST(result.first == "/path\\to");
		BOOST_TEST(result.second == "filename.txt");
	}

	{
		std::string fullPath = "";
		std::pair<std::string, std::string> result = FileSystem::SplitPathAndFilename(fullPath);
		BOOST_TEST(result.first == "");
		BOOST_TEST(result.second == "");
	}

	{
		std::string fullPath = "/filename.txt";
		std::pair<std::string, std::string> result = FileSystem::SplitPathAndFilename(fullPath);
		BOOST_TEST(result.first == "");
		BOOST_TEST(result.second == "filename.txt");
	}

	{
		std::string fullPath = "\\filename.txt";
		std::pair<std::string, std::string> result = FileSystem::SplitPathAndFilename(fullPath);
		BOOST_TEST(result.first == "");
		BOOST_TEST(result.second == "filename.txt");
	}

	{
		std::string fullPath = "/path/to\\a/b\\c/filename.txt";
		std::pair<std::string, std::string> result = FileSystem::SplitPathAndFilename(fullPath);
		BOOST_TEST(result.first == "/path/to\\a/b\\c");
		BOOST_TEST(result.second == "filename.txt");
	}

	{
		std::string fullPath = "/";
		std::pair<std::string, std::string> result = FileSystem::SplitPathAndFilename(fullPath);
		BOOST_TEST(result.first == "");
		BOOST_TEST(result.second == "");
	}

	{
		std::string fullPath = "\\";
		std::pair<std::string, std::string> result = FileSystem::SplitPathAndFilename(fullPath);
		BOOST_TEST(result.first == "");
		BOOST_TEST(result.second == "");
	}
}

BOOST_AUTO_TEST_SUITE_END()
