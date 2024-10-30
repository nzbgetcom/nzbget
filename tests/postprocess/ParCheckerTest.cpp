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

#include "Options.h"
#include "ParChecker.h"
#include "TestUtil.h"
#include "FileSystem.h"
#include "YEncode.h"

static std::string currDir = FileSystem::GetCurrentDirectory().Str();
static std::string testDataDir = currDir + "/" + "parchecker";

class ParCheckerMock : public ParChecker
{
public:
	ParCheckerMock(std::string workingDir);
	void Execute();
	void CorruptFile(const char* filename, int offset);
	~ParCheckerMock()
	{
		CString errmsg;
		BOOST_CHECK(FileSystem::DeleteDirectoryWithContent(m_workingDir.c_str(), errmsg));
	}

protected:
	bool RequestMorePars(int blockNeeded, int* blockFound) override { return false; }
	EFileStatus FindFileCrc(const char* filename, uint32* crc, SegmentList* segments) override;

private:
	uint32 CalcFileCrc(const char* filename);
	std::string m_workingDir;
};

ParCheckerMock::ParCheckerMock(std::string workingDir) : m_workingDir(std::move(workingDir))
{
	SetDestDir(m_workingDir.c_str());
	BOOST_REQUIRE(FileSystem::CreateDirectory(m_workingDir.c_str()));
	TestUtil::CopyAllFiles(m_workingDir.c_str(), testDataDir.c_str());
}

void ParCheckerMock::Execute()
{
	ParChecker::Execute();
}

void ParCheckerMock::CorruptFile(const char* filename, int offset)
{
	std::string fullfilename(m_workingDir + "/" + filename);

	FILE* file = fopen(fullfilename.c_str(), FOPEN_RBP);
	BOOST_REQUIRE(file != nullptr);

	fseek(file, offset, SEEK_SET);
	char b = 0;
	int written = fwrite(&b, 1, 1, file);
	BOOST_REQUIRE(written == 1);

	fclose(file);
}

ParCheckerMock::EFileStatus ParCheckerMock::FindFileCrc(const char* filename, uint32* crc, SegmentList* segments)
{
	std::string crcFileName = m_workingDir + "/crc.txt";
	std::ifstream sm(crcFileName);
	std::string smfilename, smcrc;
	while (!sm.eof())
	{
		sm >> smfilename >> smcrc;
		if (smfilename == filename)
		{
			*crc = strtoul(smcrc.c_str(), nullptr, 16);
			uint32 realCrc = CalcFileCrc((m_workingDir + "/" + filename).c_str());
			return *crc == realCrc ? ParChecker::fsSuccess : ParChecker::fsUnknown;
		}
	}
	return ParChecker::fsUnknown;
}

uint32 ParCheckerMock::CalcFileCrc(const char* filename)
{
	FILE* infile = fopen(filename, FOPEN_RB);
	BOOST_REQUIRE(infile);

	CharBuffer buffer(1024 * 64);
	Crc32 downloadCrc;

	int cnt = buffer.Size();
	while (cnt == buffer.Size())
	{
		cnt = (int)fread(buffer, 1, buffer.Size(), infile);
		downloadCrc.Append((uchar*)(char*)buffer, cnt);
	}

	fclose(infile);

	return downloadCrc.Finish();
}

BOOST_AUTO_TEST_CASE(RepairNoNeedTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=no");
	Options options(&cmdOpts, nullptr);

	ParCheckerMock parChecker(currDir + "/" + "RepairNoNeedTest");
	parChecker.Execute();

	BOOST_CHECK(parChecker.GetStatus() == ParChecker::psRepairNotNeeded);
	BOOST_CHECK(parChecker.GetParFull() == true);
}

BOOST_AUTO_TEST_CASE(RepairPossibleTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=no");
	Options options(&cmdOpts, nullptr);

	ParCheckerMock parChecker(currDir + "/" + "RepairPossibleTest");
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.Execute();

	BOOST_CHECK(parChecker.GetStatus() == ParChecker::psRepairPossible);
	BOOST_CHECK(parChecker.GetParFull() == true);
}

BOOST_AUTO_TEST_CASE(RepairSuccessfulTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=yes");
	Options options(&cmdOpts, nullptr);

	ParCheckerMock parChecker(currDir + "/" + "RepairSuccessfulTest");
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.Execute();

	BOOST_CHECK(parChecker.GetStatus() == ParChecker::psRepaired);
	BOOST_CHECK(parChecker.GetParFull() == true);
}

BOOST_AUTO_TEST_CASE(RepairFailedTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=no");
	Options options(&cmdOpts, nullptr);

	ParCheckerMock parChecker(currDir + "/" + "RepairSuccessfulTest");
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.CorruptFile("testfile.dat", 30000);
	parChecker.CorruptFile("testfile.dat", 40000);
	parChecker.CorruptFile("testfile.dat", 50000);
	parChecker.CorruptFile("testfile.dat", 60000);
	parChecker.CorruptFile("testfile.dat", 70000);
	parChecker.CorruptFile("testfile.dat", 80000);
	parChecker.Execute();

	BOOST_CHECK(parChecker.GetStatus() == ParChecker::psFailed);
	BOOST_CHECK(parChecker.GetParFull() == true);
}

BOOST_AUTO_TEST_CASE(QuickVerificationRepairNotNeededTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=no");
	Options options(&cmdOpts, nullptr);

	YEncode::init();

	ParCheckerMock parChecker(currDir + "/" + "QuickVerificationRepairNotNeededTest");
	parChecker.SetParQuick(true);
	parChecker.Execute();

	BOOST_CHECK(parChecker.GetStatus() == ParChecker::psRepairNotNeeded);
	BOOST_CHECK(parChecker.GetParFull() == false);
}

BOOST_AUTO_TEST_CASE(QuickVerificationRepairSuccessfulTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=yes");
	Options options(&cmdOpts, nullptr);

	YEncode::init();

	ParCheckerMock parChecker(currDir + "/" + "QuickVerificationRepairSuccessfulTest");
	parChecker.SetParQuick(true);
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.Execute();

	BOOST_CHECK(parChecker.GetStatus() == ParChecker::psRepaired);
	BOOST_CHECK(parChecker.GetParFull() == false);
}

BOOST_AUTO_TEST_CASE(QuickFullVerificationRepairSuccessfulTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=yes");
	Options options(&cmdOpts, nullptr);

	YEncode::init();

	ParCheckerMock parChecker(currDir + "/" + "QuickFullVerificationRepairSuccessfulTest");
	parChecker.SetParQuick(true);
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.CorruptFile("testfile.nfo", 100);
	parChecker.Execute();

	// All files were damaged, the full verification was performed

	BOOST_CHECK(parChecker.GetStatus() == ParChecker::psRepaired);
	BOOST_CHECK(parChecker.GetParFull() == true);
}

BOOST_AUTO_TEST_CASE(ParIgnoreExtDatTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=yes");

	ParChecker::EStatus expectedStatus;

	cmdOpts.push_back("ExtCleanupDisk=.dat");
	expectedStatus = ParChecker::psFailed;

	Options options(&cmdOpts, nullptr);

	ParCheckerMock parChecker(currDir + "/" + "ParIgnoreExtDatTest");
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.CorruptFile("testfile.dat", 30000);
	parChecker.CorruptFile("testfile.dat", 40000);
	parChecker.CorruptFile("testfile.dat", 50000);
	parChecker.CorruptFile("testfile.dat", 60000);
	parChecker.CorruptFile("testfile.dat", 70000);
	parChecker.CorruptFile("testfile.dat", 80000);

	parChecker.Execute();

	BOOST_CHECK(parChecker.GetStatus() == expectedStatus);
}

BOOST_AUTO_TEST_CASE(ExtCleanupDiskDatTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=yes");

	ParChecker::EStatus expectedStatus;

	cmdOpts.push_back("ExtCleanupDisk=.dat");
	expectedStatus = ParChecker::psFailed;

	Options options(&cmdOpts, nullptr);

	ParCheckerMock parChecker(currDir + "/" + "ExtCleanupDiskDatTest");
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.CorruptFile("testfile.dat", 30000);
	parChecker.CorruptFile("testfile.dat", 40000);
	parChecker.CorruptFile("testfile.dat", 50000);
	parChecker.CorruptFile("testfile.dat", 60000);
	parChecker.CorruptFile("testfile.dat", 70000);
	parChecker.CorruptFile("testfile.dat", 80000);

	parChecker.Execute();

	BOOST_CHECK(parChecker.GetStatus() == expectedStatus);
}
