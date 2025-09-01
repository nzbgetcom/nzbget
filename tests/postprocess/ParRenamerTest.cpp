/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include <string>
#include "Options.h"
#include "ParRenamer.h"

namespace fs = boost::filesystem;

const fs::path CURR_DIR = fs::current_path();
const fs::path TEST_DATA_DIR = CURR_DIR / "parchecker";
const fs::path TEST_DATA_DIR_UTF8 = CURR_DIR / "parcheckerUtf8";

class ParRenamerMock : public ParRenamer
{
public:
	ParRenamerMock(const fs::path& workingDir, const fs::path& testDataDir);
	void Execute();
	~ParRenamerMock()
	{
		fs::remove_all(m_workingDir);
	}
private:
	const fs::path& m_workingDir;
	const fs::path& m_testDataDir;
};

ParRenamerMock::ParRenamerMock(const fs::path& workingDir, const fs::path& testDataDir) 
	: m_workingDir(workingDir)
	, m_testDataDir(testDataDir)
{
	SetDestDir(m_workingDir.string().c_str());
	fs::copy(m_testDataDir, m_workingDir);
}

void ParRenamerMock::Execute()
{
	ParRenamer::Execute();
}

BOOST_AUTO_TEST_CASE(RenameNotNeededTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, nullptr);

	const fs::path workingDir = CURR_DIR / "RenameNotNeededTest";
	ParRenamerMock parRenamer(workingDir, TEST_DATA_DIR);
	parRenamer.Execute();

	BOOST_CHECK(parRenamer.GetRenamedCount() == 0);
}

BOOST_AUTO_TEST_CASE(RenameSuccessfulTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, nullptr);

	const fs::path workingDir = CURR_DIR / "RenameSuccessfulTest";
	const fs::path testFile = workingDir / "testfile.dat";
	const fs::path renamedTestFile = workingDir / "123456";
	ParRenamerMock parRenamer(workingDir, TEST_DATA_DIR);

	fs::rename(testFile, renamedTestFile);

	parRenamer.Execute();

	BOOST_CHECK(parRenamer.GetRenamedCount() == 1);
	BOOST_CHECK(parRenamer.HasMissedFiles() == false);
}

BOOST_AUTO_TEST_CASE(DetectingMissingTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, nullptr);

	const fs::path workingDir = CURR_DIR / "DetectingMissingTest";
	const fs::path testFile = workingDir / "testfile.dat";
	const fs::path testFileNfo = workingDir / "testfile.nfo";
	const fs::path renamedTestFile = workingDir / "123456";
	ParRenamerMock parRenamer(workingDir, TEST_DATA_DIR);

	fs::rename(testFile, renamedTestFile);
	parRenamer.SetDetectMissing(true);
	BOOST_CHECK(fs::remove(testFileNfo));

	parRenamer.Execute();

	BOOST_CHECK(parRenamer.GetRenamedCount() == 1);
	BOOST_CHECK(parRenamer.HasMissedFiles());
}

BOOST_AUTO_TEST_CASE(RenameDupeParTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, nullptr);

	const fs::path workingDir = CURR_DIR / "RenameDupeParTest";
	const fs::path testFile = workingDir / "testfile.dat";
	const fs::path renamedTestFile = workingDir / "123456";
	const fs::path testFilePar = workingDir / "testfile.vol00+1.PAR2";
	const fs::path renamedTestFilePar = workingDir / "testfil2.par2";
	ParRenamerMock parRenamer(workingDir, TEST_DATA_DIR);

	fs::rename(testFile, renamedTestFile);
	fs::rename(testFilePar, renamedTestFilePar);

	parRenamer.SetDetectMissing(true);
	parRenamer.Execute();

	BOOST_CHECK(parRenamer.GetRenamedCount() == 5);
	BOOST_CHECK(parRenamer.HasMissedFiles() == false);
}

BOOST_AUTO_TEST_CASE(NoParExtensionTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, nullptr);

	const fs::path workingDir = CURR_DIR / "NoParExtensionTest";
	const fs::path testFilePar = workingDir / "testfile.par2";
	const fs::path renamedTestFilePar = workingDir / "testfile";
	ParRenamerMock parRenamer(workingDir, TEST_DATA_DIR);

	fs::rename(testFilePar, renamedTestFilePar);

	parRenamer.SetDetectMissing(true);
	parRenamer.Execute();

	BOOST_CHECK(parRenamer.GetRenamedCount() == 4);
	BOOST_CHECK(parRenamer.HasMissedFiles() == false);
}

BOOST_AUTO_TEST_CASE(Utf8Par2Test)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, nullptr);

	const fs::path workingDir = CURR_DIR / "Utf8Par2Test";
	ParRenamerMock parRenamer(workingDir, TEST_DATA_DIR_UTF8);

	parRenamer.Execute();

	BOOST_CHECK_EQUAL(parRenamer.GetRenamedCount(), 1);
	BOOST_CHECK(parRenamer.HasMissedFiles() == false);
}
