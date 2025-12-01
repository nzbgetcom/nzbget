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

#include "Options.h"
#include "PathsValidator.h"

namespace fs = boost::filesystem;
using namespace SystemHealth;
using namespace SystemHealth::Paths;

struct PathsValidatorFixture
{
	fs::path m_tempDir;
	fs::path m_mainDir;
	fs::path m_destDir;
	fs::path m_interDir;
	fs::path m_nzbDir;
	fs::path m_queueDir;
	fs::path m_webDir;
	fs::path m_tempDirPath;
	fs::path m_scriptDir;
	fs::path m_configTemplate;
	fs::path m_logFile;
	fs::path m_certStoreFile;
	fs::path m_certStoreDir;
	fs::path m_lockFile;
	fs::path m_readonlyDir;
	fs::path m_readonlyFile;

	Options::CmdOptList m_cmdOpts;
	std::unique_ptr<Options> m_options;
	std::unique_ptr<SystemHealth::Paths::PathsValidator> m_validator;

	PathsValidatorFixture()
	{
		m_tempDir = fs::current_path() / fs::unique_path();
		fs::create_directory(m_tempDir);

		m_mainDir = m_tempDir / "main";
		m_destDir = m_tempDir / "dest";
		m_interDir = m_tempDir / "inter";
		m_nzbDir = m_tempDir / "nzb";
		m_queueDir = m_tempDir / "queue";
		m_webDir = m_tempDir / "web";
		m_tempDirPath = m_tempDir / "temp";
		m_scriptDir = m_tempDir / "scripts";
		m_configTemplate = m_tempDir / "nzbget.conf.template";
		m_logFile = m_tempDir / "nzbget.log";
		m_certStoreFile = m_tempDir / "certstore.pem";
		m_certStoreDir = m_tempDir / "certstore";
		m_lockFile = m_tempDir / "nzbget.lock";
		m_readonlyDir = m_tempDir / "readonly";
		m_readonlyFile = m_tempDir / "readonly.txt";

		fs::create_directory(m_mainDir);
		fs::create_directory(m_destDir);
		fs::create_directory(m_interDir);
		fs::create_directory(m_nzbDir);
		fs::create_directory(m_queueDir);
		fs::create_directory(m_webDir);
		fs::create_directory(m_tempDirPath);
		fs::create_directory(m_scriptDir);
		fs::create_directory(m_readonlyDir);
		fs::create_directory(m_certStoreDir);

		std::ofstream(m_configTemplate.string()) << "# Config template";
		std::ofstream(m_logFile.string()) << "# Log file";
		std::ofstream(m_readonlyFile.string()) << "# Readonly file";
		std::ofstream(m_certStoreFile.string()) << "# Cert file";

		// Set readonly permissions
		fs::permissions(m_readonlyDir, fs::perms::owner_read);
		fs::permissions(m_readonlyFile, fs::perms::owner_read);

		// Setup command line options
		auto mainDirOpt = "MainDir=" + m_mainDir.string();
		m_cmdOpts.push_back(mainDirOpt.c_str());
		auto destDirOpt = "DestDir=" + m_destDir.string();
		m_cmdOpts.push_back(destDirOpt.c_str());
		auto interDirOpt = "InterDir=" + m_interDir.string();
		m_cmdOpts.push_back(interDirOpt.c_str());
		auto nzbDirOpt = "NzbDir=" + m_nzbDir.string();
		m_cmdOpts.push_back(nzbDirOpt.c_str());
		auto queueDirOpt = "QueueDir=" + m_queueDir.string();
		m_cmdOpts.push_back(queueDirOpt.c_str());
		auto webDirOpt = "WebDir=" + m_webDir.string();
		m_cmdOpts.push_back(webDirOpt.c_str());
		auto tempDirOpt = "TempDir=" + m_tempDirPath.string();
		m_cmdOpts.push_back(tempDirOpt.c_str());
		auto scriptDirOpt = "ScriptDir=" + m_scriptDir.string();
		m_cmdOpts.push_back(scriptDirOpt.c_str());
		auto configTemplateOpt = "ConfigTemplate=" + m_configTemplate.string();
		m_cmdOpts.push_back(configTemplateOpt.c_str());
		auto logFileOpt = "LogFile=" + m_logFile.string();
		m_cmdOpts.push_back(logFileOpt.c_str());
		auto certStoreOpt = "CertStore=" + m_certStoreFile.string();
		m_cmdOpts.push_back(certStoreOpt.c_str());
		auto lockFileOpt = "LockFile=" + m_lockFile.string();
		m_cmdOpts.push_back(lockFileOpt.c_str());
		m_cmdOpts.push_back("WriteLog=append");
		m_cmdOpts.push_back("CertCheck=yes");

		m_options = std::make_unique<Options>(&m_cmdOpts, nullptr);
		m_validator = std::make_unique<SystemHealth::Paths::PathsValidator>(*m_options);
	}

	~PathsValidatorFixture()
	{
		fs::permissions(m_readonlyDir, fs::perms::owner_all);
		fs::permissions(m_readonlyFile, fs::perms::owner_all);
		fs::remove_all(m_tempDir);
	}
};

BOOST_FIXTURE_TEST_SUITE(PathsValidatorTests, PathsValidatorFixture)

BOOST_AUTO_TEST_CASE(TestGetName) { BOOST_CHECK_EQUAL(m_validator->GetName(), "Paths"); }

static const SystemHealth::OptionStatus* FindOption(const SystemHealth::SectionReport& report,
													std::string_view optName)
{
	for (const auto& opt : report.options)
	{
		if (opt.name == optName) return &opt;
	}
	return nullptr;
}

BOOST_AUTO_TEST_CASE(TestRunAllChecks)
{
	auto report = m_validator->Validate();
	BOOST_CHECK(!report.options.empty());

	// Check that all expected options are present
	BOOST_CHECK(FindOption(report, Options::MAINDIR));
	BOOST_CHECK(FindOption(report, Options::DESTDIR));
	BOOST_CHECK(FindOption(report, Options::INTERDIR));
	BOOST_CHECK(FindOption(report, Options::NZBDIR));
	BOOST_CHECK(FindOption(report, Options::QUEUEDIR));
	BOOST_CHECK(FindOption(report, Options::WEBDIR));
	BOOST_CHECK(FindOption(report, Options::TEMPDIR));
	BOOST_CHECK(FindOption(report, Options::SCRIPTDIR));
	BOOST_CHECK(FindOption(report, Options::CONFIGTEMPLATE));
	BOOST_CHECK(FindOption(report, Options::LOGFILE));
	BOOST_CHECK(FindOption(report, Options::CERTSTORE));
	BOOST_CHECK(FindOption(report, Options::REQUIREDDIR));
#ifndef _WIN32
	BOOST_CHECK(FindOption(report, Options::LOCKFILE));
#endif
}

BOOST_AUTO_TEST_CASE(TestCheckMainDir)
{
	auto report = m_validator->Validate();
	auto opt = FindOption(report, Options::MAINDIR);
	BOOST_REQUIRE(opt);
	BOOST_CHECK(opt->status.IsOk());
}

BOOST_AUTO_TEST_CASE(TestCheckMainDir_Empty)
{
	Options::CmdOptList emptyOpts;
	emptyOpts.push_back("MainDir=");
	Options emptyOptions(&emptyOpts, nullptr);
	SystemHealth::Paths::PathsValidator emptyValidator(emptyOptions);

	auto report = emptyValidator.Validate();
	auto opt = FindOption(report, Options::MAINDIR);
	BOOST_REQUIRE(opt);
	BOOST_CHECK(opt->status.IsError());
	BOOST_CHECK_EQUAL(opt->status.GetMessage(), "MainDir is required and cannot be empty.");
}

BOOST_AUTO_TEST_SUITE_END()
