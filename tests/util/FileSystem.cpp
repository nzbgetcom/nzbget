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

BOOST_AUTO_TEST_CASE(DiskFileCloseOnExecTest)
{
	CString tempFile = CString::FormatStr("/tmp/nzbget_test_cloexec_%d.tmp", (int)getpid());

	DiskFile file;
	BOOST_REQUIRE(file.Open(tempFile, DiskFile::omWrite));

	int fd = file.GetFileDescriptor();
	BOOST_REQUIRE(fd >= 0);

	int flags = fcntl(fd, F_GETFD);
	BOOST_REQUIRE(flags != -1);
	BOOST_CHECK((flags & FD_CLOEXEC) != 0);

	file.Close();
	FileSystem::DeleteFile(tempFile);
}

BOOST_AUTO_TEST_CASE(ForkExecDoesNotRetainCloseOnExecFilesTest)
{
	char tmpl[] = "/tmp/nzbget-cloexec-XXXXXX";
	int tmpfd = mkstemp(tmpl);
	BOOST_REQUIRE(tmpfd >= 0);
	close(tmpfd);

	DiskFile file;
	BOOST_REQUIRE(file.Open(tmpl, DiskFile::omRead));

	pid_t pid = fork();
	BOOST_REQUIRE(pid != -1);
	if (pid == 0)
	{
		execl("/bin/sleep", "sleep", "5", (char*)nullptr);
		_exit(127);
	}

	usleep(100000);

	file.Close();
	BOOST_CHECK(FileSystem::DeleteFile(tmpl));

	BOOST_REQUIRE(kill(pid, SIGTERM) == 0);
	int status = 0;
	waitpid(pid, &status, 0);
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

BOOST_AUTO_TEST_CASE(SanitizePathSegmentLimitsLengthTest)
{
	// A truncated name ends with "-" and 12 hex digits of SHA-256 over the original name.
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment(std::string(300, 'a') + ".mkv"),
		std::string(222, 'a') + "-2de2dc2b16e9.mkv");
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment(std::string(300, 'b')),
		std::string(226, 'b') + "-dccc1450d6fc");
	// An extension longer than 16 bytes is not kept.
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment(std::string(300, 'x') + ".very-long-extension"),
		std::string(226, 'x') + "-3d451f497dae");
}

BOOST_AUTO_TEST_CASE(SanitizePathSegmentTruncatedReservedNameStaysAtLimit)
{
	// The device prefix "_" must fit in the reserved byte, so a cut name never exceeds 240.
	const std::string result = FileSystem::SanitizePathSegment("CON." + std::string(300, 'a') + ".mkv");
	BOOST_CHECK_EQUAL(result.size(), 240u);
	BOOST_CHECK(result.starts_with("_CON."));
	BOOST_CHECK(result.ends_with(".mkv"));
}

BOOST_AUTO_TEST_CASE(SanitizePathSegmentKeepsLongNamesDistinct)
{
	// Same first 240 bytes, different tails: plain prefix truncation would map both to one name.
	const std::string prefix(250, 'a');
	const std::string one = FileSystem::SanitizePathSegment(prefix + "-one.mkv");
	const std::string two = FileSystem::SanitizePathSegment(prefix + "-two.mkv");
	BOOST_CHECK_EQUAL(one.size(), 239u);
	BOOST_CHECK_EQUAL(two.size(), 239u);
	BOOST_CHECK(one != two);
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment(prefix + "-one.mkv"), one);
}

BOOST_AUTO_TEST_CASE(SanitizePathSegmentKeepsUtf8CharactersWhole)
{
	// "é" is two bytes; the cut backs up to a character boundary instead of splitting one.
	std::string name = "a";
	for (int i = 0; i < 200; i++)
	{
		name += "é";
	}

	const std::string result = FileSystem::SanitizePathSegment(name + ".mkv");
	BOOST_CHECK_EQUAL(result.size(), 238u);
	std::string expectedPrefix = "a";
	for (int i = 0; i < 110; i++)
	{
		expectedPrefix += "é";
	}
	BOOST_CHECK_EQUAL(result.substr(0, 221), expectedPrefix);
	BOOST_CHECK(result.ends_with(".mkv"));
}

BOOST_AUTO_TEST_CASE(SanitizeRelativePathLimitsEachSegmentTest)
{
	const std::string input = std::string(300, 'x') + "/" + std::string(300, 'y') + ".mkv";
	const std::string expected = std::string(226, 'x') + "-0d4e2ca9e9cb" + PATH_SEPARATOR +
		std::string(222, 'y') + "-82c25194a6d9.mkv";
	BOOST_CHECK_EQUAL(FileSystem::SanitizeRelativePath(input), expected);
}

BOOST_AUTO_TEST_CASE(SanitizePathSegmentLengthBoundaryTest)
{
	// 240 bytes is the last length that passes through untouched.
	const std::string atLimit(240, 'a');
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment(atLimit), atLimit);

	// One byte more gets a hash suffix and is cut back to 240.
	const std::string overLimit(241, 'a');
	const std::string limited = FileSystem::SanitizePathSegment(overLimit);
	BOOST_CHECK_EQUAL(limited.size(), 239u);
	BOOST_CHECK(limited.starts_with(std::string(226, 'a')));
	BOOST_CHECK(limited != overLimit);
}

BOOST_AUTO_TEST_CASE(SanitizePathSegmentPrefixedReservedNameStaysWithinLimit)
{
	// The Windows device prefix adds one byte, so a 240-byte reserved name must be cut to fit.
	const std::string name = "CON." + std::string(236, 'a');
	BOOST_REQUIRE_EQUAL(name.size(), 240u);
	const std::string result = FileSystem::SanitizePathSegment(name);
	BOOST_CHECK_LE(result.size(), 240u);
	BOOST_CHECK(result.starts_with("_CON."));
	BOOST_CHECK(result != "_" + name);
}

BOOST_AUTO_TEST_CASE(SanitizePathSegmentCleansLongPrefixAndExtension)
{
	// Illegal chars survive in neither the kept prefix nor the extension.
	const std::string name = std::string(150, 'a') + ":" + std::string(150, 'b') + ".t?t";
	const std::string result = FileSystem::SanitizePathSegment(name);
	BOOST_CHECK_EQUAL(result.size(), 239u);
	BOOST_CHECK(result.find(':') == std::string::npos);
	BOOST_CHECK(result.find('?') == std::string::npos);
	BOOST_CHECK(result.ends_with(".t_t"));
}

BOOST_AUTO_TEST_CASE(SanitizePathSegmentRejectsDotOnlyLongName)
{
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment(std::string(300, '.')), "");
}

BOOST_AUTO_TEST_CASE(SanitizePathSegmentCapsUntrustedInput)
{
	// Bytes past the input bound must not change the result, so a huge name costs bounded work.
	const std::string head(1024, 'a');
	const std::string huge = head + std::string(1 << 20, 'b') + ".mkv";
	const std::string result = FileSystem::SanitizePathSegment(huge);
	BOOST_CHECK_LE(result.size(), 240u);
	BOOST_CHECK_EQUAL(result, FileSystem::SanitizePathSegment(head));
}

BOOST_AUTO_TEST_CASE(SanitizePathSegmentCapsAtUtf8Boundary)
{
	// The bound falls inside "é" (two bytes), so the whole character is dropped, not half of it.
	const std::string head(1023, 'a');
	const std::string name = head + "\xC3\xA9" + std::string(100, 'b');
	BOOST_CHECK_EQUAL(FileSystem::SanitizePathSegment(name), FileSystem::SanitizePathSegment(head));
}

BOOST_AUTO_TEST_SUITE_END()
