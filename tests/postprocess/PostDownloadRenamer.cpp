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
	BOOST_CHECK(!FileTypes::IsSampleStem("sample"));
}

BOOST_AUTO_TEST_CASE(ResolveUniqueNameTest)
{
	const fs::path workingDir = CURR_DIR / "ResolveUniqueName_Test";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

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

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(BasicRenameTest)
{
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_Basic";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.10");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

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

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(DownloadedScopeSkipsExtractedFilesTest)
{
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_DownloadedScope";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	// Only the first file was downloaded; the second is an extracted output.
	auto nzbInfo = SetupNzb(workingDir, { "2c0837e5fa42c8cfb5d5e583168a2af4.mkv" });

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Downloaded);

	BOOST_CHECK_EQUAL(count, 1);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".mkv")));
	BOOST_CHECK(fs::exists(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"));
	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCompletedFiles()->at(0).GetFilename()),
		METANAME + ".mkv");

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(SkipIgnoreExtTest)
{
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_SkipIgnoreExt";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.txt");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.nfo");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 0);
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.txt"));
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.nfo"));

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(SkipArchiveAndParityTest)
{
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_SkipArchiveParity";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.rar");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.par2");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.sfv");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.vob");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 0);
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.rar"));
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.par2"));
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.sfv"));
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.vob"));

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(SkipNonObfuscatedTest)
{
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_SkipNonObfuscated";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	WriteEmptyFile(workingDir / "Some.Legitimate.Name.mkv");
	WriteEmptyFile(workingDir / "Another.Name.mp4");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 0);
	BOOST_CHECK(fs::exists(workingDir / "Some.Legitimate.Name.mkv"));
	BOOST_CHECK(fs::exists(workingDir / "Another.Name.mp4"));

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(SubtitleLanguageTagTest)
{
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_SubtitleLangTag";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.eng.srt");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.dut.sub");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.srt");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.ass");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 4);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".eng.srt")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".dut.sub")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".srt")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".ass")));

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(SampleSuffixTest)
{
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_SampleSuffix";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4-sample.mkv");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.sample.mkv");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4_sample.mkv");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 3);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + "-sample.mkv")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + "(1).mkv")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + "(2).mkv")));

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(EqualSizedCollectionSkippedTest)
{
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_EqualCollection";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv");
	WriteEmptyFile(workingDir / "a4c7d1f239b71aa1c0a8b1790e65c943.mkv");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 0);
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));
	BOOST_CHECK(fs::exists(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"));
	BOOST_CHECK(fs::exists(workingDir / "a4c7d1f239b71aa1c0a8b1790e65c943.mkv"));

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(DominantCollectionRenameTest)
{
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_DominantCollection";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	WriteFileWithSize(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv", 4000);
	WriteFileWithSize(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv", 1000);

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 2);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".mkv")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + "(1).mkv")));
	BOOST_CHECK(!fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));
	BOOST_CHECK(!fs::exists(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"));

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(SubtitleCollisionTest)
{
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_SubtitleCollision";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.eng.srt");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.eng.srt");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 2);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".eng.srt")));
	BOOST_CHECK(
		fs::exists(workingDir / (METANAME + ".2c0837e5fa42c8cfb5d5e583168a2af4.eng.srt")) ||
		fs::exists(workingDir / (METANAME + ".5KzdcWdGVGUG83Q9jv8KXht4O2k57w.eng.srt")));

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(NoMetanameTest)
{
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_NoMetaname";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = std::make_unique<NzbInfo>();
	nzbInfo->SetName("");
	nzbInfo->SetDestDir(workingDir.string().c_str());

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 0);
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(ObfuscatedMetanameTest)
{
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_ObfuscatedMetaname";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = std::make_unique<NzbInfo>();
	nzbInfo->SetName("2c0837e5fa42c8cfb5d5e583168a2af4");
	nzbInfo->SetDestDir(workingDir.string().c_str());

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 0);
	BOOST_CHECK(fs::exists(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"));

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(ObfuscatedMetaNameWithCleanCollectionNameTest)
{
	const std::string collectionName = "Clean.Collection.Name.1080p";
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_ObfuscatedMetaCleanColl";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = std::make_unique<NzbInfo>();
	nzbInfo->SetName(collectionName.c_str());
	nzbInfo->SetDestDir(workingDir.string().c_str());
	nzbInfo->GetParameters()->SetParameter("*MetaName", "M7hATCUQ5RPgNEkb");

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 1);
	BOOST_CHECK(fs::exists(workingDir / (collectionName + ".mkv")));
	BOOST_CHECK(!fs::exists(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"));

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(CleanCollectionNameFallbackTest)
{
	const std::string collectionName = "Clean.Collection.Name.1080p";
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_CleanCollFallback";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = std::make_unique<NzbInfo>();
	nzbInfo->SetName(collectionName.c_str());
	nzbInfo->SetDestDir(workingDir.string().c_str());

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 1);
	BOOST_CHECK(fs::exists(workingDir / (collectionName + ".mkv")));
	BOOST_CHECK(!fs::exists(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"));

	fs::remove_all(workingDir);
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

	PostDownloadRenamerDownloadQueueMock downloadQueue;
	PostInfo postInfo;
	postInfo.SetNzbInfo(nzbInfo.get());
	PostDownloadRenamer::Controller renamer;
	int count = PostDownloadRenamer::RenameFiles(renamer, &postInfo, PostDownloadRenamer::Scope::Downloaded);

	BOOST_CHECK_EQUAL(count, 1);
	BOOST_CHECK(fs::exists(workDir / (METANAME + ".mkv")));
	BOOST_CHECK(!fs::exists(workDir / obfuscated));
	BOOST_CHECK(fs::exists(finalDir / obfuscated));

	fs::remove_all(workDir);
	fs::remove_all(destDir);
}

BOOST_AUTO_TEST_CASE(AudioCarveOutRenameTest)
{
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_AudioCarveOut";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	// Equal-sized audio files (ambiguous music album): should always rename (tags carry identity).
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mp3");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mp3");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 2);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".mp3")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + "(1).mp3")));

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(SubfolderScopedDominanceTest)
{
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_SubfolderScoped";
	fs::path subA = workingDir / "Season.01";
	fs::path subB = workingDir / "Season.02";
	fs::remove_all(workingDir);
	fs::create_directories(subA);
	fs::create_directories(subB);

	// Season 01: dominant file (4000 vs 1000) -> should rename both.
	WriteFileWithSize(subA / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv", 4000);
	WriteFileWithSize(subA / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv", 1000);

	// Season 02: equal size (1000 vs 1000) -> ambiguous collection, should skip.
	WriteFileWithSize(subB / "a4c7d1f239b71aa1c0a8b1790e65c943.mkv", 1000);
	WriteFileWithSize(subB / "b5d6e2f340c82bb2d1b9c2801f76d054.mkv", 1000);

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	// Only Season 01 files get renamed (2 files). Season 02 files stay untouched.
	BOOST_CHECK_EQUAL(count, 2);
	BOOST_CHECK(fs::exists(subA / (METANAME + ".mkv")));
	BOOST_CHECK(fs::exists(subA / (METANAME + "(1).mkv")));
	BOOST_CHECK(fs::exists(subB / "a4c7d1f239b71aa1c0a8b1790e65c943.mkv"));
	BOOST_CHECK(fs::exists(subB / "b5d6e2f340c82bb2d1b9c2801f76d054.mkv"));

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(OrphanedSubtitleSkippedWithAmbiguousCollectionTest)
{
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_OrphanedSub";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	// Three equal-sized .mkv files (ambiguous collection -> skipped) plus a subtitle companion.
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv");
	WriteEmptyFile(workingDir / "a4c7d1f239b71aa1c0a8b1790e65c943.mkv");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.eng.srt");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	// Since there is no resolved primary group in the directory, the subtitle companion
	// must also be skipped to avoid an orphaned/inconsistent rename.
	BOOST_CHECK_EQUAL(count, 0);
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.eng.srt"));

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(SeasonPackSingletonPerSubfolderTest)
{
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_SeasonPack";
	fs::path ep1 = workingDir / "Season.01" / "Episode.01";
	fs::path ep2 = workingDir / "Season.01" / "Episode.02";
	fs::path ep3 = workingDir / "Season.01" / "Episode.03";
	fs::remove_all(workingDir);
	fs::create_directories(ep1);
	fs::create_directories(ep2);
	fs::create_directories(ep3);

	// Each subfolder contains exactly one file (singleton per directory).
	// Under whole-tree grouping this would be 3 equal files and skipped;
	// under directory scoping each folder's singleton is renameable.
	WriteFileWithSize(ep1 / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv", 1000);
	WriteFileWithSize(ep2 / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv", 1000);
	WriteFileWithSize(ep3 / "a4c7d1f239b71aa1c0a8b1790e65c943.mkv", 1000);

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 3);
	BOOST_CHECK(fs::exists(ep1 / (METANAME + ".mkv")));
	BOOST_CHECK(fs::exists(ep2 / (METANAME + ".mkv")));
	BOOST_CHECK(fs::exists(ep3 / (METANAME + ".mkv")));

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(MergedScopeRenamesBothAndSetsBothStatusesTest)
{
	// The merged controller renames downloaded and extracted files in one scan
	// and sets both statuses independently.
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_MergedScope";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	// One downloaded file (present in completed files) and one extracted output.
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mp4");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

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

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(MergedScopeSkipsExtractedWhenUnpackNotSucceededTest)
{
	// When unpack did not succeed (or RenameAfterUnpack is off), only the
	// downloaded scope runs and only its status is set.
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_MergedScopeNoUnpack";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mp4");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

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

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(ExtractedScopeLeavesDownloadedFilesUntouchedTest)
{
	// The extracted scope must never rename downloaded files or update their
	// completed-file records, even when both file kinds are on disk.
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_ExtractedScope";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	WriteFileWithSize(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv", 4000);
	WriteEmptyFile(workingDir / "a4c7d1f239b71aa1c0a8b1790e65c943.eng.srt");
	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = SetupNzb(workingDir, { "2c0837e5fa42c8cfb5d5e583168a2af4.mkv" });

	int count = RunRename(nzbInfo.get(), PostDownloadRenamer::Scope::Extracted);

	BOOST_CHECK_EQUAL(count, 2);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".mkv")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".eng.srt")));
	BOOST_CHECK(fs::exists(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));

	// The downloaded file's completed record must stay untouched.
	BOOST_CHECK_EQUAL(std::string(nzbInfo->GetCompletedFiles()->at(0).GetFilename()),
		"2c0837e5fa42c8cfb5d5e583168a2af4.mkv");

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_SUITE_END()
