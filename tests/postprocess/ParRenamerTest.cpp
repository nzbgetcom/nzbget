/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2024 Denis <denis@nzbget.com>
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
#include "FileSystem.h"
#include "TestUtil.h"

static std::string currDir = FileSystem::GetCurrentDirectory().Str();
static std::string testDataDir = currDir + PATH_SEPARATOR + "parchecker";

class ParRenamerMock : public ParRenamer
{
public:
	ParRenamerMock(std::string workingDir);
	void Execute();
	~ParRenamerMock()
	{
		CString errmsg;
		BOOST_CHECK(FileSystem::DeleteDirectoryWithContent(m_workingDir.c_str(), errmsg));
	}
private:
	std::string m_workingDir;
};

ParRenamerMock::ParRenamerMock(std::string workingDir) : m_workingDir(std::move(workingDir))
{
	SetDestDir(m_workingDir.c_str());
	BOOST_REQUIRE(FileSystem::CreateDirectory(m_workingDir.c_str()));
	TestUtil::CopyAllFiles(m_workingDir.c_str(), testDataDir.c_str());
}

void ParRenamerMock::Execute()
{
	TestUtil::DisableCout();
	ParRenamer::Execute();
	TestUtil::EnableCout();
}

BOOST_AUTO_TEST_CASE(RenameNotNeededTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, nullptr);

	ParRenamerMock parRenamer(currDir + PATH_SEPARATOR + "RenameNotNeededTest");
	parRenamer.Execute();

	BOOST_CHECK(parRenamer.GetRenamedCount() == 0);
}

BOOST_AUTO_TEST_CASE(RenameSuccessfulTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, nullptr);

	std::string workingDir = currDir + PATH_SEPARATOR + "RenameSuccessfulTest";
	std::string testFile = workingDir + "/testfile.dat";
	std::string renamedTestFile = workingDir + "/123456";

	ParRenamerMock parRenamer(workingDir);

	BOOST_CHECK(FileSystem::MoveFile(testFile.c_str(), renamedTestFile.c_str()));

	parRenamer.Execute();

	BOOST_CHECK(parRenamer.GetRenamedCount() == 1);
	BOOST_CHECK(parRenamer.HasMissedFiles() == false);
}

BOOST_AUTO_TEST_CASE(DetectingMissingTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, nullptr);

	std::string workingDir = currDir + PATH_SEPARATOR + "DetectingMissingTest";
	std::string testFile = workingDir + PATH_SEPARATOR + "testfile.dat";
	std::string testFileNfo = workingDir + PATH_SEPARATOR + "testfile.nfo";
	std::string renamedTestFile = workingDir + PATH_SEPARATOR + "123456";

	ParRenamerMock parRenamer(workingDir);

	BOOST_CHECK(FileSystem::MoveFile(testFile.c_str(), renamedTestFile.c_str()));
	parRenamer.SetDetectMissing(true);
	BOOST_CHECK(FileSystem::DeleteFile(testFileNfo.c_str()));

	parRenamer.Execute();

	BOOST_CHECK(parRenamer.GetRenamedCount() == 1);
	BOOST_CHECK(parRenamer.HasMissedFiles());
}

BOOST_AUTO_TEST_CASE(RenameDupeParTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, nullptr);

	std::string workingDir = currDir + PATH_SEPARATOR + "RenameDupeParTest";
	std::string testFile = workingDir + PATH_SEPARATOR + "testfile.dat";
	std::string renamedTestFile = workingDir + PATH_SEPARATOR + "123456";
	std::string testFilePar = workingDir + PATH_SEPARATOR + "testfile.vol00+1.PAR2";
	std::string renamedTestFilePar = workingDir + PATH_SEPARATOR + "testfil2.par2";
	
	ParRenamerMock parRenamer(workingDir);

	BOOST_CHECK(FileSystem::MoveFile(testFile.c_str(), renamedTestFile.c_str()));\
	BOOST_CHECK(FileSystem::MoveFile(testFilePar.c_str(), renamedTestFilePar.c_str()));

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

	std::string workingDir = currDir + PATH_SEPARATOR + "NoParExtensionTest";
	std::string testFilePar = workingDir + PATH_SEPARATOR + "testfile.par2";
	std::string renamedTestFilePar = workingDir + PATH_SEPARATOR + "testfile";

	ParRenamerMock parRenamer(workingDir);
	BOOST_CHECK(FileSystem::MoveFile(testFilePar.c_str(), renamedTestFilePar.c_str()));

	parRenamer.SetDetectMissing(true);
	parRenamer.Execute();

	BOOST_CHECK(parRenamer.GetRenamedCount() == 4);
	BOOST_CHECK(parRenamer.HasMissedFiles() == false);
}
