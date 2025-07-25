/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

const std::string CURRENT_DIR = FileSystem::GetCurrentDirectory().Str();

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

BOOST_AUTO_TEST_CASE(ExistingWritableDirectory) 
{
	const auto [valid, message] = FileSystem::CheckDirAccess(".");
	BOOST_CHECK_EQUAL(valid, true);
	BOOST_CHECK_EQUAL(message, "");
}

BOOST_AUTO_TEST_CASE(NonExistentDirectory) 
{
	const auto [valid, message] = FileSystem::CheckDirAccess("non_existent_directory_for_testing");
	BOOST_CHECK_EQUAL(valid, false);
	BOOST_CHECK_EQUAL(message, "Doesn't exist");
}

BOOST_AUTO_TEST_CASE(ExistingFile) 
{
	const std::string testFile = CURRENT_DIR + "/temp_file_for_testing.txt";
	std::ofstream tempFile(testFile);
	BOOST_REQUIRE(tempFile.is_open());

	tempFile << "This is a temporary file.";
	tempFile.close();

	const auto [valid, message] = FileSystem::CheckDirAccess(testFile);
	BOOST_CHECK_EQUAL(valid, false);
	BOOST_CHECK_EQUAL(message, "Not a directory");

	BOOST_REQUIRE_EQUAL(std::remove(testFile.c_str()), 0);
}

BOOST_AUTO_TEST_CASE(ReadOnlyDirectory)
{
	const std::string testDirPath = CURRENT_DIR + "/test_readonly_dir";

	//Creating the directory with rwxrwxrwx permissions
	BOOST_REQUIRE_EQUAL(mkdir(testDirPath.c_str(), 0777), 0);

	//Removing write permissions
	BOOST_REQUIRE_EQUAL(chmod(testDirPath.c_str(), 0555), 0);

	const auto [valid, message] = FileSystem::CheckDirAccess(testDirPath);
	BOOST_CHECK_EQUAL(valid, false);

	BOOST_CHECK_EQUAL(message, "No read/write permissions");

	BOOST_REQUIRE_EQUAL(rmdir(testDirPath.c_str()), 0);
}

BOOST_AUTO_TEST_CASE(ExecutableFileDoesNotExist)
{
	const auto [valid, message] = FileSystem::CheckExeAccess("nonexistent_executable");
	BOOST_CHECK_EQUAL(valid, false);
	BOOST_CHECK_EQUAL(message, "Doesn't exist");
}

BOOST_AUTO_TEST_CASE(NonExecutableFileExists)
{
	const std::string filename = "temp_nonexecutable_file.txt";
	std::ofstream tempFile(filename);
	BOOST_REQUIRE(tempFile.is_open());
	tempFile << "This is a non-executable file.";
	tempFile.close();

	const auto [valid, message] = FileSystem::CheckExeAccess(filename);
	BOOST_CHECK_EQUAL(valid, false);
	BOOST_CHECK_EQUAL(message, "No execute permission");

	BOOST_REQUIRE_EQUAL(std::remove(filename.c_str()), 0);
}
