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

BOOST_AUTO_TEST_SUITE_END()
