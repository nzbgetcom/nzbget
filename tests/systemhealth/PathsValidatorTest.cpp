/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2025-2026 Denis <denis@nzbget.com>
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

#include "Options.h"
#include "PathsValidator.h"

namespace fs = boost::filesystem;
using namespace SystemHealth;
using namespace SystemHealth::Paths;

struct StubOptions : public Options
{
	StubOptions() : Options(nullptr, nullptr) {}

	fs::path GetMainDirPath() const { return ""; }
	fs::path GetDestDirPath() const { return ""; }
	fs::path GetInterDirPath() const { return ""; }
	fs::path GetNzbDirPath() const { return ""; }
	fs::path GetQueueDirPath() const { return ""; }
	fs::path GetWebDirPath() const { return ""; }
	fs::path GetTempDirPath() const { return ""; }
	std::vector<std::string> GetScriptDirPaths() const { return {}; }
	fs::path GetConfigTemplatePath() const { return ""; }
	fs::path GetLogFilePath() const { return ""; }
	fs::path GetCertStorePath() const { return ""; }
	fs::path GetConfigFilePath() const { return ""; }
	fs::path GetLockFilePath() const { return ""; }

	EWriteLog GetWriteLog() const { return EWriteLog::wlAppend; }
	bool GetCertCheck() const { return false; }
	bool GetDaemonMode() const { return false; }
};

struct PathsFixture
{
	fs::path tempPath;
	StubOptions options;

	PathsFixture()
	{
		tempPath = fs::temp_directory_path() / fs::unique_path("nzbget_paths_test_%%%%");
		fs::create_directories(tempPath);
	}

	~PathsFixture()
	{
		boost::system::error_code ec;
		fs::remove_all(tempPath, ec);
	}
};

BOOST_FIXTURE_TEST_SUITE(PathsValidatorsSuite, PathsFixture)

BOOST_AUTO_TEST_CASE(TestMainDirValidator)
{
	MainDirValidator validator(options);
	fs::path dir = tempPath / "main";

	BOOST_CHECK(validator.Validate(dir).IsError());

	fs::create_directory(dir);
	BOOST_CHECK(validator.Validate(dir).IsOk());

	BOOST_CHECK(validator.Validate("").IsError());
}

BOOST_AUTO_TEST_CASE(TestDestDirValidator)
{
	DestDirValidator validator(options);
	fs::path dir = tempPath / "dest";
	fs::create_directory(dir);

	BOOST_CHECK(validator.Validate(dir).IsOk());
	BOOST_CHECK(validator.Validate(tempPath / "missing").IsError());
}

BOOST_AUTO_TEST_CASE(TestQueueDirValidator)
{
	QueueDirValidator validator(options);
	fs::path dir = tempPath / "queue";
	fs::create_directory(dir);

	BOOST_CHECK(validator.Validate(dir).IsOk());
}

BOOST_AUTO_TEST_CASE(TestInterDirValidator)
{
	InterDirValidator validator(options);
	fs::path dir = tempPath / "inter";

	Status s = validator.Validate("");
	BOOST_CHECK(s.IsWarning());
	BOOST_CHECK(s.GetMessage().find("not recommended") != std::string::npos);

	fs::create_directory(dir);
	BOOST_CHECK(validator.Validate(dir).IsOk());

	BOOST_CHECK(validator.Validate(tempPath / "phantom").IsError());
}

BOOST_AUTO_TEST_CASE(TestWebDirValidator)
{
	WebDirValidator validator(options);
	fs::path dir = tempPath / "web";

	BOOST_CHECK(validator.Validate("").IsOk());

	fs::create_directory(dir);
	BOOST_CHECK(validator.Validate(dir).IsOk());
	BOOST_CHECK(validator.Validate(tempPath / "noweb").IsError());
}

BOOST_AUTO_TEST_CASE(TestLogFileValidator)
{
	LogFileValidator validator(options, *g_Log);
	fs::path log = tempPath / "nzbget.log";

	Status s1 = validator.Validate("", Options::EWriteLog::wlAppend);
	BOOST_CHECK(s1.IsError());

	Status s2 = validator.Validate("", Options::EWriteLog::wlNone);
	BOOST_CHECK(s2.IsOk());
	BOOST_CHECK(s2.GetMessage().empty());

	{
		std::ofstream(log.c_str()) << "log";
	}
	BOOST_CHECK(validator.Validate(log, Options::EWriteLog::wlAppend).IsOk());

	BOOST_CHECK(validator.Validate(tempPath / "ghost.log", Options::EWriteLog::wlAppend).IsError());
}

BOOST_AUTO_TEST_CASE(TestCertStoreValidator)
{
	CertStoreValidator validator(options);
	fs::path certFile = tempPath / "cacert.pem";
	fs::path certDir = tempPath / "certs";

	BOOST_CHECK(validator.Validate("", false).IsOk());
	BOOST_CHECK(validator.Validate("", true).IsError());

	{
		std::ofstream(certFile.c_str()) << "CERT";
	}
	BOOST_CHECK(validator.Validate(certFile, true).IsOk());

	fs::create_directory(certDir);
	BOOST_CHECK(validator.Validate(certDir, true).IsOk());

	Status s = validator.Validate(tempPath / "missing.pem", true);
	BOOST_CHECK(s.IsError());
}

#ifndef _WIN32
BOOST_AUTO_TEST_CASE(TestLockFileValidator)
{
	LockFileValidator validator(options);
	fs::path lock = tempPath / "nzbget.lock";

	Status s1 = validator.Validate("", true);
	BOOST_CHECK(s1.IsWarning());
	BOOST_CHECK(s1.GetMessage().find("check for another running instance is disabled") !=
				std::string::npos);

	BOOST_CHECK(validator.Validate("", false).IsOk());
	BOOST_CHECK(validator.Validate("random/path", false).IsOk());

	{
		std::ofstream(lock.c_str()) << "pid";
	}
	BOOST_CHECK(validator.Validate(lock, true).IsOk());

	BOOST_CHECK(validator.Validate(tempPath / "nolock", true).IsError());
}
#endif

BOOST_AUTO_TEST_CASE(TestScriptDirValidator)
{
	ScriptDirValidator validator(options);
	fs::path dir = tempPath / "scripts";

	fs::create_directory(dir);
	BOOST_CHECK(validator.Validate(dir).IsOk());

	BOOST_CHECK(validator.Validate(tempPath / "missing_scripts").IsError());
}

BOOST_AUTO_TEST_CASE(TestPathsValidatorComposite)
{
	PathsValidator pathsVal(options, *g_Log);
	BOOST_CHECK_EQUAL(pathsVal.GetName(), "Paths");

	SectionReport report = pathsVal.Validate();
	BOOST_CHECK_EQUAL(report.name, "Paths");
}

BOOST_AUTO_TEST_SUITE_END()
