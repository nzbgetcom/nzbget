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

#endif

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

BOOST_AUTO_TEST_SUITE_END()
