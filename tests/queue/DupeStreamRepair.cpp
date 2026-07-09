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

#include <boost/test/unit_test.hpp>
#include "DownloadInfo.h"
#include "DupeStreamRepair.h"
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

		// M1: ANY file type is captured - a byte-identical repost can donate
		// to rar volumes (passworded or compressed) and par2 files alike
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

	// (2) renamed target pairs by member-specific suffix key
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

	// bare-extension keys ("mkv") are not member-specific: tier 2 must NOT
	// fire, so the closest-size donor comes first, not the file-list order
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

BOOST_AUTO_TEST_SUITE_END()
