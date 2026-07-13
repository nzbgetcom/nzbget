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
#include <fstream>
#include "Options.h"
#include "Rename.h"
#include "FileSystem.h"
#include "FileTypes.h"
#include "DownloadInfo.h"
#include "Deobfuscation.h"

BOOST_AUTO_TEST_SUITE(PostprocessTest)

const fs::path CURR_DIR = fs::current_path();
const std::string METANAME = "Some.Filename.1080p.WEB.H264";

namespace
{
	void WriteEmptyFile(const fs::path& path)
	{
		std::ofstream f(path);
		f.close();
	}
}

BOOST_AUTO_TEST_CASE(ResolveSubtitleNameTest)
{
	ObfuscatedRenamer renamer;
	BOOST_CHECK_EQUAL(renamer.ResolveSubtitleName("meta", "abcde.eng", ".srt"), "meta.eng.srt");
	BOOST_CHECK_EQUAL(renamer.ResolveSubtitleName("meta", "abcde.dut", ".sub"), "meta.dut.sub");
	BOOST_CHECK_EQUAL(renamer.ResolveSubtitleName("meta", "abcde.srt", ".srt"), "meta.srt.srt");
	BOOST_CHECK_EQUAL(renamer.ResolveSubtitleName("meta", "abcde.123", ".srt"), "meta.srt");
	BOOST_CHECK_EQUAL(renamer.ResolveSubtitleName("meta", "abcde.e", ".srt"), "meta.srt");
	BOOST_CHECK_EQUAL(renamer.ResolveSubtitleName("meta", "abcde.english", ".srt"), "meta.srt");
	BOOST_CHECK_EQUAL(renamer.ResolveSubtitleName("meta", "abc", ".srt"), "meta.srt");
	BOOST_CHECK_EQUAL(renamer.ResolveSubtitleName("meta", "a.b", ".srt"), "meta.srt");
	BOOST_CHECK_EQUAL(renamer.ResolveSubtitleName("meta", "abcde", ".srt"), "meta.srt");
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
	ObfuscatedRenamer renamer;
	const fs::path workingDir = CURR_DIR / "ResolveUniqueName_Test";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	std::unordered_set<std::string> usedNames;

	std::string name1 = renamer.ResolveUniqueName("meta", "stem", ".mkv", "meta.mkv", usedNames, workingDir);
	BOOST_CHECK_EQUAL(name1, "meta.mkv");
	usedNames.insert(name1);

	std::string name2 = renamer.ResolveUniqueName("meta", "stem", ".mkv", "meta.mkv", usedNames, workingDir);
	BOOST_CHECK_EQUAL(name2, "meta(1).mkv");
	usedNames.insert(name2);

	WriteEmptyFile(workingDir / "meta(2).mkv");
	std::string name3 = renamer.ResolveUniqueName("meta", "stem", ".mkv", "meta.mkv", usedNames, workingDir);
	BOOST_CHECK_EQUAL(name3, "meta(3).mkv");
	usedNames.insert(name3);

	std::string sub1 = renamer.ResolveUniqueName("meta", "eng", ".srt", "meta.srt", usedNames, workingDir);
	BOOST_CHECK_EQUAL(sub1, "meta.srt");
	usedNames.insert(sub1);

	std::string sub2 = renamer.ResolveUniqueName("meta", "eng", ".srt", "meta.srt", usedNames, workingDir);
	BOOST_CHECK_EQUAL(sub2, "meta.eng.srt");
	usedNames.insert(sub2);

	std::string sub3 = renamer.ResolveUniqueName("meta", "eng", ".srt", "meta.srt", usedNames, workingDir);
	BOOST_CHECK_EQUAL(sub3, "meta.eng(1).srt");
	usedNames.insert(sub3);

	fs::remove_all(workingDir);
}

namespace
{
	std::unique_ptr<NzbInfo> SetupNzb(const fs::path& workingDir,
		const std::vector<std::string>& completedObfuscatedNames)
	{
		auto nzbInfo = std::make_unique<NzbInfo>();
		nzbInfo->SetName(workingDir.filename().string().c_str());
		nzbInfo->SetDestDir(workingDir.string().c_str());
		nzbInfo->GetParameters()->SetParameter("*MetaName", METANAME.c_str());

		int id = 1;
		for (const std::string& name : completedObfuscatedNames)
		{
			nzbInfo->GetCompletedFiles()->emplace_back(
				id++, name, name, CompletedFile::cfSuccess, 0, false, "", "");
		}

		return nzbInfo;
	}

	int RunRename(const NzbInfo* nzbInfo)
	{
		PostInfo postInfo;
		postInfo.SetNzbInfo(const_cast<NzbInfo*>(nzbInfo));
		ObfuscatedRenamer renamer;
		return renamer.RenameFiles(&postInfo);
	}
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

	int count = RunRename(nzbInfo.get());

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

	int count = RunRename(nzbInfo.get());

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

	int count = RunRename(nzbInfo.get());

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

	int count = RunRename(nzbInfo.get());

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

	int count = RunRename(nzbInfo.get());

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

	int count = RunRename(nzbInfo.get());

	BOOST_CHECK_EQUAL(count, 3);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + "-sample.mkv")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + "(1).mkv")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + "(2).mkv")));

	fs::remove_all(workingDir);
}

BOOST_AUTO_TEST_CASE(MediaCollisionTest)
{
	const fs::path workingDir = CURR_DIR / "RenameObfuscatedFiles_MediaCollision";
	fs::remove_all(workingDir);
	fs::create_directory(workingDir);

	WriteEmptyFile(workingDir / "2c0837e5fa42c8cfb5d5e583168a2af4.mkv");
	WriteEmptyFile(workingDir / "5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv");
	WriteEmptyFile(workingDir / "a4c7d1f239b71aa1c0a8b1790e65c943.mkv");

	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	auto nzbInfo = SetupNzb(workingDir, {});

	int count = RunRename(nzbInfo.get());

	BOOST_CHECK_EQUAL(count, 3);
	BOOST_CHECK(fs::exists(workingDir / (METANAME + ".mkv")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + "(1).mkv")));
	BOOST_CHECK(fs::exists(workingDir / (METANAME + "(2).mkv")));

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

	int count = RunRename(nzbInfo.get());

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

	int count = RunRename(nzbInfo.get());

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

	int count = RunRename(nzbInfo.get());

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

	int count = RunRename(nzbInfo.get());

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

	int count = RunRename(nzbInfo.get());

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

	int count = RunRename(nzbInfo.get());

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

	PostInfo postInfo;
	postInfo.SetNzbInfo(nzbInfo.get());
	ObfuscatedRenamer renamer;
	int count = renamer.RenameFiles(&postInfo);

	BOOST_CHECK_EQUAL(count, 1);
	BOOST_CHECK(fs::exists(workDir / (METANAME + ".mkv")));
	BOOST_CHECK(!fs::exists(workDir / obfuscated));
	BOOST_CHECK(fs::exists(finalDir / obfuscated));

	fs::remove_all(workDir);
	fs::remove_all(destDir);
}

BOOST_AUTO_TEST_SUITE_END()
