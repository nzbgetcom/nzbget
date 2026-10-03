/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Denis <denis@nzbget.com>
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

#include <set>

#include <boost/test/unit_test.hpp>
#include <fstream>
#include "DownloadInfo.h"
#include "DupeStreamRepair.h"
#include "DupeArticleFallback.h"
#include "FileSystem.h"
#include "Options.h"

BOOST_AUTO_TEST_SUITE(QueueTest)

namespace
{

// builds a file whose articles carry known decoded ranges; a part with
// size 0 is marked failed (failed articles carry no segment data)
std::unique_ptr<FileInfo> BuildStreamFile(int64 decodedFileSize,
	std::vector<std::pair<int64, int>> parts)
{
	std::unique_ptr<FileInfo> fileInfo = std::make_unique<FileInfo>();
	fileInfo->SetFilename("movie.mkv");
	fileInfo->SetDecodedFileSize(decodedFileSize);
	int partNumber = 1;
	int successArticles = 0;
	int failedArticles = 0;
	for (std::pair<int64, int>& part : parts)
	{
		std::unique_ptr<ArticleInfo> article = std::make_unique<ArticleInfo>();
		article->SetPartNumber(partNumber++);
		if (part.second > 0)
		{
			article->SetStatus(ArticleInfo::aiFinished);
			article->SetSegmentOffset(part.first);
			article->SetSegmentSize(part.second);
			successArticles++;
		}
		else
		{
			article->SetStatus(ArticleInfo::aiFailed);
			failedArticles++;
		}
		fileInfo->GetArticles()->push_back(std::move(article));
	}
	fileInfo->SetTotalArticles((int)parts.size());
	fileInfo->SetSuccessArticles(successArticles);
	fileInfo->SetFailedArticles(failedArticles);
	return fileInfo;
}

// donor file as parsed from an nzb: per-article ENCODED sizes only
std::unique_ptr<FileInfo> BuildDonorFile(const char* filename, std::vector<int> encodedSizes)
{
	std::unique_ptr<FileInfo> fileInfo = std::make_unique<FileInfo>();
	fileInfo->SetFilename(filename);
	int partNumber = 1;
	int64 total = 0;
	for (int encodedSize : encodedSizes)
	{
		std::unique_ptr<ArticleInfo> article = std::make_unique<ArticleInfo>();
		article->SetPartNumber(partNumber);
		article->SetSize(encodedSize);
		article->SetMessageId(BString<1024>("donor-%i@example.com", partNumber));
		total += encodedSize;
		partNumber++;
		fileInfo->GetArticles()->push_back(std::move(article));
	}
	fileInfo->SetSize(total);
	fileInfo->SetTotalArticles((int)encodedSizes.size());
	return fileInfo;
}

// restores g_Options after a test constructs local Options instances
// (BuildRepairJob reads the global): the Options constructor repoints
// g_Options at itself and its destructor nulls it, so the guard must be
// declared BEFORE any local Options (destroyed after them) or later tests
// in this binary see a null g_Options
struct OptionsGuard
{
	Options* m_prev = g_Options;
	~OptionsGuard() { g_Options = m_prev; }
};

} // namespace

BOOST_AUTO_TEST_CASE(StreamRepairEligibilityTest)
{
	BOOST_CHECK(DupeStreamRepair::IsStreamEligible("movie.mkv"));
	BOOST_CHECK(DupeStreamRepair::IsStreamEligible("MOVIE.MKV"));
	BOOST_CHECK(DupeStreamRepair::IsStreamEligible("clip.mp4"));
	BOOST_CHECK(DupeStreamRepair::IsStreamEligible("show.ts"));
	BOOST_CHECK(!DupeStreamRepair::IsStreamEligible("release.r01"));
	BOOST_CHECK(!DupeStreamRepair::IsStreamEligible("archive.rar"));
	BOOST_CHECK(!DupeStreamRepair::IsStreamEligible("movie.par2"));
	BOOST_CHECK(!DupeStreamRepair::IsStreamEligible("noextension"));
	BOOST_CHECK(!DupeStreamRepair::IsStreamEligible(""));
	BOOST_CHECK(!DupeStreamRepair::IsStreamEligible(nullptr));
}

BOOST_AUTO_TEST_CASE(StreamRepairComputeHolesTest)
{
	// 1000 bytes: [0,300) ok, [300,500) missing, [500,800) ok, [800,1000) missing
	std::unique_ptr<FileInfo> fileInfo = BuildStreamFile(1000,
		{{0, 300}, {300, 0}, {500, 300}, {800, 0}});

	StreamRangeList holes = DupeStreamRepair::ComputeHoles(fileInfo.get());

	BOOST_REQUIRE_EQUAL(holes.size(), 2u);
	BOOST_CHECK_EQUAL(holes[0].Offset, 300);
	BOOST_CHECK_EQUAL(holes[0].Size, 200);
	BOOST_CHECK_EQUAL(holes[1].Offset, 800);
	BOOST_CHECK_EQUAL(holes[1].Size, 200);
	BOOST_CHECK_EQUAL(DupeStreamRepair::TotalSize(holes), 400);
}

BOOST_AUTO_TEST_CASE(StreamRepairComputeHolesLeadingHoleTest)
{
	// leading hole: first article missing
	std::unique_ptr<FileInfo> fileInfo = BuildStreamFile(600,
		{{0, 0}, {200, 400}});

	StreamRangeList holes = DupeStreamRepair::ComputeHoles(fileInfo.get());

	BOOST_REQUIRE_EQUAL(holes.size(), 1u);
	BOOST_CHECK_EQUAL(holes[0].Offset, 0);
	BOOST_CHECK_EQUAL(holes[0].Size, 200);
}

BOOST_AUTO_TEST_CASE(StreamRepairComputeHolesCompleteAndUnknownTest)
{
	// complete file has no holes
	std::unique_ptr<FileInfo> complete = BuildStreamFile(700, {{0, 300}, {300, 400}});
	BOOST_CHECK(DupeStreamRepair::ComputeHoles(complete.get()).empty());

	// unknown decoded size cannot produce holes
	std::unique_ptr<FileInfo> unknown = BuildStreamFile(0, {{0, 300}, {300, 0}});
	BOOST_CHECK(DupeStreamRepair::ComputeHoles(unknown.get()).empty());
}

BOOST_AUTO_TEST_CASE(StreamRepairComputeHolesOverlapTest)
{
	// overlapping/unordered finished ranges must not create phantom holes
	std::unique_ptr<FileInfo> fileInfo = BuildStreamFile(1000,
		{{500, 300}, {0, 400}, {300, 300}});

	StreamRangeList holes = DupeStreamRepair::ComputeHoles(fileInfo.get());

	BOOST_REQUIRE_EQUAL(holes.size(), 1u);
	BOOST_CHECK_EQUAL(holes[0].Offset, 800);
	BOOST_CHECK_EQUAL(holes[0].Size, 200);
}

BOOST_AUTO_TEST_CASE(StreamRepairEstimateDonorRangesTest)
{
	// uniform parts: 4 x 250 encoded over 1000 decoded -> exact quarters
	std::unique_ptr<FileInfo> uniform = BuildDonorFile("movie.mkv", {250, 250, 250, 250});
	StreamRangeList ranges = DupeStreamRepair::EstimateDonorRanges(uniform.get(), 1000);
	BOOST_REQUIRE_EQUAL(ranges.size(), 4u);
	for (int i = 0; i < 4; i++)
	{
		BOOST_CHECK_EQUAL(ranges[i].Offset, i * 250);
		BOOST_CHECK_EQUAL(ranges[i].Size, 250);
	}

	// uneven parts scale proportionally and still cover [0, size) exactly
	std::unique_ptr<FileInfo> uneven = BuildDonorFile("movie.mkv", {300, 100, 100});
	StreamRangeList ranges2 = DupeStreamRepair::EstimateDonorRanges(uneven.get(), 1000);
	BOOST_REQUIRE_EQUAL(ranges2.size(), 3u);
	BOOST_CHECK_EQUAL(ranges2[0].Offset, 0);
	BOOST_CHECK_EQUAL(ranges2[0].Size, 600);
	BOOST_CHECK_EQUAL(ranges2[1].Offset, 600);
	BOOST_CHECK_EQUAL(ranges2[1].Size, 200);
	BOOST_CHECK_EQUAL(ranges2[2].Offset, 800);
	BOOST_CHECK_EQUAL(ranges2[2].Size, 200);

	// degenerate inputs yield no ranges
	std::unique_ptr<FileInfo> empty = BuildDonorFile("movie.mkv", {});
	BOOST_CHECK(DupeStreamRepair::EstimateDonorRanges(empty.get(), 1000).empty());
	BOOST_CHECK(DupeStreamRepair::EstimateDonorRanges(uniform.get(), 0).empty());
}

BOOST_AUTO_TEST_CASE(StreamRepairSelectPatchPartsTest)
{
	StreamRangeList donorRanges;
	for (int i = 0; i < 10; i++)
	{
		donorRanges.push_back({i * 100, 100});
	}

	// hole [450,550) overlaps parts 4 and 5; margin 1 adds 3 and 6
	StreamRangeList holes = {{450, 100}};
	std::vector<int> picks = DupeStreamRepair::SelectPatchParts(donorRanges, holes, 1);
	BOOST_REQUIRE_EQUAL(picks.size(), 4u);
	BOOST_CHECK_EQUAL(picks[0], 3);
	BOOST_CHECK_EQUAL(picks[1], 4);
	BOOST_CHECK_EQUAL(picks[2], 5);
	BOOST_CHECK_EQUAL(picks[3], 6);

	// margin clamps at the array edges
	StreamRangeList edgeHole = {{0, 50}};
	std::vector<int> edgePicks = DupeStreamRepair::SelectPatchParts(donorRanges, edgeHole, 2);
	BOOST_REQUIRE_EQUAL(edgePicks.size(), 3u);
	BOOST_CHECK_EQUAL(edgePicks[0], 0);
	BOOST_CHECK_EQUAL(edgePicks[2], 2);

	// no holes -> nothing to patch
	BOOST_CHECK(DupeStreamRepair::SelectPatchParts(donorRanges, StreamRangeList(), 2).empty());
}

BOOST_AUTO_TEST_CASE(StreamRepairSelectProbePartsTest)
{
	StreamRangeList donorRanges;
	for (int i = 0; i < 10; i++)
	{
		donorRanges.push_back({i * 100, 100});
	}

	// hole [450,550): parts 4,5 hit it, 3,6 are their neighbors ->
	// probe candidates are {0,1,2,7,8,9}; two probes spread across them
	StreamRangeList holes = {{450, 100}};
	std::vector<int> probes = DupeStreamRepair::SelectProbeParts(donorRanges, holes, 2);
	BOOST_REQUIRE_EQUAL(probes.size(), 2u);
	BOOST_CHECK_EQUAL(probes[0], 1);
	BOOST_CHECK_EQUAL(probes[1], 8);

	// entirely holey file has no probe candidates
	StreamRangeList allHoles = {{0, 1000}};
	BOOST_CHECK(DupeStreamRepair::SelectProbeParts(donorRanges, allHoles, 2).empty());

	// fewer candidates than probes: return them all
	StreamRangeList bigHole = {{0, 850}}; // leaves only part 9 (window 8-9 clear? no)
	std::vector<int> few = DupeStreamRepair::SelectProbeParts(donorRanges, bigHole, 2);
	// hole covers parts 0-8; part 9's neighbor window includes part 8 -> no candidates
	BOOST_CHECK(few.empty());
}

BOOST_AUTO_TEST_CASE(StreamRepairSubtractCoveredTest)
{
	// carve the middle out of a hole
	StreamRangeList holes = {{300, 200}, {800, 200}};
	DupeStreamRepair::SubtractCovered(holes, {350, 100});
	BOOST_REQUIRE_EQUAL(holes.size(), 3u);
	BOOST_CHECK_EQUAL(holes[0].Offset, 300);
	BOOST_CHECK_EQUAL(holes[0].Size, 50);
	BOOST_CHECK_EQUAL(holes[1].Offset, 450);
	BOOST_CHECK_EQUAL(holes[1].Size, 50);
	BOOST_CHECK_EQUAL(holes[2].Offset, 800);
	BOOST_CHECK_EQUAL(holes[2].Size, 200);

	// full cover removes the hole entirely
	DupeStreamRepair::SubtractCovered(holes, {800, 200});
	BOOST_REQUIRE_EQUAL(holes.size(), 2u);

	// no overlap leaves the list untouched
	DupeStreamRepair::SubtractCovered(holes, {600, 100});
	BOOST_CHECK_EQUAL(holes.size(), 2u);
}

BOOST_AUTO_TEST_CASE(StreamRepairBuildRepairJobTest)
{
	OptionsGuard optionsGuard;

	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("DupeArticleFallback=stream");
	Options streamOptions(&cmdOpts, nullptr); // constructor sets g_Options

	{
		NzbInfo nzbInfo;

		// incomplete media file: captured with its hole
		std::unique_ptr<FileInfo> media = BuildStreamFile(1000, {{0, 300}, {300, 0}, {500, 500}});
		media->SetNzbInfo(&nzbInfo);
		BOOST_CHECK(DupeStreamRepair::BuildRepairJob(media.get(), "movie.mkv"));
		BOOST_REQUIRE_EQUAL(nzbInfo.GetStreamRepairJobs()->size(), 1u);
		StreamRepairJob& job = (*nzbInfo.GetStreamRepairJobs())[0];
		BOOST_CHECK_EQUAL(job.GetFilename(), "movie.mkv");
		BOOST_CHECK_EQUAL(job.GetDecodedFileSize(), 1000);
		BOOST_REQUIRE_EQUAL(job.GetHoles()->size(), 1u);
		BOOST_CHECK_EQUAL((*job.GetHoles())[0].Offset, 300);
		BOOST_CHECK_EQUAL((*job.GetHoles())[0].Size, 200);

		// A byte-identical repost can donate to archive data as well as media.
		std::unique_ptr<FileInfo> rar = BuildStreamFile(1000, {{0, 300}, {300, 0}});
		rar->SetFilename("release.r01");
		rar->SetNzbInfo(&nzbInfo);
		BOOST_CHECK(DupeStreamRepair::BuildRepairJob(rar.get(), "release.r01"));

		// but not once post-processing started (late par2 completions must
		// not re-arm the drained job list)
		nzbInfo.EnterPostProcess();
		std::unique_ptr<FileInfo> late = BuildStreamFile(1000, {{0, 300}, {300, 0}});
		late->SetFilename("late.par2");
		late->SetNzbInfo(&nzbInfo);
		BOOST_CHECK(!DupeStreamRepair::BuildRepairJob(late.get(), "late.par2"));
		nzbInfo.LeavePostProcess();

		// complete file is not captured
		std::unique_ptr<FileInfo> complete = BuildStreamFile(800, {{0, 300}, {300, 500}});
		complete->SetNzbInfo(&nzbInfo);
		BOOST_CHECK(!DupeStreamRepair::BuildRepairJob(complete.get(), "movie2.mkv"));

		BOOST_CHECK_EQUAL(nzbInfo.GetStreamRepairJobs()->size(), 2u);
	}

	// below "stream" (the default) nothing is captured; a freshly-parsed
	// default Options repoints g_Options via its constructor
	Options::CmdOptList noCmdOpts;
	Options noOptions(&noCmdOpts, nullptr);

	NzbInfo plainNzb;
	std::unique_ptr<FileInfo> media = BuildStreamFile(1000, {{0, 300}, {300, 0}});
	media->SetNzbInfo(&plainNzb);
	BOOST_CHECK(!DupeStreamRepair::BuildRepairJob(media.get(), "movie.mkv"));
	BOOST_CHECK(plainNzb.GetStreamRepairJobs()->empty());
}

BOOST_AUTO_TEST_CASE(StreamRepairWholeFileJobTest)
{
	OptionsGuard optionsGuard;

	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("DupeArticleFallback=stream");
	Options streamOptions(&cmdOpts, nullptr);

	NzbInfo nzbInfo;

	// a file none of whose articles arrived: captured whole, with no decoded
	// size and no holes (both are learned from the first donor article)
	std::unique_ptr<FileInfo> missing = BuildStreamFile(0, {{0, 0}, {0, 0}, {0, 0}});
	missing->SetFilename("ZGWRiqtR4jt2nswwN.part03.rar");
	missing->SetSize(2400000);
	missing->SetFailedSize(1600000);
	missing->SetMissedSize(800000);
	missing->SetMissedArticles(1);
	missing->SetFailedArticles(2);
	missing->SetNzbInfo(&nzbInfo);
	BOOST_CHECK(DupeStreamRepair::BuildRepairJob(missing.get(), "ZGWRiqtR4jt2nswwN.part03.rar"));
	BOOST_REQUIRE_EQUAL(nzbInfo.GetStreamRepairJobs()->size(), 1u);
	StreamRepairJob& job = (*nzbInfo.GetStreamRepairJobs())[0];
	BOOST_CHECK_EQUAL(job.GetDecodedFileSize(), 0);
	BOOST_CHECK(job.GetHoles()->empty());
	BOOST_CHECK_EQUAL(job.GetFailedSize() + job.GetMissedSize(), 2400000);
	BOOST_CHECK_EQUAL(job.GetFailedArticles(), 3);

	// a parity file is never captured, whole or not
	std::unique_ptr<FileInfo> par = BuildStreamFile(0, {{0, 0}, {0, 0}});
	par->SetFilename("rel.vol00+01.par2");
	par->SetSize(100000);
	par->SetParFile(true);
	par->SetNzbInfo(&nzbInfo);
	BOOST_CHECK(!DupeStreamRepair::BuildRepairJob(par.get(), "rel.vol00+01.par2"));

	// an nzb entry without a declared size cannot be paired with anything
	std::unique_ptr<FileInfo> sizeless = BuildStreamFile(0, {{0, 0}});
	sizeless->SetFilename("x.r00");
	sizeless->SetNzbInfo(&nzbInfo);
	BOOST_CHECK(!DupeStreamRepair::BuildRepairJob(sizeless.get(), "x.r00"));
	BOOST_CHECK_EQUAL(nzbInfo.GetStreamRepairJobs()->size(), 1u);
}

BOOST_AUTO_TEST_CASE(StreamRepairDecodedSizePlausibleTest)
{
	// yEnc adds 1-3% on top of the decoded bytes
	BOOST_CHECK(DupeStreamRepair::DecodedSizePlausible(1000000, 1020000));
	BOOST_CHECK(DupeStreamRepair::DecodedSizePlausible(1020000, 1020000));
	BOOST_CHECK(!DupeStreamRepair::DecodedSizePlausible(1020001, 1020000));
	BOOST_CHECK(!DupeStreamRepair::DecodedSizePlausible(800000, 1020000));
	BOOST_CHECK(!DupeStreamRepair::DecodedSizePlausible(0, 1020000));
	BOOST_CHECK(!DupeStreamRepair::DecodedSizePlausible(1000, 0));
}

BOOST_AUTO_TEST_CASE(StreamRepairSelectWholeFileDonorTest)
{
	std::vector<int> flat(12, 768000);
	std::vector<int> twin = {770318, 768951, 771402, 769037, 770660, 769845, 768590, 771113, 769722, 771000, 769500, 398760};
	std::vector<int> other = {769211, 770840, 768392, 771005, 769930, 768777, 770123, 769504, 771288, 770100, 769400, 398760};
	auto shifted = [](std::vector<int> sizes, int shift)
	{
		for (int& size : sizes) size += shift;
		return sizes;
	};
	auto total = [](const std::vector<int>& sizes)
	{
		int64 sum = 0;
		for (int size : sizes) sum += size;
		return sum;
	};

	// 1. the size-step fingerprint identifies the member whatever the names
	{
		std::unique_ptr<FileInfo> target = BuildDonorFile("Q8aZ1kLmN0pR", twin);
		uint64 hash = DupeArticleFallback::ArticleSizeStepsHash(target.get());
		NzbInfo donorNzb;
		donorNzb.GetFileList()->Add(BuildDonorFile("aaa.bin", shifted(other, 27)), false);
		donorNzb.GetFileList()->Add(BuildDonorFile("ccc.bin", shifted(twin, 27)), false);
		FileInfo* match = DupeStreamRepair::SelectWholeFileDonor("Q8aZ1kLmN0pR", total(twin), 12, hash, &donorNzb);
		BOOST_REQUIRE(match);
		BOOST_CHECK_EQUAL(match->GetFilename(), "ccc.bin");

		// a member already proven to be another file's twin is skipped
		std::set<FileInfo*> claimed = { match };
		BOOST_CHECK(DupeStreamRepair::SelectWholeFileDonor("Q8aZ1kLmN0pR", total(twin), 12, hash, &donorNzb, &claimed) == nullptr);
	}

	// 2. flat sizes: the exact name, else the volume suffix - never size alone
	{
		NzbInfo donorNzb;
		donorNzb.GetFileList()->Add(BuildDonorFile("LN2ttWTMLC4N1DkTm.part02.rar", flat), false);
		donorNzb.GetFileList()->Add(BuildDonorFile("LN2ttWTMLC4N1DkTm.part03.rar", flat), false);
		donorNzb.GetFileList()->Add(BuildDonorFile("LN2ttWTMLC4N1DkTm.part04.rar", flat), false);
		donorNzb.GetFileList()->Add(BuildDonorFile("rel.vol00+08.par2", flat), false);
		(*donorNzb.GetFileList())[3]->SetParFile(true);

		FileInfo* match = DupeStreamRepair::SelectWholeFileDonor("x8Tdn3e3iFf1MaWLu.part03.rar", total(flat), 12, 0, &donorNzb);
		BOOST_REQUIRE(match);
		BOOST_CHECK_EQUAL(match->GetFilename(), "LN2ttWTMLC4N1DkTm.part03.rar");

		match = DupeStreamRepair::SelectWholeFileDonor("ln2ttwtmlc4n1dktm.PART04.RAR", total(flat), 12, 0, &donorNzb);
		BOOST_REQUIRE(match);
		BOOST_CHECK_EQUAL(match->GetFilename(), "LN2ttWTMLC4N1DkTm.part04.rar");

		// no usable name: nothing, although every member has the right size
		BOOST_CHECK(DupeStreamRepair::SelectWholeFileDonor("x8Tdn3e3iFf1MaWLu", total(flat), 12, 0, &donorNzb) == nullptr);
		// a repost cut into other article sizes still pairs by its volume
		// numbering; a different size never does
		match = DupeStreamRepair::SelectWholeFileDonor("x8Tdn3e3iFf1MaWLu.part03.rar", total(flat), 20, 0, &donorNzb);
		BOOST_REQUIRE(match);
		BOOST_CHECK_EQUAL(match->GetFilename(), "LN2ttWTMLC4N1DkTm.part03.rar");
		BOOST_CHECK(DupeStreamRepair::SelectWholeFileDonor("x8Tdn3e3iFf1MaWLu.part03.rar", total(flat) * 2, 12, 0, &donorNzb) == nullptr);
		// parity is neither target nor donor
		BOOST_CHECK(DupeStreamRepair::SelectWholeFileDonor("rel.vol00+08.par2", total(flat), 12, 0, &donorNzb) == nullptr);
	}
}

BOOST_AUTO_TEST_CASE(StreamRepairSuffixKeyTest)
{
	BOOST_CHECK_EQUAL(DupeStreamRepair::SuffixKey("Rel.part03.rar"), "part03.rar");
	BOOST_CHECK_EQUAL(DupeStreamRepair::SuffixKey("REL.R00"), "r00");
	BOOST_CHECK_EQUAL(DupeStreamRepair::SuffixKey("movie.mkv"), "mkv");
	BOOST_CHECK_EQUAL(DupeStreamRepair::SuffixKey("a.vol07+08.par2"), "vol07+08.par2");
	BOOST_CHECK_EQUAL(DupeStreamRepair::SuffixKey("archive.7z.001"), "7z.001");
	BOOST_CHECK_EQUAL(DupeStreamRepair::SuffixKey("noextension"), "");
	BOOST_CHECK_EQUAL(DupeStreamRepair::SuffixKey(""), "");
	BOOST_CHECK_EQUAL(DupeStreamRepair::SuffixKey(nullptr), "");
}

BOOST_AUTO_TEST_CASE(StreamRepairDoesNotCaptureParFilesTest)
{
	OptionsGuard optionsGuard;
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("DupeArticleFallback=stream");
	Options options(&cmdOpts, nullptr);
	NzbInfo nzb;

	for (int kind = 0; kind < 3; kind++)
	{
		std::unique_ptr<FileInfo> file = BuildStreamFile(1000, {{0, 300}, {300, 0}});
		file->SetNzbInfo(&nzb);
		file->SetFilename(kind == 1 ? "release.PaR2" : "obfuscated.bin");
		file->SetFilenameConfirmed(kind == 1);
		file->SetParFile(kind == 0);
		const char* diskName = kind == 2 ? "release.vol01+02.PAR2" : "obfuscated.bin";

		// PAR flags, confirmed names, and decoded disk names each identify parity.
		BOOST_CHECK(!DupeStreamRepair::BuildRepairJob(file.get(), diskName));
		BOOST_CHECK(nzb.GetStreamRepairJobs()->empty());
	}
}

BOOST_AUTO_TEST_CASE(StreamRepairRefusesParTargetCandidatesTest)
{
	NzbInfo donor;
	donor.GetFileList()->Add(BuildDonorFile("release.PAR2", {500, 500}), false);
	donor.GetFileList()->Add(BuildDonorFile("renamed.bin", {500, 500}), false);
	BOOST_CHECK(DupeStreamRepair::SelectDonorCandidates(
		"release.PAR2", 980, 0, 2, &donor, DupeStreamRepair::MaxDonorCandidates).empty());
}

BOOST_AUTO_TEST_CASE(StreamRepairExcludesParDonorCandidatesTest)
{
	NzbInfo donor;
	std::unique_ptr<FileInfo> flagged = BuildDonorFile("release.r01", {500, 500});
	flagged->SetParFile(true);
	donor.GetFileList()->Add(std::move(flagged), false);
	std::unique_ptr<FileInfo> named = BuildDonorFile("release.PaR2", {500, 500});
	named->SetFilenameConfirmed(true);
	donor.GetFileList()->Add(std::move(named), false);
	donor.GetFileList()->Add(BuildDonorFile("renamed.bin", {500, 500}), false);

	// Neither exact name nor equal size makes a parity donor acceptable.
	std::vector<FileInfo*> candidates = DupeStreamRepair::SelectDonorCandidates(
		"release.r01", 980, 0, 3, &donor, DupeStreamRepair::MaxDonorCandidates);
	BOOST_REQUIRE_EQUAL(candidates.size(), 1u);
	BOOST_CHECK_EQUAL(candidates[0]->GetFilename(), "renamed.bin");
}

BOOST_AUTO_TEST_CASE(StreamRepairStepFingerprintCandidateTest)
{
	// A target whose name stayed obfuscated (its first article was missing, so
	// par-rename could not identify it) still finds its byte-identical twin in
	// an obfuscated repost: the member whose article sizes step like its own.
	std::vector<int> twin = {770318, 768951, 771402, 769037, 770660, 769845, 768590, 771113, 769722, 398760};
	std::vector<int> other = {769211, 770840, 768392, 771005, 769930, 768777, 770123, 769504, 771288, 398760};
	std::vector<int> third = {768845, 771230, 769918, 770402, 768611, 771077, 769283, 770936, 768458, 398760};
	auto shifted = [](std::vector<int> sizes, int shift)
	{
		for (int& size : sizes) size += shift;
		return sizes;
	};

	std::unique_ptr<FileInfo> target = BuildDonorFile("Q8aZ1kLmN0pR", twin);
	uint64 targetHash = DupeArticleFallback::ArticleSizeStepsHash(target.get());
	BOOST_CHECK(targetHash != 0);

	NzbInfo donorNzb;
	donorNzb.GetFileList()->Add(BuildDonorFile("aaa.bin", shifted(other, 27)), false);
	donorNzb.GetFileList()->Add(BuildDonorFile("ccc.bin", shifted(twin, 27)), false);
	donorNzb.GetFileList()->Add(BuildDonorFile("bbb.bin", shifted(third, 27)), false);
	int64 decodedSize = 7300000;

	std::vector<FileInfo*> candidates = DupeStreamRepair::SelectDonorCandidates(
		"Q8aZ1kLmN0pR", decodedSize, -1, 0, &donorNzb, DupeStreamRepair::MaxDonorCandidates,
		targetHash);
	BOOST_REQUIRE(!candidates.empty());
	BOOST_CHECK_EQUAL(candidates[0]->GetFilename(), "ccc.bin");

	// uniform article sizes carry no fingerprint
	std::vector<int> flat(10, 750000);
	std::unique_ptr<FileInfo> flatFile = BuildDonorFile("flat.bin", flat);
	BOOST_CHECK_EQUAL(DupeArticleFallback::ArticleSizeStepsHash(flatFile.get()), 0u);
}

BOOST_AUTO_TEST_CASE(StreamRepairSelectDonorCandidatesTest)
{
	// donor "repost": three equal-size volumes + a small nfo outside the window
	NzbInfo donorNzb;
	donorNzb.GetFileList()->Add(BuildDonorFile("rel.part01.rar", {500, 500}), false);
	donorNzb.GetFileList()->Add(BuildDonorFile("rel.part02.rar", {500, 500}), false);
	donorNzb.GetFileList()->Add(BuildDonorFile("rel.part03.rar", {500, 500}), false);
	donorNzb.GetFileList()->Add(BuildDonorFile("info.nfo", {50}), false);

	// (1) exact name match wins
	std::vector<FileInfo*> byName = DupeStreamRepair::SelectDonorCandidates(
		"rel.part02.rar", 980, -1, 0, &donorNzb, DupeStreamRepair::MaxDonorCandidates);
	BOOST_REQUIRE(!byName.empty());
	BOOST_CHECK_EQUAL(byName[0]->GetFilename(), "rel.part02.rar");

	// (2) renamed target pairs by suffix key (unique within the donor window)
	std::vector<FileInfo*> bySuffix = DupeStreamRepair::SelectDonorCandidates(
		"other.part03.rar", 980, -1, 0, &donorNzb, DupeStreamRepair::MaxDonorCandidates);
	BOOST_REQUIRE(!bySuffix.empty());
	BOOST_CHECK_EQUAL(bySuffix[0]->GetFilename(), "rel.part03.rar");

	// (3) fully obfuscated names pair positionally by donor filename order,
	// but only when the window cardinality matches
	NzbInfo obfNzb;
	obfNzb.GetFileList()->Add(BuildDonorFile("ccc.bin", {500, 500}), false);
	obfNzb.GetFileList()->Add(BuildDonorFile("aaa.bin", {500, 500}), false);
	obfNzb.GetFileList()->Add(BuildDonorFile("bbb.bin", {500, 500}), false);
	std::vector<FileInfo*> byRank = DupeStreamRepair::SelectDonorCandidates(
		"zz.dat", 980, 1, 3, &obfNzb, DupeStreamRepair::MaxDonorCandidates);
	BOOST_REQUIRE(!byRank.empty());
	BOOST_CHECK_EQUAL(byRank[0]->GetFilename(), "bbb.bin");

	// cardinality mismatch disables the positional tier (still fills via size)
	std::vector<FileInfo*> badWindow = DupeStreamRepair::SelectDonorCandidates(
		"zz.dat", 980, 1, 4, &obfNzb, DupeStreamRepair::MaxDonorCandidates);
	BOOST_CHECK_EQUAL(badWindow.size(), 3u);

	// out-of-range rank is ignored gracefully
	std::vector<FileInfo*> badRank = DupeStreamRepair::SelectDonorCandidates(
		"zz.dat", 980, 99, 3, &obfNzb, DupeStreamRepair::MaxDonorCandidates);
	BOOST_CHECK_EQUAL(badRank.size(), 3u);

	// (4) the nfo never enters the window; the cap bounds the list
	for (FileInfo* candidate : byName)
	{
		BOOST_CHECK(strcasecmp(candidate->GetFilename(), "info.nfo") != 0);
	}
	BOOST_CHECK(byName.size() <= (size_t)DupeStreamRepair::MaxDonorCandidates);

	// a shared bare-extension key ("mkv") matches BOTH donor members - the
	// uniqueness gate keeps tier 2 out, so the closest-size donor comes first
	NzbInfo mkvNzb;
	mkvNzb.GetFileList()->Add(BuildDonorFile("aaa.mkv", {500, 500}), false);
	mkvNzb.GetFileList()->Add(BuildDonorFile("bbb.mkv", {490, 490}), false);
	std::vector<FileInfo*> bareExt = DupeStreamRepair::SelectDonorCandidates(
		"zzz.mkv", 980, -1, 0, &mkvNzb, DupeStreamRepair::MaxDonorCandidates);
	BOOST_REQUIRE_EQUAL(bareExt.size(), 2u);
	BOOST_CHECK_EQUAL(bareExt[0]->GetFilename(), "bbb.mkv");

	// digit-bearing shared extensions ("mp4") are equally ambiguous: the
	// uniqueness rule, not a character-class test, gates the suffix tier
	NzbInfo mp4Nzb;
	mp4Nzb.GetFileList()->Add(BuildDonorFile("ep1.mp4", {500, 500}), false);
	mp4Nzb.GetFileList()->Add(BuildDonorFile("ep2.mp4", {490, 490}), false);
	std::vector<FileInfo*> digitExt = DupeStreamRepair::SelectDonorCandidates(
		"zzz.mp4", 980, -1, 0, &mp4Nzb, DupeStreamRepair::MaxDonorCandidates);
	BOOST_REQUIRE_EQUAL(digitExt.size(), 2u);
	BOOST_CHECK_EQUAL(digitExt[0]->GetFilename(), "ep2.mp4");
}

BOOST_AUTO_TEST_CASE(StreamRepairObfuscatedSiblingCandidatesTest)
{
	// a dupe of sixteen equal-size volumes whose names say nothing: every
	// member is a candidate (probe verification picks the twin), and members
	// already proven identical to another target are never offered again
	NzbInfo donorNzb;
	for (int i = 0; i < 16; i++)
	{
		donorNzb.GetFileList()->Add(BuildDonorFile(
			("obf" + std::to_string(100 + i) + (char)('q' - i)).c_str(), {500, 500}), false);
	}

	std::vector<FileInfo*> all = DupeStreamRepair::SelectDonorCandidates(
		"zz.dat", 980, -1, 0, &donorNzb, DupeStreamRepair::MaxSiblingCandidates);
	BOOST_CHECK_EQUAL(all.size(), 16u);

	std::set<FileInfo*> claimed = {all[0], all[5]};
	std::vector<FileInfo*> rest = DupeStreamRepair::SelectDonorCandidates(
		"zz.dat", 980, -1, 0, &donorNzb, DupeStreamRepair::MaxSiblingCandidates, 0, &claimed);
	BOOST_CHECK_EQUAL(rest.size(), 14u);
	for (FileInfo* candidate : rest)
	{
		BOOST_CHECK(!claimed.count(candidate));
	}

	// a claimed member is skipped by the named tiers too
	FileInfo* named = all[3];
	std::set<FileInfo*> claimedNamed = {named};
	std::vector<FileInfo*> byName = DupeStreamRepair::SelectDonorCandidates(
		named->GetFilename(), 980, -1, 0, &donorNzb, DupeStreamRepair::MaxSiblingCandidates,
		0, &claimedNamed);
	BOOST_REQUIRE(!byName.empty());
	BOOST_CHECK(byName[0] != named);
}

BOOST_AUTO_TEST_CASE(StreamRepairRequiredCompareFloorTest)
{
	// large file, probes fully present: the full 16 KB floor stands
	StreamRangeList holes = {{0, 1000}};
	StreamRangeList donorRanges = {{0, 500000}, {500000, 500000}, {1000000, 500000}};
	std::vector<int> probes = {1, 2};
	BOOST_CHECK_EQUAL(DupeStreamRepair::RequiredCompareFloor(1500000, holes, donorRanges, probes),
		16 * 1024);

	// small file: floor scales to present bytes (80 KB file, 70 KB hole -> 10 KB)
	StreamRangeList parHoles = {{0, 70000}};
	StreamRangeList parRanges = {{0, 80000}};
	std::vector<int> parProbes = {0};
	BOOST_CHECK_EQUAL(DupeStreamRepair::RequiredCompareFloor(80000, parHoles, parRanges, parProbes),
		10000);

	// scattered present bytes: floor clamps to what the selected probes can
	// actually reach (present = 6000 across three slivers, probe 0 reaches 2000)
	StreamRangeList scatterHoles = {{2000, 8000}, {12000, 8000}, {22000, 8000}};
	StreamRangeList scatterRanges = {{0, 10000}, {10000, 10000}, {20000, 10000}};
	std::vector<int> oneProbe = {0};
	BOOST_CHECK_EQUAL(DupeStreamRepair::RequiredCompareFloor(30000, scatterHoles, scatterRanges, oneProbe),
		2000);

	// probes reach fewer than 64 present bytes: identity unknowable - the
	// unclamped base comes back so any achievable compare fails the floor
	StreamRangeList tinyHoles = {{60, 999940}};
	StreamRangeList tinyRanges = {{0, 500000}, {500000, 500000}};
	std::vector<int> tinyProbes = {0};
	BOOST_CHECK_EQUAL(DupeStreamRepair::RequiredCompareFloor(1000000, tinyHoles, tinyRanges, tinyProbes),
		64);
}

BOOST_AUTO_TEST_CASE(SelectExtractedInnerTest)
{
	fs::path tempDir = fs::temp_directory_path() / fs::make_unique_filename();
	fs::create_directories(tempDir);

	auto writeFile = [](const fs::path& path, int64 size)
	{
		std::ofstream out(fs::u8string(path), std::ios::binary);
		std::vector<char> data(size, 'x');
		out.write(data.data(), (std::streamsize)data.size());
	};

	// two extracted files share innerSize, one is a size-only distractor
	fs::path nameMatch = tempDir / "movie.mkv";
	fs::path otherSameSize = tempDir / "extracted.bin";
	fs::path wrongSize = tempDir / "readme.txt";
	writeFile(nameMatch, 1000);
	writeFile(otherSameSize, 1000);
	writeFile(wrongSize, 50);

	std::string dirStr = fs::u8string(tempDir);
	std::string nameMatchStr = fs::u8string(nameMatch);
	std::string otherSameSizeStr = fs::u8string(otherSameSize);

	// several same-size candidates: the (case-insensitive) basename match wins
	BOOST_CHECK_EQUAL(DupeStreamRepair::SelectExtractedInner(dirStr.c_str(), 1000, "MOVIE.MKV"),
		nameMatchStr);

	// no basename match among the size matches: falls back to the first path
	// in sorted order
	std::string expectedFirst = std::min(nameMatchStr, otherSameSizeStr);
	BOOST_CHECK_EQUAL(DupeStreamRepair::SelectExtractedInner(dirStr.c_str(), 1000, "nomatch.mkv"),
		expectedFirst);

	// no file of that size anywhere under dir: empty
	BOOST_CHECK_EQUAL(DupeStreamRepair::SelectExtractedInner(dirStr.c_str(), 999999, "movie.mkv"), "");

	// Any archive link fails closed. A hostile archive could otherwise point
	// at the target's own file and trivially "verify", and accepting other
	// members would make safety depend on extractor-specific link behavior.
	fs::path outsideDir = fs::temp_directory_path() / fs::make_unique_filename();
	fs::create_directories(outsideDir);
	fs::path outsideTarget = outsideDir / "secret.mkv";
	writeFile(outsideTarget, 1000);
	fs::error_code linkEc;
	fs::create_symlink(outsideTarget, tempDir / "escape.mkv", linkEc);
	if (!linkEc)	// platforms without symlink support skip this assertion
	{
		BOOST_CHECK_EQUAL(
			DupeStreamRepair::SelectExtractedInner(dirStr.c_str(), 1000, "escape.mkv"), "");
	}
	fs::error_code outEc;
	fs::remove_all(outsideDir, outEc);

	fs::error_code ec;
	fs::remove_all(tempDir, ec);
}

BOOST_AUTO_TEST_CASE(DonorPostingIdentityPreservesRepairAlternativesTest)
{
	NzbInfo first;
	NzbInfo copy;
	first.SetQueuedFilename("release.nzb.2.queued");
	copy.SetQueuedFilename("release.nzb.8.queued");
	first.GetFileList()->Add(BuildDonorFile("movie.7z", {500, 500}), false);
	copy.GetFileList()->Add(BuildDonorFile("movie.7z", {500, 500}), false);
	const std::string original = DupeStreamRepair::BuildDonorKey(&first, "");
	BOOST_CHECK_EQUAL(original, DupeStreamRepair::BuildDonorKey(&copy, ""));

	// The same posting with corrected archive credentials remains useful.
	BOOST_CHECK(original != DupeStreamRepair::BuildDonorKey(&copy, "corrected-password"));
	FileInfo* member = copy.GetFileList()->front().get();
	member->GetGroups()->emplace_back("alt.binaries.other");
	BOOST_CHECK(original != DupeStreamRepair::BuildDonorKey(&copy, ""));
	member->GetGroups()->clear();
	BOOST_CHECK_EQUAL(original, DupeStreamRepair::BuildDonorKey(&copy, ""));

	// A repost with a new article ID must still be tried for missing bytes.
	member->GetArticles()->front()->SetMessageId("replacement@example.com");
	BOOST_CHECK(original != DupeStreamRepair::BuildDonorKey(&copy, ""));
}

BOOST_AUTO_TEST_CASE(ExceedsDecompressCapTest)
{
	const int64 Max = DupeStreamRepair::MaxDecompressBytes;

	// exactly at the cap via accumulated decoded bytes: not exceeded
	BOOST_CHECK(!DupeStreamRepair::ExceedsDecompressCap(Max - 100, 100, 0, 0));
	// one byte over via accumulated decoded bytes: exceeded
	BOOST_CHECK(DupeStreamRepair::ExceedsDecompressCap(Max - 99, 100, 0, 0));

	// exactly at the cap via accumulated file extent: not exceeded
	BOOST_CHECK(!DupeStreamRepair::ExceedsDecompressCap(0, 0, Max - 50, 50));
	// one byte over via accumulated file extent: exceeded
	BOOST_CHECK(DupeStreamRepair::ExceedsDecompressCap(0, 0, Max - 49, 50));

	// already at the cap: even a 1-byte addition on either dimension trips it
	BOOST_CHECK(DupeStreamRepair::ExceedsDecompressCap(Max, 1, 0, 0));
	BOOST_CHECK(DupeStreamRepair::ExceedsDecompressCap(0, 0, Max, 1));

	// nowhere near either bound: not exceeded
	BOOST_CHECK(!DupeStreamRepair::ExceedsDecompressCap(0, 1024, 0, 1024));
}

BOOST_AUTO_TEST_SUITE_END()
