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

#ifndef WIN32
#include <sys/resource.h>
#include <csignal>
#include <fstream>
#endif

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

namespace
{
struct Utf8TempDir
{
	Utf8TempDir()
	{
		path = fs::temp_directory_path() /
			fs::u8path("nzbget_l\xC3\xA4_utf8_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		fs::create_directories(path);
	}

	~Utf8TempDir()
	{
		fs::error_code ec;
		fs::remove_all(path, ec);
	}

	std::string Str() const { return fs::u8string(path); }

	fs::path path;
};
}

BOOST_AUTO_TEST_CASE(GetDiskStateUtf8PathTest)
{
	Utf8TempDir dir;

	auto state = FileSystem::GetDiskState(dir.Str().c_str());

	BOOST_REQUIRE(state.has_value());
	BOOST_CHECK(state->total > 0);
	BOOST_CHECK(state->available > 0);
}

BOOST_AUTO_TEST_CASE(DeleteFileUtf8PathTest)
{
	Utf8TempDir dir;
	fs::path file = dir.path / fs::u8path("\xE4\xBD\xA0\xE5\xA5\xBD_l\xC3\xA4.bin");
	{
		std::ofstream(file, std::ios::binary) << "data";
	}
	BOOST_REQUIRE(fs::exists(file));

	BOOST_CHECK(FileSystem::DeleteFile(fs::u8string(file).c_str()));
	BOOST_CHECK(!fs::exists(file));
}

BOOST_AUTO_TEST_CASE(DeleteReadOnlyFileUtf8PathTest)
{
	Utf8TempDir dir;
	fs::path file = dir.path / fs::u8path("readonly_l\xC3\xA4.bin");
	{
		std::ofstream(file, std::ios::binary) << "data";
	}
	fs::permissions(file, fs::perms::owner_read, fs::perm_options::replace);

	BOOST_CHECK(FileSystem::DeleteFile(fs::u8string(file).c_str()));
	BOOST_CHECK(!fs::exists(file));
}

#ifndef WIN32
BOOST_AUTO_TEST_CASE(CopyFileFailsOnFailedWrite)
{
	// a file size limit makes writes past 64 KB fail, as a full disk does
	const fs::path dir = fs::temp_directory_path() / "nzbget-copyfile-test";
	fs::remove_all(dir);
	fs::create_directories(dir);
	const std::string src = (dir / "src.bin").string();
	const std::string dst = (dir / "dst.bin").string();
	std::ofstream(src, std::ios::binary) << std::string(300 * 1024, 'x');

	struct rlimit oldLimit;
	getrlimit(RLIMIT_FSIZE, &oldLimit);
	struct rlimit limit = oldLimit;
	limit.rlim_cur = 64 * 1024;
	auto oldHandler = signal(SIGXFSZ, SIG_IGN);
	setrlimit(RLIMIT_FSIZE, &limit);

	bool copied = FileSystem::CopyFile(src.c_str(), dst.c_str());

	setrlimit(RLIMIT_FSIZE, &oldLimit);
	signal(SIGXFSZ, oldHandler);

	BOOST_CHECK(!copied);
	BOOST_CHECK(!fs::exists(dst));
	BOOST_CHECK(FileSystem::CopyFile(src.c_str(), dst.c_str()));
	BOOST_CHECK_EQUAL(fs::file_size(dst), 300 * 1024);

	fs::remove_all(dir);
}
#endif

BOOST_AUTO_TEST_SUITE_END()
