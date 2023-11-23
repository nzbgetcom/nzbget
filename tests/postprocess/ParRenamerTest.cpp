/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include <boost/test/unit_test.hpp>

#include "Options.h"
#include "ParRenamer.h"
#include "FileSystem.h"
#include "TestUtil.h"

class ParRenamerMock: public ParRenamer
{
public:
	ParRenamerMock();
	void Execute();
};

ParRenamerMock::ParRenamerMock()
{
	TestUtil::PrepareWorkingDir("parchecker");
	SetDestDir(TestUtil::WorkingDir().c_str());
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

	ParRenamerMock parRenamer;
	parRenamer.Execute();

	BOOST_CHECK(parRenamer.GetRenamedCount() == 0);
}

BOOST_AUTO_TEST_CASE(RenameSuccessfulTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, nullptr);

	ParRenamerMock parRenamer;
	FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile.dat").c_str(), (TestUtil::WorkingDir() + "/123456").c_str());
	parRenamer.Execute();

	BOOST_CHECK(parRenamer.GetRenamedCount() == 1);
	BOOST_CHECK(parRenamer.HasMissedFiles() == false);
}

BOOST_AUTO_TEST_CASE(DetectingMissingTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, nullptr);

	ParRenamerMock parRenamer;
	FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile.dat").c_str(), (TestUtil::WorkingDir() + "/123456").c_str());
	parRenamer.SetDetectMissing(true);
	BOOST_CHECK(FileSystem::DeleteFile((TestUtil::WorkingDir() + "/testfile.nfo").c_str()));
	parRenamer.Execute();

	BOOST_CHECK(parRenamer.GetRenamedCount() == 1);
	BOOST_CHECK(parRenamer.HasMissedFiles());
}

BOOST_AUTO_TEST_CASE(RenameDupeParTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRename=yes");
	Options options(&cmdOpts, nullptr);

	ParRenamerMock parRenamer;
	FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile.dat").c_str(), (TestUtil::WorkingDir() + "/123456").c_str());
	FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile.vol00+1.PAR2").c_str(), (TestUtil::WorkingDir() + "/testfil2.par2").c_str());
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

	ParRenamerMock parRenamer;
	FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile.par2").c_str(), (TestUtil::WorkingDir() + "/testfile").c_str());
	parRenamer.SetDetectMissing(true);
	parRenamer.Execute();

	BOOST_CHECK(parRenamer.GetRenamedCount() == 4);
	BOOST_CHECK(parRenamer.HasMissedFiles() == false);
}
