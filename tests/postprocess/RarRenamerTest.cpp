/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2016-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "RarRenamer.h"
#include "FileSystem.h"
#include "TestUtil.h"

static std::string currDir = FileSystem::GetCurrentDirectory().Str();
static std::string testDataDir = currDir + PATH_SEPARATOR + "rarrenamer";

class RarRenamerMock : public RarRenamer
{
public:
	RarRenamerMock(std::string workingDir);
	~RarRenamerMock()
	{
		CString errmsg;
		BOOST_CHECK(FileSystem::DeleteDirectoryWithContent(m_workingDir.c_str(), errmsg));
	}
private:
	std::string m_workingDir;
};

RarRenamerMock::RarRenamerMock(std::string workingDir) : m_workingDir(std::move(workingDir))
{
	SetDestDir(m_workingDir.c_str());
	BOOST_REQUIRE(FileSystem::CreateDirectory(m_workingDir.c_str()));
	TestUtil::CopyAllFiles(m_workingDir.c_str(), testDataDir.c_str());
}

BOOST_AUTO_TEST_CASE(RarRenameNotNeededTest)
{
	RarRenamerMock rarRenamer(currDir + PATH_SEPARATOR + "RarRenameNotNeededTest");

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 0);
}

BOOST_AUTO_TEST_CASE(RarRenameNotNeeded2Test)
{
	std::string workingDir = currDir + PATH_SEPARATOR + "RarRenameNotNeeded2Test";

	std::string testFile = workingDir + PATH_SEPARATOR + "testfile5.part02.rar";
	std::string testFile2 = workingDir + PATH_SEPARATOR + "testfile3oldnam.r00";
	std::string renamedTestFile = workingDir + PATH_SEPARATOR + "12348";
	std::string renamedTestFile2 = workingDir + PATH_SEPARATOR + "testfile3oldnamB.r00";

	RarRenamerMock rarRenamer(workingDir);

	BOOST_CHECK(FileSystem::CopyFile(testFile.c_str(), renamedTestFile.c_str()));
	BOOST_CHECK(FileSystem::CopyFile(testFile2.c_str(), renamedTestFile2.c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 0);
}

BOOST_AUTO_TEST_CASE(RarRenameSlowDataTest)
{
	std::string workingDir = currDir + PATH_SEPARATOR + "RarRenameSlowDataTest";

	std::string testFile = workingDir + PATH_SEPARATOR + "testfile3.part01.rar";
	std::string testFile2 = workingDir + PATH_SEPARATOR + "testfile3.part02.rar";
	std::string testFile3 = workingDir + PATH_SEPARATOR + "testfile3.part03.rar";
	std::string renamedTestFile = workingDir + PATH_SEPARATOR + "12348";
	std::string renamedTestFile2 = workingDir + PATH_SEPARATOR + "12342";
	std::string renamedTestFile3 = workingDir + PATH_SEPARATOR + "12346";

	RarRenamerMock rarRenamer(workingDir);

	BOOST_CHECK(FileSystem::MoveFile(testFile.c_str(), renamedTestFile.c_str()));
	BOOST_CHECK(FileSystem::MoveFile(testFile2.c_str(), renamedTestFile2.c_str()));
	BOOST_CHECK(FileSystem::MoveFile(testFile3.c_str(), renamedTestFile3.c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
}

BOOST_AUTO_TEST_CASE(RenameRar5Test)
{
	std::string workingDir = currDir + PATH_SEPARATOR + "RarRenameSlowDataTest";

	std::string testFile = workingDir + PATH_SEPARATOR + "testfile5.part01.rar";
	std::string testFile2 = workingDir + PATH_SEPARATOR + "testfile5.part02.rar";
	std::string testFile3 = workingDir + PATH_SEPARATOR + "testfile5.part03.rar";
	std::string renamedTestFile = workingDir + PATH_SEPARATOR + "12348";
	std::string renamedTestFile2 = workingDir + PATH_SEPARATOR + "12342";
	std::string renamedTestFile3 = workingDir + PATH_SEPARATOR + "12346";

	RarRenamerMock rarRenamer(workingDir);

	BOOST_CHECK(FileSystem::MoveFile(testFile.c_str(), renamedTestFile.c_str()));
	BOOST_CHECK(FileSystem::MoveFile(testFile2.c_str(), renamedTestFile2.c_str()));
	BOOST_CHECK(FileSystem::MoveFile(testFile3.c_str(), renamedTestFile3.c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
}

BOOST_AUTO_TEST_CASE(MissingPartsTest)
{
	std::string workingDir = currDir + PATH_SEPARATOR + "RarRenameSlowDataTest";

	std::string testFile = workingDir + PATH_SEPARATOR + "testfile5.part01.rar";
	std::string testFile2 = workingDir + PATH_SEPARATOR + "testfile5.part02.rar";
	std::string testFile3 = workingDir + PATH_SEPARATOR + "testfile5.part03.rar";
	std::string renamedTestFile = workingDir + PATH_SEPARATOR + "12348";
	std::string renamedTestFile2 = workingDir + PATH_SEPARATOR + "12343";

	RarRenamerMock rarRenamer(workingDir);

	BOOST_CHECK(FileSystem::MoveFile(testFile.c_str(), renamedTestFile.c_str()));
	BOOST_CHECK(FileSystem::MoveFile(testFile2.c_str(), renamedTestFile2.c_str()));
	BOOST_CHECK(FileSystem::DeleteFile(testFile3.c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 0);
}

BOOST_AUTO_TEST_CASE(RenameRar3BadNamingTest)
{
	std::string workingDir = currDir + PATH_SEPARATOR + "RenameRar3BadNamingTest";

	std::string testFile = workingDir + PATH_SEPARATOR + "testfile3.part01.rar";
	std::string testFile2 = workingDir + PATH_SEPARATOR + "testfile3.part02.rar";
	std::string renamedTestFile = workingDir + PATH_SEPARATOR + "testfile3.part04.rar";
	std::string renamedTestFile2 = workingDir + PATH_SEPARATOR + "testfile3.part01.rar";

	RarRenamerMock rarRenamer(workingDir);

	BOOST_CHECK(FileSystem::MoveFile(testFile.c_str(), renamedTestFile.c_str()));
	BOOST_CHECK(FileSystem::MoveFile(testFile2.c_str(), renamedTestFile2.c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
}

BOOST_AUTO_TEST_CASE(Rar3BadNaming2)
{
	std::string workingDir = currDir + PATH_SEPARATOR + "Rar3BadNaming2";

	std::string testFile = workingDir + PATH_SEPARATOR + "testfile3.part02.rar";
	std::string renamedTestFile = workingDir + PATH_SEPARATOR + "testfile3.part2.rar";

	RarRenamerMock rarRenamer(workingDir);

	BOOST_CHECK(FileSystem::MoveFile(testFile.c_str(), renamedTestFile.c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
}

BOOST_AUTO_TEST_CASE(RenameRar3BadNaming3Test)
{
	std::string workingDir = currDir + PATH_SEPARATOR + "RenameRar3BadNaming3Test";

	std::string testFile = workingDir + PATH_SEPARATOR + "testfile3.part02.rar";
	std::string renamedTestFile = workingDir + PATH_SEPARATOR + "testfile3-1.part02.rar";

	RarRenamerMock rarRenamer(workingDir);

	BOOST_CHECK(FileSystem::MoveFile(testFile.c_str(), renamedTestFile.c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
}

BOOST_AUTO_TEST_CASE(RenameRar3BadNaming4Test)
{
	std::string workingDir = currDir + PATH_SEPARATOR + "RenameRar3BadNaming4Test";

	std::string testFile = workingDir + PATH_SEPARATOR + "testfile3.part02.rar";
	std::string renamedTestFile = workingDir + PATH_SEPARATOR + "testfil-3.part02.rar";

	RarRenamerMock rarRenamer(workingDir);

	BOOST_CHECK(FileSystem::MoveFile(testFile.c_str(), renamedTestFile.c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
}

BOOST_AUTO_TEST_CASE(RenameRar3BadNaming5Test)
{
	std::string workingDir = currDir + PATH_SEPARATOR + "RenameRar3BadNaming5Test";

	std::string testFile = workingDir + PATH_SEPARATOR + "testfile3oldnam.rar";
	std::string renamedTestFile = workingDir + PATH_SEPARATOR + "testfile3oldnamA.rar";

	RarRenamerMock rarRenamer(workingDir);

	BOOST_CHECK(FileSystem::MoveFile(testFile.c_str(), renamedTestFile.c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 1);
}

BOOST_AUTO_TEST_CASE(RenameRar3BadNaming6Test)
{
	std::string workingDir = currDir + PATH_SEPARATOR + "RenameRar3BadNaming6Test";

	std::string testFile = workingDir + PATH_SEPARATOR + "testfile3oldnam.rar";
	std::string testFile2 = workingDir + PATH_SEPARATOR + "testfile3oldnam.r00";
	std::string testFile3 = workingDir + PATH_SEPARATOR + "testfile3oldnam.r01";
	std::string renamedTestFile = workingDir + PATH_SEPARATOR + "testfile3oldnamA.rar";
	std::string renamedTestFile2 = workingDir + PATH_SEPARATOR + "testfile3oldnamB.r00";
	std::string renamedTestFile3 = workingDir + PATH_SEPARATOR + "testfile3oldnamA.r01";

	RarRenamerMock rarRenamer(workingDir);

	BOOST_CHECK(FileSystem::MoveFile(testFile.c_str(), renamedTestFile.c_str()));
	BOOST_CHECK(FileSystem::MoveFile(testFile2.c_str(), renamedTestFile2.c_str()));
	BOOST_CHECK(FileSystem::MoveFile(testFile3.c_str(), renamedTestFile3.c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
}

BOOST_AUTO_TEST_CASE(RenameTwoSetsTest)
{
	std::string workingDir = currDir + PATH_SEPARATOR + "RenameTwoSetsTest";

	std::string testFile = workingDir + PATH_SEPARATOR + "testfile3.part01.rar";
	std::string testFile2 = workingDir + PATH_SEPARATOR + "testfile3.part02.rar";
	std::string testFile3 = workingDir + PATH_SEPARATOR + "testfile3.part03.rar";
	std::string testFile4 = workingDir + PATH_SEPARATOR + "testfile5.part01.rar";
	std::string testFile5 = workingDir + PATH_SEPARATOR + "testfile5.part02.rar";
	std::string testFile6 = workingDir + PATH_SEPARATOR + "testfile5.part03.rar";
	std::string renamedTestFile = workingDir + PATH_SEPARATOR + "12345";
	std::string renamedTestFile2 = workingDir + PATH_SEPARATOR + "12342";
	std::string renamedTestFile3 = workingDir + PATH_SEPARATOR + "12346";
	std::string renamedTestFile4 = workingDir + PATH_SEPARATOR + "12348";
	std::string renamedTestFile5 = workingDir + PATH_SEPARATOR + "12343";
	std::string renamedTestFile6 = workingDir + PATH_SEPARATOR + "12344";

	RarRenamerMock rarRenamer(workingDir);

	BOOST_CHECK(FileSystem::MoveFile(testFile.c_str(), renamedTestFile.c_str()));
	BOOST_CHECK(FileSystem::MoveFile(testFile2.c_str(), renamedTestFile2.c_str()));
	BOOST_CHECK(FileSystem::MoveFile(testFile3.c_str(), renamedTestFile3.c_str()));
	BOOST_CHECK(FileSystem::MoveFile(testFile4.c_str(), renamedTestFile4.c_str()));
	BOOST_CHECK(FileSystem::MoveFile(testFile5.c_str(), renamedTestFile5.c_str()));
	BOOST_CHECK(FileSystem::MoveFile(testFile6.c_str(), renamedTestFile6.c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 6);
}

BOOST_AUTO_TEST_CASE(RenameDuplicateTest)
{
	std::string workingDir = currDir + PATH_SEPARATOR + "RenameTwoSetsTest";

	std::string testFile = workingDir + PATH_SEPARATOR + "testfile3.part01.rar";
	std::string testFile2 = workingDir + PATH_SEPARATOR + "testfile3.part02.rar";
	std::string testFile3 = workingDir + PATH_SEPARATOR + "testfile3.part03.rar";
	std::string testFile4 = workingDir + PATH_SEPARATOR + "testfile5.part01.rar";
	std::string testFile5 = workingDir + PATH_SEPARATOR + "testfile5.part02.rar";
	std::string testFile6 = workingDir + PATH_SEPARATOR + "testfile5.part03.rar";
	std::string renamedTestFile = workingDir + PATH_SEPARATOR + "12345";
	std::string renamedTestFile2 = workingDir + PATH_SEPARATOR + "12342";
	std::string renamedTestFile3 = workingDir + PATH_SEPARATOR + "12346";
	std::string renamedTestFile4 = workingDir + PATH_SEPARATOR + "testfile3.dat.part0001.rar";

	RarRenamerMock rarRenamer(workingDir);

	BOOST_CHECK(FileSystem::MoveFile(testFile.c_str(), renamedTestFile.c_str()));
	BOOST_CHECK(FileSystem::MoveFile(testFile2.c_str(), renamedTestFile2.c_str()));
	BOOST_CHECK(FileSystem::MoveFile(testFile3.c_str(), renamedTestFile3.c_str()));
	BOOST_CHECK(FileSystem::MoveFile(testFile4.c_str(), renamedTestFile4.c_str()));
	BOOST_CHECK(FileSystem::DeleteFile(testFile5.c_str()));
	BOOST_CHECK(FileSystem::DeleteFile(testFile6.c_str()));

	rarRenamer.Execute();

	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
}

#ifndef DISABLE_TLS

// BOOST_AUTO_TEST_CASE(RenameRar5EncryptedTest)
// {
// 	std::string workingDir = currDir + PATH_SEPARATOR + "RenameRar5EncryptedTest";

// 	std::string testFile = workingDir + PATH_SEPARATOR + "testfile5encnam.part01.rar";
// 	std::string testFile2 = workingDir + PATH_SEPARATOR + "testfile5encnam.part02.rar";
// 	std::string testFile3 = workingDir + PATH_SEPARATOR + "testfile5encnam.part03.rar";
// 	std::string renamedTestFile = workingDir + PATH_SEPARATOR + "12348";
// 	std::string renamedTestFile2 = workingDir + PATH_SEPARATOR + "12343";
// 	std::string renamedTestFile3 = workingDir + PATH_SEPARATOR + "12344";

// 	RarRenamerMock rarRenamer(workingDir);
// 	rarRenamer.SetPassword("123");

// 	BOOST_CHECK(FileSystem::MoveFile(testFile.c_str(), renamedTestFile.c_str()));
// 	BOOST_CHECK(FileSystem::MoveFile(testFile2.c_str(), renamedTestFile2.c_str()));
// 	BOOST_CHECK(FileSystem::MoveFile(testFile3.c_str(), renamedTestFile3.c_str()));

// 	rarRenamer.Execute();

// 	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
// }

// BOOST_AUTO_TEST_CASE(RenameRar3EncryptedTest)
// {
// 	std::string workingDir = currDir + PATH_SEPARATOR + "RenameRar3EncryptedTest";

// 	std::string testFile = workingDir + PATH_SEPARATOR + "testfile3encnam.part01.rar";
// 	std::string testFile2 = workingDir + PATH_SEPARATOR + "testfile3encnam.part02.rar";
// 	std::string testFile3 = workingDir + PATH_SEPARATOR + "testfile3encnam.part03.rar";
// 	std::string renamedTestFile = workingDir + PATH_SEPARATOR + "12348";
// 	std::string renamedTestFile2 = workingDir + PATH_SEPARATOR + "12343";
// 	std::string renamedTestFile3 = workingDir + PATH_SEPARATOR + "12344";

// 	RarRenamerMock rarRenamer(workingDir);
// 	rarRenamer.SetPassword("123");

// 	BOOST_CHECK(FileSystem::MoveFile(testFile.c_str(), renamedTestFile.c_str()));
// 	BOOST_CHECK(FileSystem::MoveFile(testFile2.c_str(), renamedTestFile2.c_str()));
// 	BOOST_CHECK(FileSystem::MoveFile(testFile3.c_str(), renamedTestFile3.c_str()));

// 	rarRenamer.Execute();

// 	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
// }

#endif
