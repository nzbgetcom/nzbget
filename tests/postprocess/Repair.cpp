/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 NZBGet Contributors
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
#include "Repair.h"

BOOST_AUTO_TEST_SUITE(PostprocessTest)

class RepairDownloadQueueMock final : public DownloadQueue
{
public:
	RepairDownloadQueueMock()
	{
		Init(this);
		Loaded();
	}
	~RepairDownloadQueueMock()
	{
		Final();
	}
	bool EditEntry(int ID, EEditAction action, const char* args) override { return false; }
	bool EditList(IdList* idList, NameList* nameList, EMatchMode matchMode,
		EEditAction action, const char* args) override { return false; }
	void HistoryChanged() override {}
	void Save() override {}
	void SaveChanged() override {}
};

class RepairControllerMock final : public RepairController
{
public:
	explicit RepairControllerMock(uint64 blockSize) : m_blockSize(blockSize) {}

	bool CallRequestMorePars(NzbInfo* nzbInfo, const char* parFilename, int blockNeeded, int* blockFound)
	{
		return RequestMorePars(nzbInfo, parFilename, blockNeeded, blockFound);
	}

	void CallFindPars(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, const char* parFilename,
		Blocks& blocks, bool strictParName, bool exactParName, int* blockFound, uint64 blockSize)
	{
		FindPars(downloadQueue, nzbInfo, parFilename, blocks, strictParName, exactParName, blockFound, blockSize);
	}

protected:
	uint64 GetParBlockSize() const override { return m_blockSize; }

private:
	uint64 m_blockSize;
};

static FileInfo* AddPausedParFile(NzbInfo& nzbInfo, const char* filename, int64 size, bool confirmed = true)
{
	auto fileInfo = std::make_unique<FileInfo>();
	fileInfo->SetNzbInfo(&nzbInfo);
	fileInfo->SetFilename(filename);
	fileInfo->SetFilenameConfirmed(confirmed);
	fileInfo->SetParFile(true);
	fileInfo->SetSize(size);
	fileInfo->SetPaused(true);
	FileInfo* raw = fileInfo.get();
	nzbInfo.GetFileList()->push_back(std::move(fileInfo));
	return raw;
}

BOOST_AUTO_TEST_CASE(EstimateParBlocksTest)
{
	// Empty or invalid file sizes
	BOOST_CHECK_EQUAL(RepairController::EstimateParBlocks(0, 1000), 0);
	BOOST_CHECK_EQUAL(RepairController::EstimateParBlocks(-100, 1000), 0);
	BOOST_CHECK_EQUAL(RepairController::EstimateParBlocks(0, 0), 0);

	// Unknown blockSize fallback (e.g. LoadMainParBak before mainpacket is loaded)
	BOOST_CHECK_EQUAL(RepairController::EstimateParBlocks(1, 0), 1);
	BOOST_CHECK_EQUAL(RepairController::EstimateParBlocks(500000, 0), 1);

	// Standard estimation: fileSize / (blockSize + 68)
	BOOST_CHECK_EQUAL(RepairController::EstimateParBlocks(1068, 1000), 1);
	BOOST_CHECK_EQUAL(RepairController::EstimateParBlocks(4476, 1000), 4);
	BOOST_CHECK_EQUAL(RepairController::EstimateParBlocks(10 * (524288 + 68), 524288), 10);

	// Non-empty file smaller than one slice clamps to 1
	BOOST_CHECK_EQUAL(RepairController::EstimateParBlocks(500, 1000), 1);

	// Overflow guard on blockSize + 68
	BOOST_CHECK_EQUAL(RepairController::EstimateParBlocks(10000, std::numeric_limits<uint64>::max()), 1);
	BOOST_CHECK_EQUAL(RepairController::EstimateParBlocks(10000, std::numeric_limits<uint64>::max() - 10), 1);
	BOOST_CHECK_EQUAL(RepairController::EstimateParBlocks(10000, std::numeric_limits<uint64>::max() - 67), 1);

	// Clamp to INT_MAX for very large fileSize with tiny blockSize
	BOOST_CHECK_EQUAL(RepairController::EstimateParBlocks(std::numeric_limits<int64>::max(), 1),
		std::numeric_limits<int>::max());
}

BOOST_AUTO_TEST_CASE(RequestMoreParsVolDashAndVolNumericTest)
{
	RepairDownloadQueueMock downloadQueue;
	NzbInfo nzbInfo;
	nzbInfo.SetName("movie");

	const uint64 blockSize = 1000;
	const int64 sliceSize = static_cast<int64>(blockSize + RepairController::PAR2_RECOVERY_SLICE_OVERHEAD);

	FileInfo* indexPar = AddPausedParFile(nzbInfo, "movie.par2", 4 * sliceSize);
	FileInfo* vol00 = AddPausedParFile(nzbInfo, "movie.vol-00.par2", 5 * sliceSize);
	FileInfo* vol01 = AddPausedParFile(nzbInfo, "movie.vol-01.par2", 10 * sliceSize);
	FileInfo* vol02 = AddPausedParFile(nzbInfo, "movie.vol-02.par2", 20 * sliceSize);
	FileInfo* vol03 = AddPausedParFile(nzbInfo, "movie.vol03.par2", 8 * sliceSize);

	RepairControllerMock repair(blockSize);
	int blockFound = 0;
	bool ok = repair.CallRequestMorePars(&nzbInfo, "/tmp/movie.par2", 12, &blockFound);

	BOOST_CHECK(ok);
	BOOST_CHECK_EQUAL(blockFound, 5 + 10 + 20 + 8);
	// Index .par2 must remain paused; smallest volumes covering 12 blocks (vol-00: 5 + vol03: 8 = 13) unpaused
	BOOST_CHECK(indexPar->GetPaused());
	BOOST_CHECK(!vol00->GetPaused());
	BOOST_CHECK(!vol03->GetPaused());
	BOOST_CHECK(vol01->GetPaused());
	BOOST_CHECK(vol02->GetPaused());
}

BOOST_AUTO_TEST_CASE(RequestMoreParsZeroBlockSizeFallbackAndTieBreakTest)
{
	RepairDownloadQueueMock downloadQueue;
	NzbInfo nzbInfo;
	nzbInfo.SetName("movie");

	FileInfo* indexPar = AddPausedParFile(nzbInfo, "movie.par2", 2000);
	FileInfo* volLarge = AddPausedParFile(nzbInfo, "movie.vol-01.par2", 20000);
	FileInfo* volSmall = AddPausedParFile(nzbInfo, "movie.vol-00.par2", 5000);

	// blockSize == 0 simulates LoadMainParBak before mainpacket is available
	RepairControllerMock repair(0);
	int blockFound = 0;
	bool ok = repair.CallRequestMorePars(&nzbInfo, "/tmp/movie.par2", 1, &blockFound);

	BOOST_CHECK(ok);
	BOOST_CHECK_EQUAL(blockFound, 2);
	BOOST_CHECK(indexPar->GetPaused());
	// Tie-breaker by file size picks the smaller volume first
	BOOST_CHECK(!volSmall->GetPaused());
	BOOST_CHECK(volLarge->GetPaused());
}

BOOST_AUTO_TEST_CASE(RequestMoreParsEmptyUnknownVolumeSkippedTest)
{
	RepairDownloadQueueMock downloadQueue;
	NzbInfo nzbInfo;
	nzbInfo.SetName("movie");

	FileInfo* emptyVol = AddPausedParFile(nzbInfo, "movie.vol-00.par2", 0);

	RepairControllerMock repair(1000);
	int blockFound = -1;
	bool ok = repair.CallRequestMorePars(&nzbInfo, "/tmp/movie.par2", 1, &blockFound);

	BOOST_CHECK(!ok);
	BOOST_CHECK_EQUAL(blockFound, 0);
	BOOST_CHECK(emptyVol->GetPaused());
}

BOOST_AUTO_TEST_CASE(RequestMoreParsMixedKnownAndUnknownVolumesTest)
{
	RepairDownloadQueueMock downloadQueue;
	NzbInfo nzbInfo;
	nzbInfo.SetName("my-movie");

	const uint64 blockSize = 1000;
	const int64 sliceSize = static_cast<int64>(blockSize + RepairController::PAR2_RECOVERY_SLICE_OVERHEAD);

	FileInfo* indexPar = AddPausedParFile(nzbInfo, "my-movie.par2", 3 * sliceSize);
	FileInfo* knownVol = AddPausedParFile(nzbInfo, "my-movie.vol00+02.par2", 3 * sliceSize);
	FileInfo* unknownVol = AddPausedParFile(nzbInfo, "my-movie.vol-01.par2", 10 * sliceSize);
	FileInfo* otherVol = AddPausedParFile(nzbInfo, "other.vol-01.par2", 50 * sliceSize);

	RepairControllerMock repair(blockSize);
	int blockFound = 0;
	bool ok = repair.CallRequestMorePars(&nzbInfo, "/tmp/my-movie.par2", 11, &blockFound);

	BOOST_CHECK(ok);
	BOOST_CHECK_EQUAL(blockFound, 12);
	BOOST_CHECK(indexPar->GetPaused());
	BOOST_CHECK(!knownVol->GetPaused());
	BOOST_CHECK(!unknownVol->GetPaused());
	BOOST_CHECK(otherVol->GetPaused());
}

BOOST_AUTO_TEST_SUITE_END()
