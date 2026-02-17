/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2024-2026 Denis <denis@nzbget.com>
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

BOOST_AUTO_TEST_SUITE(PostprocessTest)

namespace fs = boost::filesystem;

static const fs::path CURR_DIR = fs::current_path();
static const fs::path TEST_DATA_DIR = CURR_DIR / "parchecker";

class ParCheckerMock : public ParChecker
{
public:
	ParCheckerMock(const fs::path& workingDir);
	void Execute();
	void CorruptFile(const char* filename, int offset);
	~ParCheckerMock()
	{
		fs::remove_all(m_workingDir);
	}

protected:
	bool RequestMorePars(int blockNeeded, int* blockFound) override { return false; }
	EFileStatus FindFileCrc(const char* filename, uint32* crc, SegmentList* segments) override;

private:
	uint32 CalcFileCrc(const char* filename);
	const fs::path& m_workingDir;
};

ParCheckerMock::ParCheckerMock(const fs::path& workingDir) : m_workingDir(workingDir)
{
	SetDestDir(m_workingDir.string().c_str());
	fs::copy(TEST_DATA_DIR, m_workingDir);
}

void ParCheckerMock::Execute()
{
	ParChecker::Execute();
}

void ParCheckerMock::CorruptFile(const char* filename, int offset)
{
	const fs::path fullfilename =m_workingDir / filename;

	FILE* file = fopen(fullfilename.string().c_str(), FOPEN_RBP);
	BOOST_REQUIRE(file != nullptr);

	fseek(file, offset, SEEK_SET);
	char b = 0;
	int written = fwrite(&b, 1, 1, file);
	BOOST_REQUIRE(written == 1);

	fclose(file);
}

ParCheckerMock::EFileStatus ParCheckerMock::FindFileCrc(const char* filename, uint32* crc, SegmentList* segments)
{
	const fs::path crcFileName = m_workingDir / "crc.txt";
	std::ifstream sm(crcFileName.string().c_str());
	std::string smfilename, smcrc;
	while (!sm.eof())
	{
		sm >> smfilename >> smcrc;
		if (smfilename == filename)
		{
			*crc = strtoul(smcrc.c_str(), nullptr, 16);
			const fs::path file = m_workingDir / filename;
			uint32 realCrc = CalcFileCrc(file.string().c_str());
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

	const fs::path testFile = CURR_DIR / "RepairNoNeedTest";
	ParCheckerMock parChecker(testFile);
	parChecker.Execute();

	BOOST_CHECK_EQUAL(parChecker.GetStatus(), ParChecker::psRepairNotNeeded);
	BOOST_CHECK_EQUAL(parChecker.GetParFull(), true);
}

BOOST_AUTO_TEST_CASE(RepairPossibleTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=no");
	Options options(&cmdOpts, nullptr);

	const fs::path testFile = CURR_DIR / "RepairPossibleTest";
	ParCheckerMock parChecker(testFile);
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.Execute();

	BOOST_CHECK_EQUAL(parChecker.GetStatus(), ParChecker::psRepairPossible);
	BOOST_CHECK_EQUAL(parChecker.GetParFull(), true);
}

BOOST_AUTO_TEST_CASE(RepairSuccessfulTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=yes");
	Options options(&cmdOpts, nullptr);

	const fs::path testFile = CURR_DIR / "RepairSuccessfulTest";
	ParCheckerMock parChecker(testFile);
	
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.Execute();

	BOOST_CHECK_EQUAL(parChecker.GetStatus(), ParChecker::psRepaired);
	BOOST_CHECK_EQUAL(parChecker.GetParFull(), true);
}

BOOST_AUTO_TEST_CASE(RepairFailedTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=no");
	Options options(&cmdOpts, nullptr);

	const fs::path testFile = CURR_DIR / "RepairSuccessfulTest";
	ParCheckerMock parChecker(testFile);
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.CorruptFile("testfile.dat", 30000);
	parChecker.CorruptFile("testfile.dat", 40000);
	parChecker.CorruptFile("testfile.dat", 50000);
	parChecker.CorruptFile("testfile.dat", 60000);
	parChecker.CorruptFile("testfile.dat", 70000);
	parChecker.CorruptFile("testfile.dat", 80000);
	parChecker.Execute();

	BOOST_CHECK_EQUAL(parChecker.GetStatus(), ParChecker::psFailed);
	BOOST_CHECK_EQUAL(parChecker.GetParFull(), true);
}

BOOST_AUTO_TEST_CASE(QuickVerificationRepairNotNeededTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=no");
	Options options(&cmdOpts, nullptr);

	const fs::path testFile = CURR_DIR / "QuickVerificationRepairNotNeededTest";
	ParCheckerMock parChecker(testFile);
	parChecker.SetParQuick(true);
	parChecker.Execute();

	BOOST_CHECK_EQUAL(parChecker.GetStatus(), ParChecker::psRepairNotNeeded);
	BOOST_CHECK_EQUAL(parChecker.GetParFull(), false);
}

BOOST_AUTO_TEST_CASE(QuickVerificationRepairSuccessfulTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=yes");
	Options options(&cmdOpts, nullptr);

	const fs::path testFile = CURR_DIR / "QuickVerificationRepairSuccessfulTest";
	ParCheckerMock parChecker(testFile);
	parChecker.SetParQuick(true);
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.Execute();

	BOOST_CHECK_EQUAL(parChecker.GetStatus(), ParChecker::psRepaired);
	BOOST_CHECK_EQUAL(parChecker.GetParFull(), false);
}

BOOST_AUTO_TEST_CASE(QuickFullVerificationRepairSuccessfulTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=yes");
	Options options(&cmdOpts, nullptr);

	const fs::path testFile = CURR_DIR / "QuickFullVerificationRepairSuccessfulTest";
	ParCheckerMock parChecker(testFile);
	parChecker.SetParQuick(true);
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.CorruptFile("testfile.nfo", 100);
	parChecker.Execute();

	// All files were damaged, the full verification was performed

	BOOST_CHECK_EQUAL(parChecker.GetStatus(), ParChecker::psRepaired);
	BOOST_CHECK_EQUAL(parChecker.GetParFull(), true);
}

BOOST_AUTO_TEST_CASE(ParIgnoreExtDatTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=yes");

	ParChecker::EStatus expectedStatus;

	cmdOpts.push_back("ExtCleanupDisk=.dat");
	expectedStatus = ParChecker::psFailed;

	Options options(&cmdOpts, nullptr);

	const fs::path testFile = CURR_DIR / "ParIgnoreExtDatTest";
	ParCheckerMock parChecker(testFile);
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.CorruptFile("testfile.dat", 30000);
	parChecker.CorruptFile("testfile.dat", 40000);
	parChecker.CorruptFile("testfile.dat", 50000);
	parChecker.CorruptFile("testfile.dat", 60000);
	parChecker.CorruptFile("testfile.dat", 70000);
	parChecker.CorruptFile("testfile.dat", 80000);

	parChecker.Execute();

	BOOST_CHECK_EQUAL(parChecker.GetStatus(), expectedStatus);
}

BOOST_AUTO_TEST_CASE(ExtCleanupDiskDatTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("ParRepair=yes");

	ParChecker::EStatus expectedStatus;

	cmdOpts.push_back("ExtCleanupDisk=.dat");
	expectedStatus = ParChecker::psFailed;

	Options options(&cmdOpts, nullptr);

	const fs::path testFile = CURR_DIR / "ExtCleanupDiskDatTest";
	ParCheckerMock parChecker(testFile);
	parChecker.CorruptFile("testfile.dat", 20000);
	parChecker.CorruptFile("testfile.dat", 30000);
	parChecker.CorruptFile("testfile.dat", 40000);
	parChecker.CorruptFile("testfile.dat", 50000);
	parChecker.CorruptFile("testfile.dat", 60000);
	parChecker.CorruptFile("testfile.dat", 70000);
	parChecker.CorruptFile("testfile.dat", 80000);

	parChecker.Execute();

	BOOST_CHECK_EQUAL(parChecker.GetStatus(), expectedStatus);
}

BOOST_AUTO_TEST_SUITE_END()
