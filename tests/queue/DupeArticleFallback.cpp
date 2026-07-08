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
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "nzbget.h"

#include <boost/test/unit_test.hpp>
#include "DownloadInfo.h"
#include "DupeArticleFallback.h"

BOOST_AUTO_TEST_SUITE(QueueTest)

namespace
{

std::unique_ptr<FileInfo> BuildFile(const char* filename,
	std::vector<std::pair<int, int>> parts, const char* msgIdPrefix)
{
	std::unique_ptr<FileInfo> fileInfo = std::make_unique<FileInfo>();
	fileInfo->SetFilename(filename);
	int64 size = 0;
	for (std::pair<int, int>& part : parts)
	{
		std::unique_ptr<ArticleInfo> article = std::make_unique<ArticleInfo>();
		article->SetPartNumber(part.first);
		article->SetSize(part.second);
		article->SetMessageId(BString<1024>("%s-%i@example.com", msgIdPrefix, part.first));
		size += part.second;
		fileInfo->GetArticles()->push_back(std::move(article));
	}
	fileInfo->SetSize(size);
	fileInfo->SetTotalArticles((int)parts.size());
	return fileInfo;
}

void AddDonorFile(NzbInfo* donorNzb, const char* filename,
	std::vector<std::pair<int, int>> parts, const char* msgIdPrefix)
{
	donorNzb->GetFileList()->Add(BuildFile(filename, std::move(parts), msgIdPrefix), false);
}

} // namespace

BOOST_AUTO_TEST_CASE(DupeArticleFallbackSizesMatchTest)
{
	BOOST_CHECK(DupeArticleFallback::SizesMatch(100000, 100000, 64));
	BOOST_CHECK(DupeArticleFallback::SizesMatch(100000, 101000, 64)); // 1.0% < 1/64
	BOOST_CHECK(!DupeArticleFallback::SizesMatch(100000, 103000, 64)); // 3.0% > 1/64
	BOOST_CHECK(DupeArticleFallback::SizesMatch(100000, 105000, 16)); // 5.0% < 1/16
	BOOST_CHECK(!DupeArticleFallback::SizesMatch(100000, 110000, 16)); // 10% > 1/16
	BOOST_CHECK(DupeArticleFallback::SizesMatch(0, 0, 64));
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackFilenameMatchTest)
{
	std::unique_ptr<FileInfo> target = BuildFile("release.r01",
		{{1, 500000}, {2, 500000}, {3, 120000}}, "orig");

	NzbInfo donorNzb;
	AddDonorFile(&donorNzb, "release.r00", {{1, 500000}, {2, 500000}, {3, 120000}}, "donor0");
	AddDonorFile(&donorNzb, "Release.R01", {{1, 500200}, {2, 500200}, {3, 120100}}, "donor1");

	FileInfo* match = DupeArticleFallback::MatchDonorFile(target.get(), &donorNzb);
	BOOST_REQUIRE(match);
	// filename match is case-insensitive
	BOOST_CHECK_EQUAL(match->GetFilename(), "Release.R01");
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackStructuralMatchTest)
{
	// donor uses obfuscated names; exactly one file matches structurally
	std::unique_ptr<FileInfo> target = BuildFile("release.r01",
		{{1, 500000}, {2, 500000}, {3, 120000}}, "orig");

	NzbInfo donorNzb;
	AddDonorFile(&donorNzb, "adf7e6b1.bin", {{1, 500100}, {2, 500100}, {3, 120050}}, "donor0");
	AddDonorFile(&donorNzb, "b23f98c4.bin", {{1, 500000}, {2, 300000}}, "donor1");

	FileInfo* match = DupeArticleFallback::MatchDonorFile(target.get(), &donorNzb);
	BOOST_REQUIRE(match);
	BOOST_CHECK_EQUAL(match->GetFilename(), "adf7e6b1.bin");
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackAmbiguousStructuralMatchTest)
{
	// two donor files pass the structural gates and neither matches by name:
	// fail closed
	std::unique_ptr<FileInfo> target = BuildFile("release.r01",
		{{1, 500000}, {2, 500000}}, "orig");

	NzbInfo donorNzb;
	AddDonorFile(&donorNzb, "adf7e6b1.bin", {{1, 500000}, {2, 500000}}, "donor0");
	AddDonorFile(&donorNzb, "b23f98c4.bin", {{1, 500000}, {2, 500000}}, "donor1");

	BOOST_CHECK(DupeArticleFallback::MatchDonorFile(target.get(), &donorNzb) == nullptr);
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackPartCountMismatchTest)
{
	std::unique_ptr<FileInfo> target = BuildFile("release.r01",
		{{1, 500000}, {2, 500000}}, "orig");

	NzbInfo donorNzb;
	AddDonorFile(&donorNzb, "release.r01", {{1, 400000}, {2, 400000}, {3, 200000}}, "donor0");

	BOOST_CHECK(DupeArticleFallback::MatchDonorFile(target.get(), &donorNzb) == nullptr);
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackTotalSizeMismatchTest)
{
	// same part count but total size differs by ~5% (> 1/64)
	std::unique_ptr<FileInfo> target = BuildFile("release.r01",
		{{1, 500000}, {2, 500000}}, "orig");

	NzbInfo donorNzb;
	AddDonorFile(&donorNzb, "release.r01", {{1, 525000}, {2, 525000}}, "donor0");

	BOOST_CHECK(DupeArticleFallback::MatchDonorFile(target.get(), &donorNzb) == nullptr);
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackPartSizeMismatchTest)
{
	// totals are equal but the per-part split differs by more than 1/16
	std::unique_ptr<FileInfo> target = BuildFile("release.r01",
		{{1, 100000}, {2, 100000}}, "orig");

	NzbInfo donorNzb;
	AddDonorFile(&donorNzb, "release.r01", {{1, 90000}, {2, 110000}}, "donor0");

	BOOST_CHECK(DupeArticleFallback::MatchDonorFile(target.get(), &donorNzb) == nullptr);
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackFindDonorMessageIdTest)
{
	std::unique_ptr<FileInfo> donorFile = BuildFile("release.r01",
		{{1, 500000}, {3, 120000}}, "donor");

	const char* msgId = DupeArticleFallback::FindDonorMessageId(donorFile.get(), 3);
	BOOST_REQUIRE(msgId);
	BOOST_CHECK_EQUAL(msgId, "donor-3@example.com");

	// part 2 is missing in the donor collection
	BOOST_CHECK(DupeArticleFallback::FindDonorMessageId(donorFile.get(), 2) == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
