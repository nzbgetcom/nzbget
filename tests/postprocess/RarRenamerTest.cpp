/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2016-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "RarRenamer.h"
#include "FileSystem.h"
#include "TestUtil.h"

class RarRenamerMock: public RarRenamer
{
public:
	RarRenamerMock();
};

RarRenamerMock::RarRenamerMock()
{
	TestUtil::PrepareWorkingDir("rarrenamer");
	SetDestDir(TestUtil::WorkingDir().c_str());
}

BOOST_AUTO_TEST_CASE(RenameNotEeededTest)
{
	RarRenamerMock rarRenamer;

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 0);
}

BOOST_AUTO_TEST_CASE(RenameNotEeeded2Test)
{
	RarRenamerMock rarRenamer;

	BOOST_CHECK(FileSystem::CopyFile((TestUtil::WorkingDir() + "/testfile5.part02.rar").c_str(), (TestUtil::WorkingDir() + "/12348").c_str()));
	BOOST_CHECK(FileSystem::CopyFile((TestUtil::WorkingDir() + "/testfile3oldnam.r00").c_str(), (TestUtil::WorkingDir() + "/testfile3oldnamB.r00").c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 0);
}

BOOST_AUTO_TEST_CASE("Rar-renamer: rename rar3", "[Rar][RarRenamer][Slow][TestData]")
{
	RarRenamerMock rarRenamer;

	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part01.rar").c_str(), (TestUtil::WorkingDir() + "/12345").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part02.rar").c_str(), (TestUtil::WorkingDir() + "/12342").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part03.rar").c_str(), (TestUtil::WorkingDir() + "/12346").c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
}

BOOST_AUTO_TEST_CASE(RenameRar5Test)
{
	RarRenamerMock rarRenamer;

	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile5.part01.rar").c_str(), (TestUtil::WorkingDir() + "/12348").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile5.part02.rar").c_str(), (TestUtil::WorkingDir() + "/12343").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile5.part03.rar").c_str(), (TestUtil::WorkingDir() + "/12344").c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
}

BOOST_AUTO_TEST_CASE(MissingPartsTest)
{
	RarRenamerMock rarRenamer;

	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile5.part01.rar").c_str(), (TestUtil::WorkingDir() + "/12348").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile5.part02.rar").c_str(), (TestUtil::WorkingDir() + "/12343").c_str()));
	BOOST_CHECK(FileSystem::DeleteFile((TestUtil::WorkingDir() + "/testfile5.part03.rar").c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 0);
}

BOOST_AUTO_TEST_CASE(RenameRar3BadNamingTest)
{
	RarRenamerMock rarRenamer;

	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part01.rar").c_str(), (TestUtil::WorkingDir() + "/testfile3.part04.rar").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part02.rar").c_str(), (TestUtil::WorkingDir() + "/testfile3.part01.rar").c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
}

BOOST_AUTO_TEST_CASE(Rar3BadNaming2)
{
	RarRenamerMock rarRenamer;

	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part02.rar").c_str(), (TestUtil::WorkingDir() + "/testfile3.part2.rar").c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
}

BOOST_AUTO_TEST_CASE(RenameRar3BadNaming3Test)
{
	RarRenamerMock rarRenamer;

	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part02.rar").c_str(), (TestUtil::WorkingDir() + "/testfile3-1.part02.rar").c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
}

BOOST_AUTO_TEST_CASE(RenameRar3BadNaming4Test)
{
	RarRenamerMock rarRenamer;

	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part02.rar").c_str(), (TestUtil::WorkingDir() + "/testfil-3.part02.rar").c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
}

BOOST_AUTO_TEST_CASE(RenameRar3BadNaming5Test)
{
	RarRenamerMock rarRenamer;

	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3oldnam.rar").c_str(), (TestUtil::WorkingDir() + "/testfile3oldnamA.rar").c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 1);
}

BOOST_AUTO_TEST_CASE(RenameRar3BadNaming6Test)
{
	RarRenamerMock rarRenamer;

	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3oldnam.rar").c_str(), (TestUtil::WorkingDir() + "/testfile3oldnamA.rar").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3oldnam.r00").c_str(), (TestUtil::WorkingDir() + "/testfile3oldnamB.r00").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3oldnam.r01").c_str(), (TestUtil::WorkingDir() + "/testfile3oldnamA.r01").c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
}

BOOST_AUTO_TEST_CASE(RenameTwoSetsTest)
{
	RarRenamerMock rarRenamer;

	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part01.rar").c_str(), (TestUtil::WorkingDir() + "/12345").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part02.rar").c_str(), (TestUtil::WorkingDir() + "/12342").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part03.rar").c_str(), (TestUtil::WorkingDir() + "/12346").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile5.part01.rar").c_str(), (TestUtil::WorkingDir() + "/12348").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile5.part02.rar").c_str(), (TestUtil::WorkingDir() + "/12343").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile5.part03.rar").c_str(), (TestUtil::WorkingDir() + "/12344").c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 6);
}

BOOST_AUTO_TEST_CASE(RenameDuplicateTest)
{
	RarRenamerMock rarRenamer;

	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part01.rar").c_str(), (TestUtil::WorkingDir() + "/12345").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part02.rar").c_str(), (TestUtil::WorkingDir() + "/12342").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3.part03.rar").c_str(), (TestUtil::WorkingDir() + "/12346").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile5.part01.rar").c_str(), (TestUtil::WorkingDir() + "/testfile3.dat.part0001.rar").c_str()));
	BOOST_CHECK(FileSystem::DeleteFile((TestUtil::WorkingDir() + "/testfile5.part02.rar").c_str()));
	BOOST_CHECK(FileSystem::DeleteFile((TestUtil::WorkingDir() + "/testfile5.part03.rar").c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
}

#ifndef DISABLE_TLS

BOOST_AUTO_TEST_CASE(RenameRar5EncryptedTest)
{
	RarRenamerMock rarRenamer;
	rarRenamer.SetPassword("123");

	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile5encnam.part01.rar").c_str(), (TestUtil::WorkingDir() + "/12348").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile5encnam.part02.rar").c_str(), (TestUtil::WorkingDir() + "/12343").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile5encnam.part03.rar").c_str(), (TestUtil::WorkingDir() + "/12344").c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
}

BOOST_AUTO_TEST_CASE(RenameRar3EncryptedTest)
{
	RarRenamerMock rarRenamer;
	rarRenamer.SetPassword("123");

	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3encnam.part01.rar").c_str(), (TestUtil::WorkingDir() + "/12348").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3encnam.part02.rar").c_str(), (TestUtil::WorkingDir() + "/12343").c_str()));
	BOOST_CHECK(FileSystem::MoveFile((TestUtil::WorkingDir() + "/testfile3encnam.part03.rar").c_str(), (TestUtil::WorkingDir() + "/12344").c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
}

#endif
