/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2025 Denis <denis@nzbget.com>
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

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <fstream>
#include "Validators.h"

using namespace SystemHealth;
namespace fs = boost::filesystem;

struct TempDirFixture
{
	fs::path tempPath;

	TempDirFixture()
	{
		tempPath = fs::temp_directory_path() / fs::unique_path("test_syshealth_%%%%");
		fs::create_directories(tempPath);
	}

	~TempDirFixture()
	{
		boost::system::error_code ec;
		fs::remove_all(tempPath, ec);
	}
};

BOOST_AUTO_TEST_SUITE(GeneralValidatorsSuite)

BOOST_AUTO_TEST_CASE(TestRequiredOption)
{
	Status s1 = RequiredOption("Username", "admin");
	BOOST_CHECK(s1.IsOk());

	Status s2 = RequiredOption("Username", "");
	BOOST_CHECK(s2.IsError());
	BOOST_CHECK(s2.GetMessage().find("is required") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(TestRequiredPathOption)
{
	Status s1 = RequiredPathOption("OutputDir", "/tmp");
	BOOST_CHECK(s1.IsOk());

	Status s2 = RequiredPathOption("OutputDir", "");
	BOOST_CHECK(s2.IsError());
}

BOOST_AUTO_TEST_CASE(TestCheckPassword)
{
	Status s1 = CheckPassword("");
	BOOST_CHECK(s1.IsWarning());
	BOOST_CHECK(s1.GetMessage().find("empty") != std::string::npos);

	Status s2 = CheckPassword("12345");
	BOOST_CHECK(s2.IsInfo());
	BOOST_CHECK(s2.GetMessage().find("too short") != std::string::npos);

	Status s3 = CheckPassword("correcthorsebatterystaple");
	BOOST_CHECK(s3.IsOk());
}

BOOST_AUTO_TEST_CASE(TestCheckPositiveNum)
{
	BOOST_CHECK(CheckPositiveNum("Port", 8080).IsOk());
	BOOST_CHECK(CheckPositiveNum("Zero", 0).IsOk());

	Status s = CheckPositiveNum("Limit", -5);
	BOOST_CHECK(s.IsError());
	BOOST_CHECK(s.GetMessage().find("negative") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(TestUniquePath)
{
	fs::path p1 = "/data/downloads";
	fs::path p2 = "/data/movies";
	fs::path duplicate = "/data/downloads";

	std::vector<std::pair<std::string_view, const fs::path&>> others = {{"DownloadDir", p1},
																		{"MovieDir", p2}};

	BOOST_CHECK(UniquePath("MusicDir", "/data/music", others).IsOk());

	BOOST_CHECK(UniquePath("EmptyDir", "", others).IsOk());

	Status s = UniquePath("InterimDir", duplicate, others);
	BOOST_CHECK(s.IsWarning());
	BOOST_CHECK(s.GetMessage().find("'InterimDir' and 'DownloadDir' are identical") !=
				std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(NetworkValidatorsSuite)

BOOST_AUTO_TEST_CASE(TestValidHostname)
{
	using namespace SystemHealth::Network;

	BOOST_CHECK(ValidHostname("localhost").IsOk());
	BOOST_CHECK(ValidHostname("google.com").IsOk());
	BOOST_CHECK(ValidHostname("my-server_1").IsOk());

	// Errors
	BOOST_CHECK(ValidHostname("").IsError());
	BOOST_CHECK(ValidHostname("invalid char!").IsError());

	std::string longHost(300, 'a');
	BOOST_CHECK(ValidHostname(longHost).IsError());
}

BOOST_AUTO_TEST_CASE(TestValidPort)
{
	using namespace SystemHealth::Network;

	BOOST_CHECK(ValidPort(80).IsOk());
	BOOST_CHECK(ValidPort(65535).IsOk());

	BOOST_CHECK(ValidPort(0).IsError());
	BOOST_CHECK(ValidPort(-1).IsError());
	BOOST_CHECK(ValidPort(70000).IsError());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(FilesystemValidatorsSuite, TempDirFixture)

BOOST_AUTO_TEST_CASE(TestFileExists)
{
	fs::path f = tempPath / "testfile.txt";

	Status s1 = SystemHealth::File::Exists(f);
	BOOST_CHECK(s1.IsError());
	BOOST_CHECK(s1.GetMessage().find("doesn't exist") != std::string::npos);

	{
		std::ofstream(f.c_str()) << "content";
	}
	BOOST_CHECK(SystemHealth::File::Exists(f).IsOk());

	fs::path d = tempPath / "subdir";
	fs::create_directory(d);
	Status s2 = SystemHealth::File::Exists(d);
	BOOST_CHECK(s2.IsError());
	BOOST_CHECK(s2.GetMessage().find("not a regular file") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(TestFileReadable)
{
	fs::path f = tempPath / "readable.txt";

	{
		std::ofstream(f.c_str()) << "data";
	}
	BOOST_CHECK(SystemHealth::File::Readable(f).IsOk());
}

BOOST_AUTO_TEST_CASE(TestFileWritable)
{
	fs::path f = tempPath / "writable.txt";

	BOOST_CHECK(SystemHealth::File::Writable(f).IsOk());

	fs::path d = tempPath / "writedir";
	fs::create_directory(d);

	Status s = SystemHealth::File::Writable(d);
	BOOST_CHECK(s.IsError());
}

BOOST_AUTO_TEST_CASE(TestFileExecutable)
{
#ifdef _WIN32
	fs::path exe = tempPath / "program.exe";
	fs::path bat = tempPath / "script.bat";
	fs::path txt = tempPath / "readme.txt";
	fs::path noext = tempPath / "binary";

	BOOST_CHECK(SystemHealth::File::Executable(exe).IsOk());
	BOOST_CHECK(SystemHealth::File::Executable(bat).IsOk());

	Status s1 = SystemHealth::File::Executable(txt);
	BOOST_CHECK(s1.IsError());
	BOOST_CHECK(s1.GetMessage().find("invalid extension") != std::string::npos);

	Status s2 = SystemHealth::File::Executable(noext);
	BOOST_CHECK(s2.IsError());
	BOOST_CHECK(s2.GetMessage().find("missing file extension") != std::string::npos);

#else
	fs::path script = tempPath / "script.sh";
	{
		std::ofstream(script.c_str()) << "#!/bin/bash";
	}

	Status s = SystemHealth::File::Executable(script);
	BOOST_CHECK(s.IsError());

	fs::permissions(script, fs::add_perms | fs::owner_exe);
	BOOST_CHECK(SystemHealth::File::Executable(script).IsOk());
#endif
}

BOOST_AUTO_TEST_CASE(TestDirectoryExists)
{
	fs::path d = tempPath / "mydir";

	BOOST_CHECK(SystemHealth::Directory::Exists(d).IsError());

	fs::create_directory(d);
	BOOST_CHECK(SystemHealth::Directory::Exists(d).IsOk());

	fs::path f = tempPath / "fake_dir_file";
	{
		std::ofstream(f.c_str()) << "x";
	}
	Status s = SystemHealth::Directory::Exists(f);
	BOOST_CHECK(s.IsError());
	BOOST_CHECK(s.GetMessage().find("not a directory") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(TestDirectoryWritable)
{
	fs::path d = tempPath / "writedir";
	fs::create_directory(d);

	BOOST_CHECK(SystemHealth::Directory::Writable(d).IsOk());
	BOOST_CHECK(!fs::exists(d / "nzbget_write_test.tmp"));

#ifndef _WIN32
	fs::permissions(d, fs::remove_perms | fs::owner_write);
	BOOST_CHECK(SystemHealth::Directory::Writable(d).IsError());
	fs::permissions(d, fs::add_perms | fs::owner_write);
#endif
}

BOOST_AUTO_TEST_CASE(TestDirectoryReadable)
{
	fs::path d = tempPath / "readdir";
	fs::create_directory(d);

	BOOST_CHECK(SystemHealth::Directory::Readable(d).IsOk());

#ifndef _WIN32
	fs::permissions(d, fs::remove_perms | fs::owner_read);
	BOOST_CHECK(SystemHealth::Directory::Readable(d).IsError());
	fs::permissions(d, fs::add_perms | fs::owner_read);
#endif
}

BOOST_AUTO_TEST_SUITE_END()