/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"

#include "boost/test/unit_test.hpp"

#include <fstream>
#include "HealthCheck.h"

namespace fs = boost::filesystem;
using namespace HealthCheck::Checks;

struct ChecksFixture 
{
	fs::path m_baseTempdir;
	fs::path m_writableDir;
	fs::path m_readonlyDir;
	fs::path m_nonExistentPath;

	fs::path m_writableFile;
	fs::path m_readonlyFile;
	fs::path m_executableFile;

	ChecksFixture()
	 {
		m_baseTempdir = fs::temp_directory_path() / fs::unique_path();
		fs::create_directory(m_baseTempdir);

		m_writableDir = m_baseTempdir / "writableDir";
		fs::create_directory(m_writableDir);

		m_readonlyDir = m_baseTempdir / "readonlyDir";
		fs::create_directory(m_readonlyDir);

		m_nonExistentPath = m_baseTempdir / "does_not_exist";

		m_writableFile = m_writableDir / "writable.txt";
		std::ofstream(m_writableFile.string()) << "content";

		m_readonlyFile = m_writableDir / "readonly.txt";
		std::ofstream(m_readonlyFile.string()) << "content";

		m_executableFile = m_writableDir / "executable.sh";
		std::ofstream(m_executableFile.string()) << "#!/bin/sh\necho hello";

		fs::permissions(m_readonlyDir, fs::perms::owner_read);
		fs::permissions(m_readonlyFile, fs::perms::owner_read);
		fs::permissions(m_executableFile, fs::perms::owner_exe);
	}
	~ChecksFixture() 
	{
		fs::permissions(m_readonlyDir, fs::perms::owner_all);
		fs::remove_all(m_baseTempdir);
	}
};

BOOST_AUTO_TEST_CASE(CheckRequiredOptionTest)
{
	const std::string optName = "OptName";
	{
		const auto check = CheckRequiredOption(optName, "");
		BOOST_CHECK(check.IsError());
		BOOST_CHECK_EQUAL(check.GetMessage(), optName + " is required and cannot be empty");
	}

	{
		const auto check = CheckRequiredOption(optName, "some_value");
		BOOST_CHECK(check.IsOk());
		BOOST_CHECK_EQUAL(check.GetMessage(), "");
	}
}

BOOST_AUTO_TEST_CASE(CheckValueUniqueTest)
{
	const std::string optName = "OptName";
	const std::string optValue = "/downloads";
	const std::string otherOptName = "OtherOptName";
	const std::string otherOptName2 = "OtherOptName2";
	
	{
		const std::vector<std::pair<std::string_view, std::string_view>> otherOptions{
			{ otherOptName, "/downloads/dest" },
			{ otherOptName2, optValue }
		};

		const auto check = CheckValueUnique(optName, optValue, otherOptions);
		BOOST_CHECK(check.IsWarning());
		BOOST_CHECK_EQUAL(check.GetMessage(), optName + " and " + otherOptName2 + " are the same that can lead to unexpected behavior");
	}

	{
		const std::vector<std::pair<std::string_view, std::string_view>> otherOptions{
			{ otherOptName, "/downloads/dest" },
			{ otherOptName2, "/downloads/inter" }
		};

		const auto check = CheckValueUnique(optName, optValue, otherOptions);
		BOOST_CHECK(check.IsOk());
		BOOST_CHECK_EQUAL(check.GetMessage(), "");
	}
}

BOOST_AUTO_TEST_SUITE(FileSystemChecks)

BOOST_FIXTURE_TEST_CASE(CheckRequiredDirTest, ChecksFixture)
{
	const std::string optName = "OptName";
	{
		const auto check = CheckRequiredDir(optName, m_nonExistentPath.string());
		BOOST_CHECK(check.IsError());
		BOOST_CHECK_EQUAL(check.GetMessage(), optName + " doesn't exist");
	}

	{
		const auto check = CheckRequiredDir(optName, m_readonlyFile.string());
		BOOST_CHECK(check.IsError());
		BOOST_CHECK_EQUAL(check.GetMessage(), optName + " must be directory, not a file");
	}

	{
		const auto check = CheckRequiredDir(optName, m_readonlyDir.string());
		BOOST_CHECK(check.IsOk());
		BOOST_CHECK_EQUAL(check.GetMessage(), "");
	}
}

BOOST_FIXTURE_TEST_CASE(CheckOptionalDirTest, ChecksFixture)
{
	const std::string optName = "OptName";
	{
		const auto check = CheckOptionalDir(optName, m_nonExistentPath.string());
		BOOST_CHECK(check.IsWarning());
		BOOST_CHECK_EQUAL(check.GetMessage(), optName + " doesn't exist");
	}

	{
		const auto check = CheckOptionalDir(optName, m_readonlyFile.string());
		BOOST_CHECK(check.IsError());
		BOOST_CHECK_EQUAL(check.GetMessage(), optName + " is not a directory");
	}

	{
		const auto check = CheckOptionalDir(optName, m_readonlyDir.string());
		BOOST_CHECK(check.IsOk());
		BOOST_CHECK_EQUAL(check.GetMessage(), "");
	}
}

// BOOST_FIXTURE_TEST_CASE(DirectoryIsReadableSpecTest, SpecsFixture)
// {
// 	// Directory::ReadableSpec spec;

// 	// BOOST_CHECK(spec.IsSatisfiedBy(m_writableDir.string()));
// 	// BOOST_CHECK(spec.IsSatisfiedBy(m_readonlyDir.string()));
// 	// BOOST_CHECK(!spec.IsSatisfiedBy(m_nonExistentPath.string()));
// }

// BOOST_FIXTURE_TEST_CASE(DirectoryIsWritableSpecTest, SpecsFixture)
// {
// 	// Directory::WritableSpec spec;

// 	// BOOST_CHECK(spec.IsSatisfiedBy(m_writableDir.string()));
// 	// BOOST_CHECK(!spec.IsSatisfiedBy(m_readonlyDir.string()));
// 	// BOOST_CHECK(!spec.IsSatisfiedBy(m_nonExistentPath.string()));
// }

// BOOST_FIXTURE_TEST_CASE(FileIsRegularSpecTest, SpecsFixture)
// {
// 	// File::ExistsSpec spec;

// 	// BOOST_CHECK(spec.IsSatisfiedBy(m_writableFile.string()));
// 	// BOOST_CHECK(spec.IsSatisfiedBy(m_readonlyFile.string()));
// 	// BOOST_CHECK(!spec.IsSatisfiedBy(m_writableDir.string()));
// 	// BOOST_CHECK(!spec.IsSatisfiedBy(m_nonExistentPath.string()));
// }

// BOOST_FIXTURE_TEST_CASE(FileIsWritableSpecTest, SpecsFixture)
// {
// 	// File::WritableSpec spec;

// 	// BOOST_CHECK(spec.IsSatisfiedBy(m_writableFile.string()));
// 	// BOOST_CHECK(!spec.IsSatisfiedBy(m_readonlyFile.string()));
// }

// BOOST_FIXTURE_TEST_CASE(FileIsExecutableSpecTest, SpecsFixture)
// {
// 	// File::ExecutableSpec spec;

// 	// BOOST_CHECK(spec.IsSatisfiedBy(m_executableFile.string()));
// 	// BOOST_CHECK(!spec.IsSatisfiedBy(m_writableFile.string()));
// }

BOOST_AUTO_TEST_SUITE_END()
