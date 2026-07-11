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

BOOST_AUTO_TEST_CASE(DupeArticleFallbackAmbiguousFilenameMatchTest)
{
	std::unique_ptr<FileInfo> target = BuildFile("release.r01",
		{{1, 500000}, {2, 500000}}, "orig");

	NzbInfo donorNzb;
	// Two structurally compatible files with the same filename must fail
	// closed; selecting the first one would make the result depend on NZB order.
	AddDonorFile(&donorNzb, "release.r01", {{1, 500000}, {2, 500000}}, "donor0");
	AddDonorFile(&donorNzb, "RELEASE.R01", {{1, 500000}, {2, 500000}}, "donor1");

	BOOST_CHECK(DupeArticleFallback::MatchDonorFile(target.get(), &donorNzb) == nullptr);
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

BOOST_AUTO_TEST_CASE(DupeArticleFallbackCandidateListStableAcrossRoundsTest)
{
	// the pinned source list is built from these candidates; the builder must
	// be insensitive to which candidate is currently assigned to the article,
	// otherwise a re-pin (round-0 retry after an empty first pin) could skip
	// untried donors (regression test)
	std::unique_ptr<FileInfo> target = BuildFile("release.r01",
		{{1, 500000}, {2, 500000}}, "orig");

	NzbInfo donor0Nzb;
	AddDonorFile(&donor0Nzb, "release.r01", {{1, 500000}, {2, 500000}}, "donor0");
	NzbInfo donor1Nzb;
	AddDonorFile(&donor1Nzb, "release.r01", {{1, 500100}, {2, 500100}}, "donor1");

	std::vector<NzbInfo*> parsedDonors = {&donor0Nzb, &donor1Nzb};

	std::vector<CString> round0 = DupeArticleFallback::BuildCandidateMessageIds(parsedDonors, target.get(), 2);
	BOOST_REQUIRE_EQUAL(round0.size(), 2u);
	BOOST_CHECK_EQUAL(*round0[0], "donor0-2@example.com");
	BOOST_CHECK_EQUAL(*round0[1], "donor1-2@example.com");

	// simulate round 1: the article now carries donor0's message-id
	target->GetArticles()->at(1)->SetMessageId("donor0-2@example.com");

	std::vector<CString> round1 = DupeArticleFallback::BuildCandidateMessageIds(parsedDonors, target.get(), 2);
	BOOST_REQUIRE_EQUAL(round1.size(), 2u);
	BOOST_CHECK_EQUAL(*round1[0], "donor0-2@example.com");
	BOOST_CHECK_EQUAL(*round1[1], "donor1-2@example.com");
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackCandidateListDedupeTest)
{
	// two donor nzbs listing the same posting contribute one candidate
	std::unique_ptr<FileInfo> target = BuildFile("release.r01",
		{{1, 500000}, {2, 500000}}, "orig");

	NzbInfo donor0Nzb;
	AddDonorFile(&donor0Nzb, "release.r01", {{1, 500000}, {2, 500000}}, "shared");
	NzbInfo donor1Nzb;
	AddDonorFile(&donor1Nzb, "release.r01", {{1, 500000}, {2, 500000}}, "shared");

	std::vector<NzbInfo*> parsedDonors = {&donor0Nzb, &donor1Nzb};

	std::vector<CString> candidates = DupeArticleFallback::BuildCandidateMessageIds(parsedDonors, target.get(), 1);
	BOOST_REQUIRE_EQUAL(candidates.size(), 1u);
	BOOST_CHECK_EQUAL(*candidates[0], "shared-1@example.com");
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackOrderSourcesReactiveTest)
{
	// not cut over: donor candidates only (the primary was already tried normally)
	std::vector<CString> donors;
	donors.emplace_back("d0@example.com");
	donors.emplace_back("d1@example.com");

	std::vector<CString> s = DupeArticleFallback::OrderSources(donors, false, "primary@example.com");
	BOOST_REQUIRE_EQUAL(s.size(), 2u);
	BOOST_CHECK_EQUAL(*s[0], "d0@example.com");
	BOOST_CHECK_EQUAL(*s[1], "d1@example.com");
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackOrderSourcesCutoverTest)
{
	// cut over: lead with the top donor, then the remaining donors, then the
	// primary as the final fallback (the file cut over because the primary is
	// heavily holed - other duplicates are better bets than a primary sweep)
	std::vector<CString> donors;
	donors.emplace_back("d0@example.com");
	donors.emplace_back("d1@example.com");
	donors.emplace_back("d2@example.com");

	std::vector<CString> s = DupeArticleFallback::OrderSources(donors, true, "primary@example.com");
	BOOST_REQUIRE_EQUAL(s.size(), 4u);
	BOOST_CHECK_EQUAL(*s[0], "d0@example.com");      // lead with duplicate
	BOOST_CHECK_EQUAL(*s[1], "d1@example.com");
	BOOST_CHECK_EQUAL(*s[2], "d2@example.com");
	BOOST_CHECK_EQUAL(*s[3], "primary@example.com"); // primary is the last resort
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackOrderSourcesCutoverNoDonorTest)
{
	// cut over but no donor has this part: empty, so the article just uses its
	// own (primary) message-id via the normal path
	std::vector<CString> donors;
	std::vector<CString> s = DupeArticleFallback::OrderSources(donors, true, "primary@example.com");
	BOOST_CHECK(s.empty());
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackRotateToLeadTest)
{
	NzbInfo donorA, donorB, donorC;
	donorA.SetId(11);
	donorB.SetId(21);
	donorC.SetId(31);
	RawNzbList donors = {&donorA, &donorB, &donorC};

	// the lead donor comes first, the order of the others is preserved
	DupeArticleFallback::RotateToLead(donors, 21);
	BOOST_CHECK_EQUAL(donors[0]->GetId(), 21);
	BOOST_CHECK_EQUAL(donors[1]->GetId(), 31);
	BOOST_CHECK_EQUAL(donors[2]->GetId(), 11);

	// unset (0) or vanished lead: order unchanged
	DupeArticleFallback::RotateToLead(donors, 0);
	BOOST_CHECK_EQUAL(donors[0]->GetId(), 21);
	DupeArticleFallback::RotateToLead(donors, 99);
	BOOST_CHECK_EQUAL(donors[0]->GetId(), 21);
}

namespace
{

std::vector<CString> Candidates(std::initializer_list<const char*> ids)
{
	std::vector<CString> candidates;
	for (const char* id : ids)
	{
		candidates.emplace_back(id);
	}
	return candidates;
}

// an article that just failed its lead-donor attempt: fallback round 1, its
// pinned slot-0 donor being the file's current lead
ArticleInfo* FailedLeadArticle(FileInfo* fileInfo, int index, int nextLead, int donorCount)
{
	ArticleInfo* article = fileInfo->GetArticles()->at(index).get();
	article->SetDupeFallbackRound(1);
	article->SetDupeLeadSnapshot(fileInfo->GetDupeLeadDonorId());
	article->SetDupeNextLead(nextLead);
	article->SetDupeDonorCount(donorCount);
	return article;
}

} // namespace

BOOST_AUTO_TEST_CASE(DupeArticleFallbackFinishPinTest)
{
	std::unique_ptr<FileInfo> file = BuildFile("release.r01",
		{{1, 1000}, {2, 1000}}, "orig");
	ArticleInfo* article = file->GetArticles()->at(0).get();

	// two candidates from donor 11 (a spanned posting), one from donor 21:
	// slot 0 belongs to donor 11, a demotion would rotate to donor 21
	std::vector<CString> candidates = Candidates(
		{"a1@example.com", "b1@example.com", "a2@example.com"});
	std::vector<int> contributors = {11, 21, 11};

	DupeArticleFallback::FinishPin(file.get(), article, candidates, contributors,
		false, "primary@example.com");

	BOOST_CHECK_EQUAL(article->GetDupeLeadSnapshot(), 11);
	BOOST_CHECK_EQUAL(article->GetDupeNextLead(), 21);
	BOOST_CHECK_EQUAL(article->GetDupeDonorCount(), 2);
	// the file's lead is decided by its first pinned article
	BOOST_CHECK_EQUAL(file->GetDupeLeadDonorId(), 11);
	BOOST_REQUIRE_EQUAL(article->GetDupeSources()->size(), 3u);
	BOOST_CHECK_EQUAL(*article->GetDupeSources()->at(0), "a1@example.com");

	// cut over: the pinned order carries the primary revert at the last slot
	ArticleInfo* cutoverArticle = file->GetArticles()->at(1).get();
	DupeArticleFallback::FinishPin(file.get(), cutoverArticle, candidates, contributors,
		true, "primary@example.com");
	BOOST_REQUIRE_EQUAL(cutoverArticle->GetDupeSources()->size(), 4u);
	BOOST_CHECK_EQUAL(*cutoverArticle->GetDupeSources()->at(0), "a1@example.com");
	BOOST_CHECK_EQUAL(*cutoverArticle->GetDupeSources()->at(1), "b1@example.com");
	BOOST_CHECK_EQUAL(*cutoverArticle->GetDupeSources()->at(2), "a2@example.com");
	BOOST_CHECK_EQUAL(*cutoverArticle->GetDupeSources()->at(3), "primary@example.com");
	// an already decided file lead is not overwritten
	BOOST_CHECK_EQUAL(file->GetDupeLeadDonorId(), 11);
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackFinishPinNoDonorsTest)
{
	std::unique_ptr<FileInfo> file = BuildFile("release.r01", {{1, 1000}}, "orig");
	ArticleInfo* article = file->GetArticles()->at(0).get();

	DupeArticleFallback::FinishPin(file.get(), article, {}, {}, true, "primary@example.com");

	BOOST_CHECK_EQUAL(article->GetDupeLeadSnapshot(), 0);
	BOOST_CHECK_EQUAL(article->GetDupeNextLead(), 0);
	BOOST_CHECK_EQUAL(article->GetDupeDonorCount(), 0);
	BOOST_CHECK_EQUAL(file->GetDupeLeadDonorId(), 0);
	BOOST_CHECK(article->GetDupeSources()->empty());
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackLeadDemotionTest)
{
	std::unique_ptr<FileInfo> file = BuildFile("release.r01",
		{{1, 1000}, {2, 1000}, {3, 1000}, {4, 1000}}, "orig");
	file->SetDupeLeadDonorId(11);

	// two consecutive lead misses: streak accumulates, no demotion yet
	BOOST_CHECK(!DupeArticleFallback::RegisterLeadFailure(file.get(), FailedLeadArticle(file.get(), 0, 21, 2)));
	BOOST_CHECK(!DupeArticleFallback::RegisterLeadFailure(file.get(), FailedLeadArticle(file.get(), 1, 21, 2)));
	BOOST_CHECK_EQUAL(file->GetDupeLeadDonorId(), 11);
	BOOST_CHECK_EQUAL(file->GetDupeLeadFailures(), 2);

	// a failure at a non-lead round is not a lead miss
	ArticleInfo* laterRound = FailedLeadArticle(file.get(), 2, 21, 2);
	laterRound->SetDupeFallbackRound(2);
	BOOST_CHECK(!DupeArticleFallback::RegisterLeadFailure(file.get(), laterRound));
	BOOST_CHECK_EQUAL(file->GetDupeLeadFailures(), 2);

	// third consecutive lead miss: the lead rotates to the article's next
	// donor and the streak resets
	BOOST_CHECK(DupeArticleFallback::RegisterLeadFailure(file.get(), FailedLeadArticle(file.get(), 3, 21, 2)));
	BOOST_CHECK_EQUAL(file->GetDupeLeadDonorId(), 21);
	BOOST_CHECK_EQUAL(file->GetDupeLeadFailures(), 0);
	BOOST_CHECK_EQUAL(file->GetDupeLeadSwitches(), 1);
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackLeadSuccessResetsStreakTest)
{
	std::unique_ptr<FileInfo> file = BuildFile("release.r01",
		{{1, 1000}, {2, 1000}, {3, 1000}, {4, 1000}, {5, 1000}}, "orig");
	file->SetDupeLeadDonorId(11);

	BOOST_CHECK(!DupeArticleFallback::RegisterLeadFailure(file.get(), FailedLeadArticle(file.get(), 0, 21, 2)));
	BOOST_CHECK(!DupeArticleFallback::RegisterLeadFailure(file.get(), FailedLeadArticle(file.get(), 1, 21, 2)));

	// a successful fetch from the lead ends the streak: the misses are not
	// consecutive, so the lead is not demoted
	DupeArticleFallback::RegisterLeadSuccess(file.get(), FailedLeadArticle(file.get(), 2, 21, 2));
	BOOST_CHECK_EQUAL(file->GetDupeLeadFailures(), 0);

	BOOST_CHECK(!DupeArticleFallback::RegisterLeadFailure(file.get(), FailedLeadArticle(file.get(), 3, 21, 2)));
	BOOST_CHECK(!DupeArticleFallback::RegisterLeadFailure(file.get(), FailedLeadArticle(file.get(), 4, 21, 2)));
	BOOST_CHECK_EQUAL(file->GetDupeLeadDonorId(), 11);
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackLeadStaleSnapshotTest)
{
	std::unique_ptr<FileInfo> file = BuildFile("release.r01",
		{{1, 1000}, {2, 1000}, {3, 1000}, {4, 1000}, {5, 1000}, {6, 1000}}, "orig");
	file->SetDupeLeadDonorId(11);

	// pin two articles to lead 11, then demote it: their late failures are
	// failures of the OLD lead and must not count against the new lead
	// (otherwise a burst of in-flight failures would cascade-demote through
	// donors that were never tried)
	ArticleInfo* stale1 = FailedLeadArticle(file.get(), 0, 21, 2);
	ArticleInfo* stale2 = FailedLeadArticle(file.get(), 1, 21, 2);

	BOOST_CHECK(!DupeArticleFallback::RegisterLeadFailure(file.get(), FailedLeadArticle(file.get(), 2, 21, 2)));
	BOOST_CHECK(!DupeArticleFallback::RegisterLeadFailure(file.get(), FailedLeadArticle(file.get(), 3, 21, 2)));
	BOOST_CHECK(DupeArticleFallback::RegisterLeadFailure(file.get(), FailedLeadArticle(file.get(), 4, 21, 2)));
	BOOST_CHECK_EQUAL(file->GetDupeLeadDonorId(), 21);

	BOOST_CHECK(!DupeArticleFallback::RegisterLeadFailure(file.get(), stale1));
	BOOST_CHECK(!DupeArticleFallback::RegisterLeadFailure(file.get(), stale2));
	BOOST_CHECK_EQUAL(file->GetDupeLeadFailures(), 0);
	BOOST_CHECK_EQUAL(file->GetDupeLeadDonorId(), 21);

	// a stale success must not clear the new lead's streak either
	BOOST_CHECK(!DupeArticleFallback::RegisterLeadFailure(file.get(), FailedLeadArticle(file.get(), 5, 11, 2)));
	BOOST_CHECK_EQUAL(file->GetDupeLeadFailures(), 1);
	DupeArticleFallback::RegisterLeadSuccess(file.get(), stale1);
	BOOST_CHECK_EQUAL(file->GetDupeLeadFailures(), 1);
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackLeadSingleDonorNoDemotionTest)
{
	// with a single duplicate there is nothing to rotate to: the streak
	// accumulates but the lead stays put and nothing is logged
	std::unique_ptr<FileInfo> file = BuildFile("release.r01",
		{{1, 1000}, {2, 1000}, {3, 1000}, {4, 1000}}, "orig");
	file->SetDupeLeadDonorId(11);

	BOOST_CHECK(!DupeArticleFallback::RegisterLeadFailure(file.get(), FailedLeadArticle(file.get(), 0, 0, 1)));
	BOOST_CHECK(!DupeArticleFallback::RegisterLeadFailure(file.get(), FailedLeadArticle(file.get(), 1, 0, 1)));
	BOOST_CHECK(!DupeArticleFallback::RegisterLeadFailure(file.get(), FailedLeadArticle(file.get(), 2, 0, 1)));
	BOOST_CHECK_EQUAL(file->GetDupeLeadDonorId(), 11);

	// a second duplicate appearing on a later article makes the accumulated
	// streak act
	BOOST_CHECK(DupeArticleFallback::RegisterLeadFailure(file.get(), FailedLeadArticle(file.get(), 3, 21, 2)));
	BOOST_CHECK_EQUAL(file->GetDupeLeadDonorId(), 21);
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackLeadRotationBoundedTest)
{
	// duplicates that are ALL holed: the lead rotates until every donor has
	// led once, then freezes (further rotation and logging would be noise);
	// a lead success re-arms the rotation budget
	std::unique_ptr<FileInfo> file = BuildFile("release.r01",
		{{1, 1000}, {2, 1000}, {3, 1000}, {4, 1000}, {5, 1000}, {6, 1000},
		 {7, 1000}, {8, 1000}, {9, 1000}, {10, 1000}, {11, 1000}, {12, 1000},
		 {13, 1000}}, "orig");
	file->SetDupeLeadDonorId(11);

	auto missStreak = [&](int firstIndex, int nextLead)
	{
		BOOST_CHECK(!DupeArticleFallback::RegisterLeadFailure(file.get(), FailedLeadArticle(file.get(), firstIndex, nextLead, 2)));
		BOOST_CHECK(!DupeArticleFallback::RegisterLeadFailure(file.get(), FailedLeadArticle(file.get(), firstIndex + 1, nextLead, 2)));
		return DupeArticleFallback::RegisterLeadFailure(file.get(), FailedLeadArticle(file.get(), firstIndex + 2, nextLead, 2));
	};

	// switch 1: 11 -> 21; switch 2: 21 -> 11 (the full two-donor cycle)
	BOOST_CHECK(missStreak(0, 21));
	BOOST_CHECK_EQUAL(file->GetDupeLeadDonorId(), 21);
	BOOST_CHECK(missStreak(3, 11));
	BOOST_CHECK_EQUAL(file->GetDupeLeadDonorId(), 11);
	BOOST_CHECK_EQUAL(file->GetDupeLeadSwitches(), 2);

	// the budget is exhausted: misses keep counting but the lead stays put
	BOOST_CHECK(!missStreak(6, 21));
	BOOST_CHECK_EQUAL(file->GetDupeLeadDonorId(), 11);

	// a PROVISIONAL success (decoded neighbours unknown, the alignment check
	// passed vacuously - the article may be demoted again later) ends the
	// streak but must NOT re-arm the budget: a systematically drifted lead
	// keeps producing provisional accepts, and letting them re-arm would
	// bypass the rotation bound
	DupeArticleFallback::RegisterLeadSuccess(file.get(), FailedLeadArticle(file.get(), 9, 21, 2));
	BOOST_CHECK_EQUAL(file->GetDupeLeadFailures(), 0);
	BOOST_CHECK_EQUAL(file->GetDupeLeadSwitches(), 2);
	BOOST_CHECK(!missStreak(10, 21));

	// a VERIFIABLE lead success (decoded boundaries pinned by finished
	// neighbours) re-arms the budget
	auto finish = [&](int index, int64 offset, int size)
	{
		ArticleInfo* neighbour = file->GetArticles()->at(index).get();
		neighbour->SetStatus(ArticleInfo::aiFinished);
		neighbour->SetSegmentOffset(offset);
		neighbour->SetSegmentSize(size);
	};
	finish(8, 8000, 1000);
	finish(10, 10000, 1000);
	DupeArticleFallback::RegisterLeadSuccess(file.get(), FailedLeadArticle(file.get(), 9, 21, 2));
	BOOST_CHECK_EQUAL(file->GetDupeLeadSwitches(), 0);
	BOOST_CHECK(missStreak(3, 21));
	BOOST_CHECK_EQUAL(file->GetDupeLeadDonorId(), 21);
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackVacateGhostLeadTest)
{
	// a lead donor deleted from queue/history can never match a collected
	// donor again (nzb-ids are not reused): without vacating, the snapshot
	// gate would block all lead accounting for the rest of the file
	std::unique_ptr<FileInfo> file = BuildFile("release.r01", {{1, 1000}}, "orig");

	NzbInfo donorA, donorB;
	donorA.SetId(11);
	donorB.SetId(21);
	RawNzbList donors = {&donorA, &donorB};

	// live lead: nothing changes
	file->SetDupeLeadDonorId(21);
	file->SetDupeLeadFailures(2);
	DupeArticleFallback::VacateGhostLead(file.get(), donors);
	BOOST_CHECK_EQUAL(file->GetDupeLeadDonorId(), 21);
	BOOST_CHECK_EQUAL(file->GetDupeLeadFailures(), 2);

	// ghost lead: vacated, the accounting starts clean and the next pin
	// re-decides the lead
	file->SetDupeLeadDonorId(99);
	file->SetDupeLeadSwitches(2);
	DupeArticleFallback::VacateGhostLead(file.get(), donors);
	BOOST_CHECK_EQUAL(file->GetDupeLeadDonorId(), 0);
	BOOST_CHECK_EQUAL(file->GetDupeLeadFailures(), 0);
	BOOST_CHECK_EQUAL(file->GetDupeLeadSwitches(), 0);
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

namespace
{

// decoded geometry of a downloaded article: [offset, offset + size)
void FinishArticle(FileInfo* fileInfo, int index, int64 offset, int size)
{
	ArticleInfo* article = fileInfo->GetArticles()->at(index).get();
	article->SetStatus(ArticleInfo::aiFinished);
	article->SetSegmentOffset(offset);
	article->SetSegmentSize(size);
}

} // namespace

BOOST_AUTO_TEST_CASE(DupeArticleFallbackExpectedSegmentOffsetTest)
{
	// three articles, decoded tiling 0..1000..2000..2500
	std::unique_ptr<FileInfo> target = BuildFile("release.r01",
		{{1, 1300}, {2, 1300}, {3, 700}}, "orig");
	ArticleInfo* first = target->GetArticles()->at(0).get();
	ArticleInfo* middle = target->GetArticles()->at(1).get();
	ArticleInfo* last = target->GetArticles()->at(2).get();

	// the first article of a file always begins at decoded offset 0
	BOOST_CHECK_EQUAL(DupeArticleFallback::ExpectedSegmentOffset(target.get(), first), 0);

	// no finished neighbours yet: interior boundaries are unknown
	BOOST_CHECK_EQUAL(DupeArticleFallback::ExpectedSegmentOffset(target.get(), middle), -1);
	BOOST_CHECK_EQUAL(DupeArticleFallback::ExpectedSegmentEnd(target.get(), middle), -1);
	BOOST_CHECK_EQUAL(DupeArticleFallback::ExpectedSegmentEnd(target.get(), last), -1);

	FinishArticle(target.get(), 0, 0, 1000);
	FinishArticle(target.get(), 2, 2000, 500);
	target->SetDecodedFileSize(2500);

	// interior article must begin where the finished predecessor ends and end
	// where the finished successor begins
	BOOST_CHECK_EQUAL(DupeArticleFallback::ExpectedSegmentOffset(target.get(), middle), 1000);
	BOOST_CHECK_EQUAL(DupeArticleFallback::ExpectedSegmentEnd(target.get(), middle), 2000);

	// the last article must end at the decoded file size
	BOOST_CHECK_EQUAL(DupeArticleFallback::ExpectedSegmentEnd(target.get(), last), 2500);

	// the predecessor of the last article is not finished: its begin is unknown
	BOOST_CHECK_EQUAL(DupeArticleFallback::ExpectedSegmentOffset(target.get(), last), -1);
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackSegmentAlignedTest)
{
	// target decoded tiling: 0..1000..2000..2500; the middle article was
	// substituted from a duplicate and its decoded placement must exactly
	// occupy the slot between the finished neighbours
	std::unique_ptr<FileInfo> target = BuildFile("release.r01",
		{{1, 1300}, {2, 1300}, {3, 700}}, "orig");
	ArticleInfo* middle = target->GetArticles()->at(1).get();

	FinishArticle(target.get(), 0, 0, 1000);
	FinishArticle(target.get(), 2, 2000, 500);
	target->SetDecodedFileSize(2500);

	// aligned donor: same decoded boundaries as the target article
	FinishArticle(target.get(), 1, 1000, 1000);
	BOOST_CHECK(DupeArticleFallback::SegmentAligned(target.get(), middle));

	// drifted donor: begins 40 bytes late - would leave a zero-filled gap
	// after part 1 and overwrite 40 bytes of part 3 (passes the encoded-size
	// structural gate: the drift is far below the 1/16 per-part tolerance)
	FinishArticle(target.get(), 1, 1040, 1000);
	BOOST_CHECK(!DupeArticleFallback::SegmentAligned(target.get(), middle));

	// drifted donor: begins early - would overwrite the tail of part 1
	FinishArticle(target.get(), 1, 960, 1000);
	BOOST_CHECK(!DupeArticleFallback::SegmentAligned(target.get(), middle));

	// begin abuts but the donor part is short: zero-filled gap before part 3
	FinishArticle(target.get(), 1, 1000, 960);
	BOOST_CHECK(!DupeArticleFallback::SegmentAligned(target.get(), middle));
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackSegmentAlignedUnknownNeighborsTest)
{
	// neighbours not finished and file size unknown: nothing to validate
	// against, the article is accepted provisionally (the completion-time
	// re-validation of neighbours catches it once the geometry is known)
	{
		std::unique_ptr<FileInfo> target = BuildFile("release.r01",
			{{1, 1300}, {2, 1300}, {3, 700}}, "orig");
		ArticleInfo* middle = target->GetArticles()->at(1).get();
		FinishArticle(target.get(), 1, 1040, 1000);
		BOOST_CHECK(DupeArticleFallback::SegmentAligned(target.get(), middle));

		// an article without recorded decoded placement cannot be validated
		middle->SetSegmentSize(0);
		BOOST_CHECK(DupeArticleFallback::SegmentAligned(target.get(), middle));
	}

	// the first article is constrained even without neighbours: offset 0
	{
		std::unique_ptr<FileInfo> target = BuildFile("release.r01",
			{{1, 1300}, {2, 1300}, {3, 700}}, "orig");
		ArticleInfo* first = target->GetArticles()->at(0).get();
		FinishArticle(target.get(), 0, 40, 1000);
		BOOST_CHECK(!DupeArticleFallback::SegmentAligned(target.get(), first));
		FinishArticle(target.get(), 0, 0, 1000);
		BOOST_CHECK(DupeArticleFallback::SegmentAligned(target.get(), first));
	}

	// the last article is constrained by the decoded file size
	{
		std::unique_ptr<FileInfo> target = BuildFile("release.r01",
			{{1, 1300}, {2, 1300}, {3, 700}}, "orig");
		ArticleInfo* last = target->GetArticles()->at(2).get();
		target->SetDecodedFileSize(2500);
		FinishArticle(target.get(), 2, 1960, 500);
		BOOST_CHECK(!DupeArticleFallback::SegmentAligned(target.get(), last));
		FinishArticle(target.get(), 2, 2000, 500);
		BOOST_CHECK(DupeArticleFallback::SegmentAligned(target.get(), last));
	}
}

BOOST_AUTO_TEST_CASE(DupeArticleFallbackFirstUntiledArticleTest)
{
	// Whole-file completion-time check, independent of the fallback round: this
	// is what catches a drifted donor that survived a restart (which reloads it
	// as a plain finished article with round 0, so the round-gated backstops no
	// longer recognise it). Simulate a fully-loaded completed file.

	// aligned: every part tiles 0..1000..2000..2500 with no gap or overlap
	{
		std::unique_ptr<FileInfo> file = BuildFile("release.r01",
			{{1, 1300}, {2, 1300}, {3, 700}}, "orig");
		file->SetDecodedFileSize(2500);
		FinishArticle(file.get(), 0, 0, 1000);
		FinishArticle(file.get(), 1, 1000, 1000);
		FinishArticle(file.get(), 2, 2000, 500);
		BOOST_CHECK(DupeArticleFallback::FirstUntiledArticle(file.get()) == nullptr);
	}

	// interior gap/overlap: part 2 drifted 40 bytes late (part 1 ends at 1000,
	// part 2 begins at 1040) - the offending article is returned so the file is
	// not classified as a clean success
	{
		std::unique_ptr<FileInfo> file = BuildFile("release.r01",
			{{1, 1300}, {2, 1300}, {3, 700}}, "orig");
		file->SetDecodedFileSize(2500);
		FinishArticle(file.get(), 0, 0, 1000);
		FinishArticle(file.get(), 1, 1040, 1000);
		FinishArticle(file.get(), 2, 2000, 500);
		ArticleInfo* untiled = DupeArticleFallback::FirstUntiledArticle(file.get());
		BOOST_REQUIRE(untiled);
		BOOST_CHECK_EQUAL(untiled->GetPartNumber(), 2);
	}

	// first article not at offset 0
	{
		std::unique_ptr<FileInfo> file = BuildFile("release.r01",
			{{1, 1300}, {2, 1300}, {3, 700}}, "orig");
		file->SetDecodedFileSize(2500);
		FinishArticle(file.get(), 0, 40, 1000);
		FinishArticle(file.get(), 1, 1040, 1000);
		FinishArticle(file.get(), 2, 2040, 500);
		ArticleInfo* untiled = DupeArticleFallback::FirstUntiledArticle(file.get());
		BOOST_REQUIRE(untiled);
		BOOST_CHECK_EQUAL(untiled->GetPartNumber(), 1);
	}

	// tiles internally but falls short of the decoded file size: last returned
	{
		std::unique_ptr<FileInfo> file = BuildFile("release.r01",
			{{1, 1300}, {2, 1300}, {3, 700}}, "orig");
		file->SetDecodedFileSize(2500);
		FinishArticle(file.get(), 0, 0, 1000);
		FinishArticle(file.get(), 1, 1000, 1000);
		FinishArticle(file.get(), 2, 2000, 460);
		ArticleInfo* untiled = DupeArticleFallback::FirstUntiledArticle(file.get());
		BOOST_REQUIRE(untiled);
		BOOST_CHECK_EQUAL(untiled->GetPartNumber(), 3);
	}

	// non-yEnc / geometry unknown: DecodedFileSize 0 (e.g. uuencode records all
	// offsets as 0) - cannot judge, must not false-positive
	{
		std::unique_ptr<FileInfo> file = BuildFile("release.r01",
			{{1, 1300}, {2, 1300}, {3, 700}}, "orig");
		FinishArticle(file.get(), 0, 0, 1000);
		FinishArticle(file.get(), 1, 0, 1000);
		FinishArticle(file.get(), 2, 0, 500);
		BOOST_CHECK(DupeArticleFallback::FirstUntiledArticle(file.get()) == nullptr);
	}

	// not all finished: incomplete geometry, do not judge
	{
		std::unique_ptr<FileInfo> file = BuildFile("release.r01",
			{{1, 1300}, {2, 1300}, {3, 700}}, "orig");
		file->SetDecodedFileSize(2500);
		FinishArticle(file.get(), 0, 0, 1000);
		FinishArticle(file.get(), 2, 2000, 500);
		BOOST_CHECK(DupeArticleFallback::FirstUntiledArticle(file.get()) == nullptr);
	}
}

BOOST_AUTO_TEST_SUITE_END()
