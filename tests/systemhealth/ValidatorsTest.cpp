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

BOOST_AUTO_TEST_SUITE(GeneralValidators)

BOOST_AUTO_TEST_CASE(TestRequiredOption)
{
    Status s1 = RequiredOption("User", "admin");
    BOOST_CHECK(s1.IsOk());

    Status s2 = RequiredOption("User", "");
   	BOOST_CHECK(s2.IsError());
    BOOST_CHECK(s2.GetMessage().find("is required") != std::string::npos);
}

// BOOST_AUTO_TEST_CASE(TestCheckPassword)
// {
//     Status s1 = CheckPassword("");
//     BOOST_CHECK_EQUAL(s1.GetSeverity(), Severity::Warning);

//     Status s2 = CheckPassword("12345");
//     BOOST_CHECK_EQUAL(s2.GetSeverity(), Severity::Info);

//     Status s3 = CheckPassword("securePassword123");
//     BOOST_CHECK(s3.IsOk());
// }

// BOOST_AUTO_TEST_CASE(TestCheckPositiveNum)
// {
//     BOOST_CHECK(CheckPositiveNum("Port", 8080).IsOk());
    
//     Status s = CheckPositiveNum("Port", -1);
//     BOOST_CHECK_EQUAL(s.GetSeverity(), Severity::Error);
// }

// BOOST_AUTO_TEST_CASE(TestUniquePath)
// {
//     fs::path p1 = "/tmp/data";
//     fs::path p2 = "/tmp/other";
    
//     std::vector<std::pair<std::string_view, const fs::path&>> paths = {
//         {"MainDir", p1},
//         {"OtherDir", p2}
//     };

//     BOOST_CHECK(UniquePath("DestDir", "/tmp/unique", paths).IsOk());
//     BOOST_CHECK(UniquePath("DestDir", "", paths).IsOk());

//     Status s = UniquePath("DestDir", "/tmp/data", paths);
//     BOOST_CHECK_EQUAL(s.GetSeverity(), Severity::Warning);
//     BOOST_CHECK(s.GetMessage().find("identical") != std::string::npos);
// }

// BOOST_AUTO_TEST_SUITE_END()

// BOOST_AUTO_TEST_SUITE(NetworkValidators)

// BOOST_AUTO_TEST_CASE(TestValidHostname)
// {
//     using Network::ValidHostname;

//     BOOST_CHECK(ValidHostname("localhost").IsOk());
//     BOOST_CHECK(ValidHostname("google.com").IsOk());
//     BOOST_CHECK(ValidHostname("my-server_1").IsOk());

//     BOOST_CHECK_EQUAL(ValidHostname("").GetSeverity(), Severity::Error);
//     BOOST_CHECK_EQUAL(ValidHostname("invalid char!").GetSeverity(), Severity::Error);
    
//     std::string longHost(300, 'a');
//     BOOST_CHECK_EQUAL(ValidHostname(longHost).GetSeverity(), Severity::Error);
// }

// BOOST_AUTO_TEST_CASE(TestValidPort)
// {
//     using Network::ValidPort;

//     BOOST_CHECK(ValidPort(80).IsOk());
//     BOOST_CHECK(ValidPort(65535).IsOk());

//     BOOST_CHECK_EQUAL(ValidPort(0).GetSeverity(), Severity::Error); 
//     BOOST_CHECK_EQUAL(ValidPort(70000).GetSeverity(), Severity::Error);
// }

// BOOST_AUTO_TEST_SUITE_END()

// struct TempDirFixture {
//     fs::path tempPath;
//     TempDirFixture() {
//         tempPath = fs::temp_directory_path() / fs::unique_path("test_nzbget_%%%%");
//         fs::create_directories(tempPath);
//     }
//     ~TempDirFixture() {
//         fs::remove_all(tempPath);
//     }
// };

// BOOST_FIXTURE_TEST_SUITE(FilesystemValidators, TempDirFixture)

// BOOST_AUTO_TEST_CASE(TestFileExists)
// {
//     fs::path f = tempPath / "test.txt";
    
//     BOOST_CHECK_EQUAL(File::Exists(f).GetSeverity(), Severity::Error);

//     { std::ofstream(f.c_str()) << "content"; }
//     BOOST_CHECK(File::Exists(f).IsOk());

//     fs::path d = tempPath / "subdir";
//     fs::create_directory(d);
//     Status s = File::Exists(d);
//     BOOST_CHECK_EQUAL(s.GetSeverity(), Severity::Error);
//     BOOST_CHECK(s.GetMessage().find("not a regular file") != std::string::npos);
// }

// BOOST_AUTO_TEST_CASE(TestFileWritable)
// {
//     fs::path f = tempPath / "writable.txt";
    
//     BOOST_CHECK(File::Writable(f).IsOk());
    
//     BOOST_CHECK(File::Writable(f).IsOk());

// #ifndef _WIN32
//     fs::permissions(f, fs::remove_perms);
//     BOOST_CHECK_EQUAL(File::Writable(f).GetSeverity(), Severity::Error);
//     fs::permissions(f, fs::owner_all);
// #endif
// }

// BOOST_AUTO_TEST_CASE(TestFileExecutable)
// {
// #ifdef _WIN32
//     fs::path exe = tempPath / "app.exe";
//     { std::ofstream(exe.c_str()) << "x"; }
//     BOOST_CHECK(File::Executable(exe).IsOk());

//     fs::path txt = tempPath / "doc.txt";
//     { std::ofstream(txt.c_str()) << "x"; }
//     BOOST_CHECK_EQUAL(File::Executable(txt).GetSeverity(), Severity::Error);

// #else
//     fs::path script = tempPath / "script.sh";
//     { std::ofstream(script.c_str()) << "#!/bin/bash"; }
    
//     BOOST_CHECK_EQUAL(File::Executable(script).GetSeverity(), Severity::Error);

//     fs::permissions(script, fs::add_perms | fs::owner_exe);
//     BOOST_CHECK(File::Executable(script).IsOk());
// #endif
// }

// BOOST_AUTO_TEST_CASE(TestDirExists)
// {
//     fs::path d = tempPath / "mydir";

//     BOOST_CHECK_EQUAL(Directory::Exists(d).GetSeverity(), Severity::Error);

//     fs::create_directory(d);
//     BOOST_CHECK(Directory::Exists(d).IsOk());

//     fs::path f = tempPath / "fake_dir_file";
//     { std::ofstream(f.c_str()) << "x"; }
//     Status s = Directory::Exists(f);
//     BOOST_CHECK_EQUAL(s.GetSeverity(), Severity::Error);
//     BOOST_CHECK(s.GetMessage().find("not a directory") != std::string::npos);
// }

// BOOST_AUTO_TEST_CASE(TestDirWritable)
// {
//     fs::path d = tempPath / "write_dir";
//     fs::create_directory(d);

//     BOOST_CHECK(Directory::Writable(d).IsOk());

// #ifndef _WIN32
//     fs::permissions(d, fs::remove_perms | fs::owner_write);
//     BOOST_CHECK_EQUAL(Directory::Writable(d).GetSeverity(), Severity::Error);

//     fs::permissions(d, fs::add_perms | fs::owner_write);
// #endif
// }

// BOOST_AUTO_TEST_CASE(TestDirReadable)
// {
//     fs::path d = tempPath / "read_dir";
//     fs::create_directory(d);
    
//     BOOST_CHECK(Directory::Readable(d).IsOk());

// #ifndef _WIN32
//     fs::permissions(d, fs::remove_perms | fs::owner_read);
//     BOOST_CHECK_EQUAL(Directory::Readable(d).GetSeverity(), Severity::Error);

//     fs::permissions(d, fs::add_perms | fs::owner_read);
// #endif
// }

BOOST_AUTO_TEST_SUITE_END()
