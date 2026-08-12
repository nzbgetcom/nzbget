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
#include <set>
#include "Options.h"
#include "PostDownloadRenamer.h"
#include "RenamerTestHelpers.h"
#include "FileSystem.h"
#include "FileTypes.h"
#include "DownloadInfo.h"
#include "Deobfuscation.h"

BOOST_AUTO_TEST_SUITE(PostprocessTest)

BOOST_AUTO_TEST_CASE(ResolveSubtitleNameTest)
{
	BOOST_CHECK_EQUAL(PostDownloadRenamer::ResolveSubtitleName("meta", "abcde.eng", ".srt"), "meta.eng.srt");
	BOOST_CHECK_EQUAL(PostDownloadRenamer::ResolveSubtitleName("meta", "abcde.dut", ".sub"), "meta.dut.sub");
	BOOST_CHECK_EQUAL(PostDownloadRenamer::ResolveSubtitleName("meta", "abcde.srt", ".srt"), "meta.srt.srt");
	BOOST_CHECK_EQUAL(PostDownloadRenamer::ResolveSubtitleName("meta", "abcde.123", ".srt"), "meta.srt");
	BOOST_CHECK_EQUAL(PostDownloadRenamer::ResolveSubtitleName("meta", "abcde.e", ".srt"), "meta.srt");
	BOOST_CHECK_EQUAL(PostDownloadRenamer::ResolveSubtitleName("meta", "abcde.english", ".srt"), "meta.srt");
	BOOST_CHECK_EQUAL(PostDownloadRenamer::ResolveSubtitleName("meta", "abc", ".srt"), "meta.srt");
	BOOST_CHECK_EQUAL(PostDownloadRenamer::ResolveSubtitleName("meta", "a.b", ".srt"), "meta.srt");
	BOOST_CHECK_EQUAL(PostDownloadRenamer::ResolveSubtitleName("meta", "a.en", ".srt"), "meta.en.srt");
	BOOST_CHECK_EQUAL(PostDownloadRenamer::ResolveSubtitleName("meta", "1.fr", ".srt"), "meta.fr.srt");
	BOOST_CHECK_EQUAL(PostDownloadRenamer::ResolveSubtitleName("meta", "abcde", ".srt"), "meta.srt");
}

BOOST_AUTO_TEST_CASE(IsSampleStemTest)
{
	BOOST_CHECK(FileTypes::IsSampleStem("abc-sample"));
	BOOST_CHECK(FileTypes::IsSampleStem("abc.sample"));
	BOOST_CHECK(FileTypes::IsSampleStem("abc_sample"));
	BOOST_CHECK(FileTypes::IsSampleStem("abc-SAMPLE"));
	BOOST_CHECK(FileTypes::IsSampleStem("abc.Sample"));
	BOOST_CHECK(!FileTypes::IsSampleStem("abc-sampl"));
	BOOST_CHECK(!FileTypes::IsSampleStem("abc-sample-extra"));
	BOOST_CHECK(FileTypes::IsSampleStem("sample"));
	BOOST_CHECK(FileTypes::IsSampleStem("Sample"));
}

BOOST_FIXTURE_TEST_CASE(ResolveUniqueNameTest, RenamerTestFixture)
{
	std::set<fs::path> usedPaths;

	std::string name1 = PostDownloadRenamer::ResolveUniqueName("meta", "stem", ".mkv", "meta.mkv", usedPaths, workingDir);
	BOOST_CHECK_EQUAL(name1, "meta.mkv");
	usedPaths.insert(workingDir / name1);

	std::string name2 = PostDownloadRenamer::ResolveUniqueName("meta", "stem", ".mkv", "meta.mkv", usedPaths, workingDir);
	BOOST_CHECK_EQUAL(name2, "meta(1).mkv");
	usedPaths.insert(workingDir / name2);

	WriteEmptyFile(workingDir / "meta(2).mkv");
	std::string name3 = PostDownloadRenamer::ResolveUniqueName("meta", "stem", ".mkv", "meta.mkv", usedPaths, workingDir);
	BOOST_CHECK_EQUAL(name3, "meta(3).mkv");
	usedPaths.insert(workingDir / name3);

	std::string sub1 = PostDownloadRenamer::ResolveUniqueName("meta", "eng", ".srt", "meta.srt", usedPaths, workingDir);
	BOOST_CHECK_EQUAL(sub1, "meta.srt");
	usedPaths.insert(workingDir / sub1);

	std::string sub2 = PostDownloadRenamer::ResolveUniqueName("meta", "eng", ".srt", "meta.srt", usedPaths, workingDir);
	BOOST_CHECK_EQUAL(sub2, "meta.eng.srt");
	usedPaths.insert(workingDir / sub2);

	std::string sub3 = PostDownloadRenamer::ResolveUniqueName("meta", "eng", ".srt", "meta.srt", usedPaths, workingDir);
	BOOST_CHECK_EQUAL(sub3, "meta.eng(1).srt");
	usedPaths.insert(workingDir / sub3);

	// Test 2-letter subtitle language code collision resolution
	WriteEmptyFile(workingDir / "meta.en.srt");
	std::string sub2letter = PostDownloadRenamer::ResolveUniqueName("meta", "en", ".srt", "meta.en.srt", usedPaths, workingDir);
	BOOST_CHECK_EQUAL(sub2letter, "meta.en(1).srt");
}

BOOST_FIXTURE_TEST_CASE(BasicRenameTest, RenamerTestFixture)
{
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.10");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv");

	auto nzbInfo = SetupNzb(workingDir, {
		"2c0837e5fa42c8cfb5d5e583168a2af4.10",
		"5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"
	});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Downloaded);

	BOOST_CHECK_EQUAL(count, 2);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".10")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".mkv")));
	BOOST_CHECK(!fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.10"));
	BOOST_CHECK(!fs::exists(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"));

	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCompletedFiles()->at(0).GetFilename()),
		METANAME + ".10");
	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCompletedFiles()->at(1).GetFilename()),
		METANAME + ".mkv");
}

BOOST_FIXTURE_TEST_CASE(DownloadedScopeSkipsExtractedFilesTest, RenamerTestFixture)
{
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv");

	// Only the first file was downloaded; the second is an extracted output.
	auto nzbInfo = SetupNzb(workingDir, { "2c0837e5fa42c8cfb5d5e583168a2af4.mkv" });

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Downloaded);

	BOOST_CHECK_EQUAL(count, 1);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".mkv")));
	BOOST_CHECK(fs::exists(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"));
	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCompletedFiles()->at(0).GetFilename()),
		METANAME + ".mkv");
}

// DirectRenamer stores the par2-discovered name in the completed-file record,
// which can be a full relative path ("Some.Dir/<obfuscated>.mkv"). The
// downloaded-files classifier must still recognize the on-disk file as
// downloaded (matching by basename), rename it in place inside its
// subdirectory, and update the path-qualified record.
BOOST_FIXTURE_TEST_CASE(DownloadedFileInSubdirWithPathQualifiedRecordTest, RenamerTestFixture)
{
	fs::create_directories(workingDir / "Some.Dir");
	WriteEmptyFile(workingDir / "Some.Dir" / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv");

	auto nzbInfo = SetupNzb(workingDir, { "Some.Dir/2c0837e5fa42c8cfb5d5e583168a2af4.mkv" });

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Downloaded);

	BOOST_CHECK_EQUAL(count, 1);
	BOOST_CHECK(fs::exists(workingDir / "Some.Dir" / (METANAME + ".mkv")));
	BOOST_CHECK(!fs::exists(workingDir / "Some.Dir" / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));
	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCompletedFiles()->at(0).GetFilename()),
		"Some.Dir/" + METANAME + ".mkv");
}

BOOST_FIXTURE_TEST_CASE(SkipIgnoreExtTest, RenamerTestFixture)
{
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.txt");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.nfo");

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 0);
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.txt"));
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.nfo"));
}

BOOST_FIXTURE_TEST_CASE(SkipArchiveAndParityTest, RenamerTestFixture)
{
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.rar");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.par2");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.sfv");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.vob");

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 0);
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.rar"));
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.par2"));
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.sfv"));
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.vob"));
}

BOOST_FIXTURE_TEST_CASE(SkipNonObfuscatedTest, RenamerTestFixture)
{
	WriteEmptyFile(workingDir / "Some.Legitimate.Name.mkv");
	WriteEmptyFile(workingDir / "Another.Name.mp4");

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 0);
	BOOST_CHECK(fs::exists(workingDir / "Some.Legitimate.Name.mkv"));
	BOOST_CHECK(fs::exists(workingDir / "Another.Name.mp4"));
}

BOOST_FIXTURE_TEST_CASE(SubtitleLanguageTagTest, RenamerTestFixture)
{
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.eng.srt");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.dut.sub");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.srt");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.ass");

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 4);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".eng.srt")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".dut.sub")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".srt")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".ass")));
}

BOOST_FIXTURE_TEST_CASE(SampleSuffixTest, RenamerTestFixture)
{
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4-sample.mkv");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.sample.mkv");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4_sample.mkv");

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 3);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + "-sample.mkv")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + "(1).mkv")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + "(2).mkv")));
}

BOOST_FIXTURE_TEST_CASE(EqualSizedCollectionSkippedTest, RenamerTestFixture)
{
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv");
	WriteEmptyFile(workingDir / "a4c7d1f239b71aa1c0a8b1790e65c943.mkv");

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 0);
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));
	BOOST_CHECK(fs::exists(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"));
	BOOST_CHECK(fs::exists(workingDir / "a4c7d1f239b71aa1c0a8b1790e65c943.mkv"));
}

BOOST_FIXTURE_TEST_CASE(DominantCollectionRenameTest, RenamerTestFixture)
{
	WriteFileWithSize(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv", 4000);
	WriteFileWithSize(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv", 1000);

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 2);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".mkv")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + "(1).mkv")));
	BOOST_CHECK(!fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));
	BOOST_CHECK(!fs::exists(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"));
}

BOOST_FIXTURE_TEST_CASE(SubtitleCollisionTest, RenamerTestFixture)
{
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.eng.srt");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.eng.srt");

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 2);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".eng.srt")));
	BOOST_CHECK(
		fs::exists(workingDir / (METANAME + ".2c0837e5fa42c8cfb5d5e583168a2af4.eng.srt")) ||
		fs::exists(workingDir / (METANAME + ".5KzdcWdGVGUG83Q9jv8KXht4O2k57w.eng.srt")));
}

BOOST_FIXTURE_TEST_CASE(NoMetanameTest, RenamerTestFixture)
{
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv");

	auto nzbInfo = std::make_unique<NzbInfo>();
	nzbInfo->SetName("");
	nzbInfo->SetDestDir(workingDir.string().c_str());

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 0);
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));
}

BOOST_FIXTURE_TEST_CASE(DownloadedPassRespectsDirectRenameOptionTest, RenamerTestFixture)
{
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv");

	auto nzbInfo = SetupNzb(workingDir, { "2c0837e5fa42c8cfb5d5e583168a2af4.mkv" });
	nzbInfo->SetUnpackStatus(NzbInfo::usSuccess);

	// Disable DirectRename, enable RenameAfterUnpack
	Options::CmdOptList localCmdOpts = { "DirectRename=no", "RenameAfterUnpack=yes" };
	Options localOptions(&localCmdOpts, nullptr);

	PostDownloadRenamerDownloadQueueMock downloadQueue;
	PostInfo postInfo;
	postInfo.SetNzbInfo(nzbInfo.get());
	PostDownloadRenamer::Controller renamer;

	PostDownloadRenamer::RenameResult result = PostDownloadRenamer::RenameFiles(renamer, &postInfo);

	BOOST_CHECK(!result.downloadedRan);
	BOOST_CHECK(result.extractedRan);
	BOOST_CHECK_EQUAL(result.downloadedCount, 0);
	BOOST_CHECK_EQUAL(result.extractedCount, 0);
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));
}

BOOST_FIXTURE_TEST_CASE(ObfuscatedMetanameTest, RenamerTestFixture)
{
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv");

	auto nzbInfo = std::make_unique<NzbInfo>();
	nzbInfo->SetName("2c0837e5fa42c8cfb5d5e583168a2af4");
	nzbInfo->SetDestDir(workingDir.string().c_str());

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 0);
	BOOST_CHECK(fs::exists(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"));
}

BOOST_FIXTURE_TEST_CASE(ObfuscatedMetaNameWithCleanCollectionNameTest, RenamerTestFixture)
{
	const std::string collectionName = "Clean.Collection.Name.1080p";

	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv");

	auto nzbInfo = std::make_unique<NzbInfo>();
	nzbInfo->SetName(collectionName.c_str());
	nzbInfo->SetDestDir(workingDir.string().c_str());
	nzbInfo->GetParameters()->SetParameter("*MetaName", "M7hATCUQ5RPgNEkb");

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 1);
	BOOST_CHECK(fs::exists(workingDir / (collectionName + ".mkv")));
	BOOST_CHECK(!fs::exists(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"));
}

BOOST_FIXTURE_TEST_CASE(CleanCollectionNameFallbackTest, RenamerTestFixture)
{
	const std::string collectionName = "Clean.Collection.Name.1080p";

	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv");

	auto nzbInfo = std::make_unique<NzbInfo>();
	nzbInfo->SetName(collectionName.c_str());
	nzbInfo->SetDestDir(workingDir.string().c_str());

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 1);
	BOOST_CHECK(fs::exists(workingDir / (collectionName + ".mkv")));
	BOOST_CHECK(!fs::exists(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"));
}

BOOST_AUTO_TEST_CASE(NonExistentDestDirTest)
{
	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_NonExistent";
	fs::remove_all(workingDir);

	auto nzbInfo = std::make_unique<NzbInfo>();
	nzbInfo->SetName("SomeNzb");
	nzbInfo->SetDestDir(workingDir.string().c_str());
	nzbInfo->GetParameters()->SetParameter("*MetaName", METANAME.c_str());

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 0);
}

BOOST_AUTO_TEST_CASE(HardLinkFinalDirRenameTest)
{
	const fs::path workDir = CURR_DIR / "HardLinkRename_work";
	const std::string nzbName = "HardLinkRename_dest";
	const fs::path destDir = CURR_DIR / "HardLinkRename_top";
	const fs::path finalDir = destDir / nzbName;

	fs::remove_all(workDir);
	fs::remove_all(destDir);
	fs::create_directories(workDir);
	fs::create_directories(finalDir);

	std::vector<std::string> optStorage;
	optStorage.push_back("InterDir=" + workDir.string());
	optStorage.push_back("DestDir=" + destDir.string());
	optStorage.push_back("HardLinking=yes");
	optStorage.push_back("DirectRename=yes");

	Options::CmdOptList cmdOpts;
	for (const auto& s : optStorage)
	{
		cmdOpts.push_back(s.c_str());
	}
	Options options(&cmdOpts, nullptr);

	const std::string obfuscated = "2c0837e5fa42c8cfb5d5e583168a2af4.mkv";
	WriteEmptyFile(workDir / obfuscated);

	fs::error_code ec;
	fs::create_hard_link(workDir / obfuscated, finalDir / obfuscated, ec);
	BOOST_REQUIRE_MESSAGE(!ec, "create_hard_link failed: " << ec.message());

	auto nzbInfo = std::make_unique<NzbInfo>();
	nzbInfo->SetName(nzbName.c_str());
	nzbInfo->SetDestDir(workDir.string().c_str());
	nzbInfo->GetParameters()->SetParameter("*MetaName", METANAME.c_str());
	nzbInfo->GetCompletedFiles()->emplace_back(
		1, obfuscated, obfuscated, CompletedFile::cfSuccess, 0, false, "", "");

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Downloaded);

	BOOST_CHECK_EQUAL(count, 1);
	BOOST_CHECK(fs::exists(workDir / (METANAME + ".mkv")));
	BOOST_CHECK(!fs::exists(workDir / obfuscated));
	BOOST_CHECK(fs::exists(finalDir / obfuscated));

	fs::remove_all(workDir);
	fs::remove_all(destDir);
}

BOOST_FIXTURE_TEST_CASE(AudioCarveOutRenameTest, RenamerTestFixture)
{
	// Equal-sized audio files (ambiguous music album): should always rename (tags carry identity).
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mp3");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mp3");

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 2);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".mp3")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + "(1).mp3")));
}

BOOST_FIXTURE_TEST_CASE(SubfolderScopedDominanceTest, RenamerTestFixture)
{
	fs::path subA = workingDir / "Season.01";
	fs::path subB = workingDir / "Season.02";
	fs::create_directories(subA);
	fs::create_directories(subB);

	// Season 01: dominant file (4000 vs 1000) -> should rename both.
	WriteFileWithSize(subA / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv", 4000);
	WriteFileWithSize(subA / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv", 1000);

	// Season 02: equal size (1000 vs 1000) -> ambiguous collection, should skip.
	WriteFileWithSize(subB / "a4c7d1f239b71aa1c0a8b1790e65c943.mkv", 1000);
	WriteFileWithSize(subB / "b5d6e2f340c82bb2d1b9c2801f76d054.mkv", 1000);

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	// Only Season 01 files get renamed (2 files). Season 02 files stay untouched.
	BOOST_CHECK_EQUAL(count, 2);
	BOOST_CHECK(fs::exists(subA / (METANAME + ".mkv")));
	BOOST_CHECK(fs::exists(subA / (METANAME + "(1).mkv")));
	BOOST_CHECK(fs::exists(subB / "a4c7d1f239b71aa1c0a8b1790e65c943.mkv"));
	BOOST_CHECK(fs::exists(subB / "b5d6e2f340c82bb2d1b9c2801f76d054.mkv"));
}

BOOST_FIXTURE_TEST_CASE(OrphanedSubtitleSkippedWithAmbiguousCollectionTest, RenamerTestFixture)
{
	// Three equal-sized .mkv files (ambiguous collection -> skipped) plus a subtitle companion.
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv");
	WriteEmptyFile(workingDir / "a4c7d1f239b71aa1c0a8b1790e65c943.mkv");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.eng.srt");

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	// Since there is no resolved primary group in the directory, the subtitle companion
	// must also be skipped to avoid an orphaned/inconsistent rename.
	BOOST_CHECK_EQUAL(count, 0);
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.eng.srt"));
}

BOOST_FIXTURE_TEST_CASE(SeasonPackSingletonPerSubfolderTest, RenamerTestFixture)
{
	fs::path ep1 = workingDir / "Season.01" / "Episode.01";
	fs::path ep2 = workingDir / "Season.01" / "Episode.02";
	fs::path ep3 = workingDir / "Season.01" / "Episode.03";
	fs::create_directories(ep1);
	fs::create_directories(ep2);
	fs::create_directories(ep3);

	// Each subfolder contains exactly one file (singleton per directory).
	// Under whole-tree grouping this would be 3 equal files and skipped;
	// under directory scoping each folder's singleton is renameable.
	WriteFileWithSize(ep1 / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv", 1000);
	WriteFileWithSize(ep2 / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv", 1000);
	WriteFileWithSize(ep3 / "a4c7d1f239b71aa1c0a8b1790e65c943.mkv", 1000);

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 3);
	BOOST_CHECK(fs::exists(ep1 / (METANAME + ".mkv")));
	BOOST_CHECK(fs::exists(ep2 / (METANAME + ".mkv")));
	BOOST_CHECK(fs::exists(ep3 / (METANAME + ".mkv")));
}

BOOST_FIXTURE_TEST_CASE(MergedScopeRenamesBothAndSetsBothStatusesTest, RenamerTestFixture)
{
	// The merged controller renames downloaded and extracted files in one scan
	// and sets both statuses independently.

	// One downloaded file (present in completed files) and one extracted output.
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mp4");

	auto nzbInfo = SetupNzb(workingDir, { "2c0837e5fa42c8cfb5d5e583168a2af4.mkv" });
	nzbInfo->SetUnpackStatus(NzbInfo::usSuccess);

	PostDownloadRenamerDownloadQueueMock downloadQueue;
	PostInfo postInfo;
	postInfo.SetNzbInfo(nzbInfo.get());
	PostDownloadRenamer::Controller renamer;
	PostDownloadRenamer::RenameResult result = PostDownloadRenamer::RenameFiles(renamer, &postInfo);

	BOOST_CHECK_EQUAL(result.downloadedCount, 1);
	BOOST_CHECK_EQUAL(result.extractedCount, 1);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".mkv")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".mp4")));

	PostDownloadRenamer::FinishStage(renamer, &postInfo, result);
	BOOST_CHECK(nzbInfo->GetPostRenamingStatus() == NzbInfo::RenamingStatus::Success);
	BOOST_CHECK(nzbInfo->GetPostUnpackRenamingStatus() == NzbInfo::RenamingStatus::Success);
}

BOOST_FIXTURE_TEST_CASE(MergedScopeSkipsExtractedWhenUnpackNotSucceededTest, RenamerTestFixture)
{
	// When unpack did not succeed (or RenameAfterUnpack is off), only the
	// downloaded scope runs and only its status is set.
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mp4");

	auto nzbInfo = SetupNzb(workingDir, { "2c0837e5fa42c8cfb5d5e583168a2af4.mkv" });
	nzbInfo->SetUnpackStatus(NzbInfo::usFailure);

	PostDownloadRenamerDownloadQueueMock downloadQueue;
	PostInfo postInfo;
	postInfo.SetNzbInfo(nzbInfo.get());
	PostDownloadRenamer::Controller renamer;
	PostDownloadRenamer::RenameResult result = PostDownloadRenamer::RenameFiles(renamer, &postInfo);

	BOOST_CHECK_EQUAL(result.downloadedCount, 1);
	BOOST_CHECK_EQUAL(result.extractedCount, 0);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".mkv")));
	BOOST_CHECK(fs::exists(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mp4"));

	PostDownloadRenamer::FinishStage(renamer, &postInfo, result);
	BOOST_CHECK(nzbInfo->GetPostRenamingStatus() == NzbInfo::RenamingStatus::Success);
	BOOST_CHECK(nzbInfo->GetPostUnpackRenamingStatus() == NzbInfo::RenamingStatus::None);
}

BOOST_FIXTURE_TEST_CASE(ExtractedScopeLeavesDownloadedFilesUntouchedTest, RenamerTestFixture)
{
	// The extracted scope must never rename downloaded files or update their
	// completed-file records, even when both file kinds are on disk.
	WriteFileWithSize(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv", 4000);
	WriteEmptyFile(workingDir / "a4c7d1f239b71aa1c0a8b1790e65c943.eng.srt");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv");

	auto nzbInfo = SetupNzb(workingDir, { "2c0837e5fa42c8cfb5d5e583168a2af4.mkv" });

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 2);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".mkv")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".eng.srt")));
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));

	// The downloaded file's completed record must stay untouched.
	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCompletedFiles()->at(0).GetFilename()),
		"2c0837e5fa42c8cfb5d5e583168a2af4.mkv");
}

BOOST_FIXTURE_TEST_CASE(ObfuscatedMetaNameSettlesStatusesTest, RenamerTestFixture)
{
	const std::string obfuscated = "b4bd41113f2d410fa16dc6a04d2cad5c";

	WriteEmptyFile(workingDir / (obfuscated + ".mkv"));

	auto nzbInfo = std::make_unique<NzbInfo>();
	nzbInfo->SetName(obfuscated.c_str());
	nzbInfo->SetDestDir(workingDir.string().c_str());
	nzbInfo->GetParameters()->SetParameter("*MetaName", obfuscated.c_str());
	nzbInfo->GetCompletedFiles()->emplace_back(
		1, (obfuscated + ".mkv").c_str(), (obfuscated + ".mkv").c_str(),
		CompletedFile::cfSuccess, 0, false, "", "");
	nzbInfo->SetUnpackStatus(NzbInfo::usSuccess);

	PostDownloadRenamerDownloadQueueMock downloadQueue;
	PostInfo postInfo;
	postInfo.SetNzbInfo(nzbInfo.get());
	PostDownloadRenamer::Controller renamer;
	PostDownloadRenamer::RenameResult result = PostDownloadRenamer::RenameFiles(renamer, &postInfo);

	// Nothing can be renamed (no valid target name) and no file is touched.
	BOOST_CHECK_EQUAL(result.downloadedCount, 0);
	BOOST_CHECK_EQUAL(result.extractedCount, 0);
	BOOST_CHECK(fs::exists(workingDir / (obfuscated + ".mkv")));

	PostDownloadRenamer::FinishStage(renamer, &postInfo, result);

	// Both scopes ran; both statuses must be terminal so the stage is never re-entered.
	BOOST_CHECK(nzbInfo->GetPostRenamingStatus() == NzbInfo::RenamingStatus::Nothing);
	BOOST_CHECK(nzbInfo->GetPostUnpackRenamingStatus() == NzbInfo::RenamingStatus::Nothing);
}

BOOST_AUTO_TEST_CASE(EmptyDestDirSettlesStatusesTest)
{
	// A missing destDir is another early-return path: the statuses must still
	// settle so the stage does not spin.
	Options::CmdOptList cmdOpts = { "DirectRename=yes", "RenameAfterUnpack=yes" };
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = std::make_unique<NzbInfo>();
	nzbInfo->SetName("SomeNzb");
	nzbInfo->GetParameters()->SetParameter("*MetaName", METANAME.c_str());
	nzbInfo->SetUnpackStatus(NzbInfo::usSuccess);

	PostDownloadRenamerDownloadQueueMock downloadQueue;
	PostInfo postInfo;
	postInfo.SetNzbInfo(nzbInfo.get());
	PostDownloadRenamer::Controller renamer;
	PostDownloadRenamer::RenameResult result = PostDownloadRenamer::RenameFiles(renamer, &postInfo);

	BOOST_CHECK_EQUAL(result.downloadedCount, 0);
	BOOST_CHECK_EQUAL(result.extractedCount, 0);

	PostDownloadRenamer::FinishStage(renamer, &postInfo, result);

	BOOST_CHECK(nzbInfo->GetPostRenamingStatus() == NzbInfo::RenamingStatus::Nothing);
	BOOST_CHECK(nzbInfo->GetPostUnpackRenamingStatus() == NzbInfo::RenamingStatus::Nothing);
}

BOOST_FIXTURE_TEST_CASE(BoundaryExact3xCollectionSkippedTest, RenamerTestFixture)
{
	// largest == 3 * second (3000 vs 1000): ambiguous per the ratio rule.
	WriteFileWithSize(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv", 3000);
	WriteFileWithSize(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv", 1000);

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 0);
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));
	BOOST_CHECK(fs::exists(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"));
}

BOOST_FIXTURE_TEST_CASE(SampleOrphanSkippedWithAmbiguousCollectionTest, RenamerTestFixture)
{
	// Three equal-sized .mkv files (ambiguous collection -> skipped) plus a sample companion.
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv");
	WriteEmptyFile(workingDir / "a4c7d1f239b71aa1c0a8b1790e65c943.mkv");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4-sample.mkv");

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 0);
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4-sample.mkv"));
}

BOOST_FIXTURE_TEST_CASE(SubtitleAnchoredToDominantRenamesTest, RenamerTestFixture)
{
	// Dominant .mkv pair (renamed) plus an obfuscated subtitle companion.
	WriteFileWithSize(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv", 4000);
	WriteFileWithSize(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv", 1000);
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.eng.srt");

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 3);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".mkv")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + "(1).mkv")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".eng.srt")));
}

BOOST_FIXTURE_TEST_CASE(SubtitleInAudioOnlyDirRenamesTest, RenamerTestFixture)
{
	// Equal-sized audio (audio carve-out: renamed) plus a subtitle companion.
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mp3");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mp3");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.eng.srt");

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 3);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".mp3")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + "(1).mp3")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".eng.srt")));
}

BOOST_AUTO_TEST_SUITE_END()
