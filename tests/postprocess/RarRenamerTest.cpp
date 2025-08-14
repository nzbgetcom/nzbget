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

namespace fs = boost::filesystem;

const fs::path CURR_DIR = fs::current_path();
const fs::path TEST_DATA_DIR = CURR_DIR / "rarrenamer";

class RarRenamerMock : public RarRenamer
{
public:
	RarRenamerMock(const fs::path& workingDir);
	~RarRenamerMock()
	{
		fs::remove_all(m_workingDir);
	}
private:
	const fs::path& m_workingDir;
};

RarRenamerMock::RarRenamerMock(const fs::path& workingDir) : m_workingDir(workingDir)
{
	SetDestDir(m_workingDir.c_str());
	fs::copy(TEST_DATA_DIR, m_workingDir);
}

BOOST_AUTO_TEST_CASE(RarRenameNotNeededTest)
{
	const fs::path file = CURR_DIR / "RarRenameNotNeededTest";
	RarRenamerMock rarRenamer(file);

	rarRenamer.Execute();

	BOOST_CHECK_EQUAL(rarRenamer.GetRenamedCount(), 0);
}

BOOST_AUTO_TEST_CASE(RarRenameNotNeeded2Test)
{
	const fs::path workingDir = CURR_DIR / "RarRenameNotNeeded2Test";
	const fs::path testFile = workingDir / "testfile5.part02.rar";
	const fs::path testFile2 = workingDir / "testfile3oldnam.r00";
	const fs::path renamedTestFile = workingDir / "12348";
	const fs::path renamedTestFile2 = workingDir / "testfile3oldnamB.r00";

	RarRenamerMock rarRenamer(workingDir);

	BOOST_CHECK(fs::copy_file(testFile, renamedTestFile));
	BOOST_CHECK(fs::copy_file(testFile2, renamedTestFile2));

	rarRenamer.Execute();

	BOOST_CHECK_EQUAL(rarRenamer.GetRenamedCount(), 0);
}

BOOST_AUTO_TEST_CASE(RarRenameSlowDataTest)
{
	const fs::path workingDir = CURR_DIR / "RarRenameSlowDataTest";
	const fs::path testFile = workingDir / "testfile3.part01.rar";
	const fs::path testFile2 = workingDir / "testfile3.part02.rar";
	const fs::path testFile3 = workingDir / "testfile3.part03.rar";
	const fs::path renamedTestFile = workingDir / "12348";
	const fs::path renamedTestFile2 = workingDir / "12342";
	const fs::path renamedTestFile3 = workingDir / "12346";

	RarRenamerMock rarRenamer(workingDir);

	BOOST_CHECK(fs::copy_file(testFile, renamedTestFile));
	BOOST_CHECK(fs::copy_file(testFile2, renamedTestFile2));
	BOOST_CHECK(fs::copy_file(testFile3, renamedTestFile3));

	rarRenamer.Execute();

	BOOST_CHECK_EQUAL(rarRenamer.GetRenamedCount(), 3);
}

BOOST_AUTO_TEST_CASE(RenameRar5Test)
{
	const fs::path workingDir = CURR_DIR / "RarRenameSlowDataTest";
	const fs::path testFile = workingDir / "testfile5.part01.rar";
	const fs::path testFile2 = workingDir / "testfile5.part02.rar";
	const fs::path testFile3 = workingDir / "testfile5.part03.rar";
	const fs::path renamedTestFile = workingDir / "12348";
	const fs::path renamedTestFile2 = workingDir / "12342";
	const fs::path renamedTestFile3 = workingDir / "12346";

	RarRenamerMock rarRenamer(workingDir);

	BOOST_CHECK(fs::copy_file(testFile, renamedTestFile));
	BOOST_CHECK(fs::copy_file(testFile2, renamedTestFile2));
	BOOST_CHECK(fs::copy_file(testFile3, renamedTestFile3));

	rarRenamer.Execute();

	BOOST_CHECK_EQUAL(rarRenamer.GetRenamedCount(), 3);
}

BOOST_AUTO_TEST_CASE(MissingPartsTest)
{
	const fs::path workingDir = CURR_DIR / "RarRenameSlowDataTest";
	const fs::path testFile = workingDir / "testfile5.part01.rar";
	const fs::path testFile2 = workingDir / "testfile5.part02.rar";
	const fs::path testFile3 = workingDir / "testfile5.part03.rar";
	const fs::path renamedTestFile = workingDir / "12348";
	const fs::path renamedTestFile2 = workingDir / "12343";

	RarRenamerMock rarRenamer(workingDir);

	BOOST_CHECK(fs::copy_file(testFile, renamedTestFile));
	BOOST_CHECK(fs::copy_file(testFile2, renamedTestFile2));
	BOOST_CHECK(fs::remove(testFile3));

	rarRenamer.Execute();

	BOOST_CHECK_EQUAL(rarRenamer.GetRenamedCount(), 0);
}

BOOST_AUTO_TEST_CASE(RenameRar3BadNamingTest)
{
	const fs::path workingDir = CURR_DIR / "RenameRar3BadNamingTest";
	const fs::path testFile = workingDir / "testfile3.part01.rar";
	const fs::path testFile2 = workingDir / "testfile3.part02.rar";
	const fs::path renamedTestFile = workingDir / "testfile3.part04.rar";
	const fs::path renamedTestFile2 = workingDir / "testfile3.part01.rar";

	RarRenamerMock rarRenamer(workingDir);

	fs::rename(testFile, renamedTestFile);
	fs::rename(testFile2, renamedTestFile2);

	rarRenamer.Execute();

	BOOST_CHECK_EQUAL(rarRenamer.GetRenamedCount(), 3);
}

BOOST_AUTO_TEST_CASE(Rar3BadNaming2)
{
	const fs::path workingDir = CURR_DIR / "Rar3BadNaming2";
	const fs::path testFile = workingDir / "testfile3.part02.rar";
	const fs::path renamedTestFile = workingDir / "testfile3.part2.rar";

	RarRenamerMock rarRenamer(workingDir);

	fs::rename(testFile, renamedTestFile);

	rarRenamer.Execute();

	BOOST_CHECK_EQUAL(rarRenamer.GetRenamedCount(), 3);
}

BOOST_AUTO_TEST_CASE(RenameRar3BadNaming3Test)
{
	const fs::path workingDir = CURR_DIR / "RenameRar3BadNaming3Test";
	const fs::path testFile = workingDir / "testfile3.part02.rar";
	const fs::path renamedTestFile = workingDir / "testfile3-1.part02.rar";

	RarRenamerMock rarRenamer(workingDir);

	fs::rename(testFile, renamedTestFile);

	rarRenamer.Execute();

	BOOST_CHECK_EQUAL(rarRenamer.GetRenamedCount(), 3);
}

BOOST_AUTO_TEST_CASE(RenameRar3BadNaming4Test)
{
	const fs::path workingDir = CURR_DIR / "RenameRar3BadNaming4Test";
	const fs::path testFile = workingDir / "testfile3.part02.rar";
	const fs::path renamedTestFile = workingDir / "testfil-3.part02.rar";

	RarRenamerMock rarRenamer(workingDir);

	fs::rename(testFile, renamedTestFile);

	rarRenamer.Execute();

	BOOST_CHECK_EQUAL(rarRenamer.GetRenamedCount(), 3);
}

BOOST_AUTO_TEST_CASE(RenameRar3BadNaming5Test)
{
	const fs::path workingDir = CURR_DIR / "RenameRar3BadNaming5Test";
	const fs::path testFile = workingDir / "testfile3oldnam.rar";
	const fs::path renamedTestFile = workingDir / "testfile3oldnamA.rar";

	RarRenamerMock rarRenamer(workingDir);

	fs::rename(testFile, renamedTestFile);

	rarRenamer.Execute();

	BOOST_CHECK_EQUAL(rarRenamer.GetRenamedCount(), 1);
}

BOOST_AUTO_TEST_CASE(RenameRar3BadNaming6Test)
{
	const fs::path workingDir = CURR_DIR / "RenameRar3BadNaming6Test";
	const fs::path testFile = workingDir / "testfile3oldnam.rar";
	const fs::path testFile2 = workingDir / "testfile3oldnam.r00";
	const fs::path testFile3 = workingDir / "testfile3oldnam.r01";
	const fs::path renamedTestFile = workingDir / "testfile3oldnamA.rar";
	const fs::path renamedTestFile2 = workingDir / "testfile3oldnamB.r00";
	const fs::path renamedTestFile3 = workingDir / "testfile3oldnamA.r01";

	RarRenamerMock rarRenamer(workingDir);

	fs::rename(testFile, renamedTestFile);
	fs::rename(testFile2, renamedTestFile2);
	fs::rename(testFile3, renamedTestFile3);

	rarRenamer.Execute();

	BOOST_CHECK_EQUAL(rarRenamer.GetRenamedCount(), 3);
}

BOOST_AUTO_TEST_CASE(RenameTwoSetsTest)
{
	const fs::path workingDir = CURR_DIR / "RenameTwoSetsTest";
	const fs::path testFile = workingDir / "testfile3.part01.rar";
	const fs::path testFile2 = workingDir / "testfile3.part02.rar";
	const fs::path testFile3 = workingDir / "testfile3.part03.rar";
	const fs::path testFile4 = workingDir / "testfile5.part01.rar";
	const fs::path testFile5 = workingDir / "testfile5.part02.rar";
	const fs::path testFile6 = workingDir / "testfile5.part03.rar";
	const fs::path renamedTestFile = workingDir / "12345";
	const fs::path renamedTestFile2 = workingDir / "12342";
	const fs::path renamedTestFile3 = workingDir / "12346";
	const fs::path renamedTestFile4 = workingDir / "12348";
	const fs::path renamedTestFile5 = workingDir / "12343";
	const fs::path renamedTestFile6 = workingDir / "12344";

	RarRenamerMock rarRenamer(workingDir);

	fs::rename(testFile, renamedTestFile);
	fs::rename(testFile2, renamedTestFile2);
	fs::rename(testFile3, renamedTestFile3);
	fs::rename(testFile4, renamedTestFile4);
	fs::rename(testFile5, renamedTestFile5);
	fs::rename(testFile6, renamedTestFile6);

	rarRenamer.Execute();

	BOOST_CHECK_EQUAL(rarRenamer.GetRenamedCount(), 6);
}

BOOST_AUTO_TEST_CASE(RenameDuplicateTest)
{
	const fs::path workingDir = CURR_DIR / "RenameTwoSetsTest";
	const fs::path testFile = workingDir / "testfile3.part01.rar";
	const fs::path testFile2 = workingDir / "testfile3.part02.rar";
	const fs::path testFile3 = workingDir / "testfile3.part03.rar";
	const fs::path testFile4 = workingDir / "testfile5.part01.rar";
	const fs::path testFile5 = workingDir / "testfile5.part02.rar";
	const fs::path testFile6 = workingDir / "testfile5.part03.rar";
	const fs::path renamedTestFile = workingDir / "12345";
	const fs::path renamedTestFile2 = workingDir / "12342";
	const fs::path renamedTestFile3 = workingDir / "12346";
	const fs::path renamedTestFile4 = workingDir / "testfile3.dat.part0001.rar";

	RarRenamerMock rarRenamer(workingDir);

	fs::rename(testFile, renamedTestFile);
	fs::rename(testFile2, renamedTestFile2);
	fs::rename(testFile3, renamedTestFile3);
	fs::rename(testFile4, renamedTestFile4);
	fs::remove(testFile5);
	fs::remove(testFile6);

	rarRenamer.Execute();

	BOOST_CHECK_EQUAL(rarRenamer.GetRenamedCount(), 3);
}

#ifndef DISABLE_TLS

// BOOST_AUTO_TEST_CASE(RenameRar5EncryptedTest)
// {
// 	std::string workingDir = currDir / "RenameRar5EncryptedTest";

// 	std::string testFile = workingDir / "testfile5encnam.part01.rar";
// 	std::string testFile2 = workingDir / "testfile5encnam.part02.rar";
// 	std::string testFile3 = workingDir / "testfile5encnam.part03.rar";
// 	std::string renamedTestFile = workingDir / "12348";
// 	std::string renamedTestFile2 = workingDir / "12343";
// 	std::string renamedTestFile3 = workingDir / "12344";

// 	RarRenamerMock rarRenamer(workingDir);
// 	rarRenamer.SetPassword("123");

// 	BOOST_CHECK(FileSystem::MoveFile(testFile.c_str(), renamedTestFile);
// 	BOOST_CHECK(FileSystem::MoveFile(testFile2.c_str(), renamedTestFile2);
// 	BOOST_CHECK(FileSystem::MoveFile(testFile3.c_str(), renamedTestFile3);

// 	rarRenamer.Execute();

// 	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
// }

// BOOST_AUTO_TEST_CASE(RenameRar3EncryptedTest)
// {
// 	std::string workingDir = currDir / "RenameRar3EncryptedTest";

// 	std::string testFile = workingDir / "testfile3encnam.part01.rar";
// 	std::string testFile2 = workingDir / "testfile3encnam.part02.rar";
// 	std::string testFile3 = workingDir / "testfile3encnam.part03.rar";
// 	std::string renamedTestFile = workingDir / "12348";
// 	std::string renamedTestFile2 = workingDir / "12343";
// 	std::string renamedTestFile3 = workingDir / "12344";

// 	RarRenamerMock rarRenamer(workingDir);
// 	rarRenamer.SetPassword("123");

// 	BOOST_CHECK(FileSystem::MoveFile(testFile.c_str(), renamedTestFile);
// 	BOOST_CHECK(FileSystem::MoveFile(testFile2.c_str(), renamedTestFile2);
// 	BOOST_CHECK(FileSystem::MoveFile(testFile3.c_str(), renamedTestFile3);

// 	rarRenamer.Execute();

// 	BOOST_CHECK(rarRenamer.GetRenamedCount() == 3);
// }

#endif
