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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */


#include "nzbget.h"
#include <boost/test/unit_test.hpp>
#include "CollectionAnalyzer.h"
#include "Options.h"

BOOST_AUTO_TEST_SUITE(PostprocessTest)

BOOST_AUTO_TEST_CASE(CollectionAnalyzerSingleFeatureDominanceTest)
{
	std::vector<CollectionAnalyzer::FileEntry> files = {
		{ "/path/feature.mkv", "feature.mkv", "feature", ".mkv", 4000000000ULL },
		{ "/path/sample.mkv", "sample.mkv", "sample", ".mkv", 50000000ULL },
		{ "/path/feature.en.srt", "feature.en.srt", "feature.en", ".srt", 100000ULL }
	};

	CollectionAnalyzer::AnalysisResult result = CollectionAnalyzer::Analyze(files);

	BOOST_CHECK(result.CanRename());
	BOOST_CHECK(!result.isAmbiguousCollection);
	BOOST_CHECK_EQUAL(result.mainVideo.filename, "feature.mkv");
	BOOST_CHECK_EQUAL(result.sampleVideo.filename, "sample.mkv");
	BOOST_REQUIRE_EQUAL(result.subtitles.size(), 1u);
	BOOST_CHECK_EQUAL(result.subtitles[0].filename, "feature.en.srt");
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerAmbiguousMultiEpisodeTest)
{
	// Season pack / two episodes of comparable size (e.g. 1.2 GB and 1.1 GB)
	std::vector<CollectionAnalyzer::FileEntry> files = {
		{ "/path/ep01.mkv", "ep01.mkv", "ep01", ".mkv", 1200000000ULL },
		{ "/path/ep02.mkv", "ep02.mkv", "ep02", ".mkv", 1100000000ULL }
	};

	CollectionAnalyzer::AnalysisResult result = CollectionAnalyzer::Analyze(files);

	// The 3:1 rule must prevent renaming to avoid folding multiple episodes into one
	BOOST_CHECK(result.isAmbiguousCollection);
	BOOST_CHECK(!result.CanRename());
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerDominantRatioTest)
{
	// Dominant feature: 4 GB vs 1 GB (ratio 4:1 > 3:1)
	std::vector<CollectionAnalyzer::FileEntry> files = {
		{ "/path/feature.mkv", "feature.mkv", "feature", ".mkv", 4000000000ULL },
		{ "/path/extra.mkv", "extra.mkv", "extra", ".mkv", 1000000000ULL }
	};

	CollectionAnalyzer::AnalysisResult result = CollectionAnalyzer::Analyze(files);

	BOOST_CHECK(result.CanRename());
	BOOST_CHECK(!result.isAmbiguousCollection);
	BOOST_CHECK_EQUAL(result.mainVideo.filename, "feature.mkv");
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerDiscStructureProtectionTest)
{
	std::vector<CollectionAnalyzer::FileEntry> files = {
		{ "/path/BDMV/STREAM/00001.m2ts", "00001.m2ts", "00001", ".m2ts", 20000000000ULL },
		{ "/path/BDMV/PLAYLIST/00001.mpls", "00001.mpls", "00001", ".mpls", 5000ULL }
	};

	CollectionAnalyzer::AnalysisResult result = CollectionAnalyzer::Analyze(files);

	BOOST_CHECK(result.isDiscStructure);
	BOOST_CHECK(!result.CanRename());
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerMusicAlbumProtectionTest)
{
	// Music album with tracks of similar sizes
	std::vector<CollectionAnalyzer::FileEntry> files = {
		{ "/path/01-track.flac", "01-track.flac", "01-track", ".flac", 30000000ULL },
		{ "/path/02-track.flac", "02-track.flac", "02-track", ".flac", 35000000ULL }
	};

	CollectionAnalyzer::AnalysisResult result = CollectionAnalyzer::Analyze(files);

	BOOST_CHECK(result.hasAudio);
	BOOST_CHECK(result.isAmbiguousCollection);
	BOOST_CHECK(!result.CanRename());
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerSidecarNamingTest)
{
	BOOST_CHECK_EQUAL(
		CollectionAnalyzer::ResolveSubtitleName("Movie.2026", "12345.en", ".srt"),
		"Movie.2026.en.srt");

	BOOST_CHECK_EQUAL(
		CollectionAnalyzer::ResolveSubtitleName("Movie.2026", "sub.fre", ".srt"),
		"Movie.2026.fre.srt");

	BOOST_CHECK_EQUAL(
		CollectionAnalyzer::ResolveSubtitleName("Movie.2026", "obfuscated_sub", ".srt"),
		"Movie.2026.srt");

	BOOST_CHECK_EQUAL(
		CollectionAnalyzer::ResolveSampleName("Movie.2026", ".mkv"),
		"Movie.2026-sample.mkv");
}

BOOST_AUTO_TEST_CASE(PostDownloadRenameOptionTest)
{
	Options options(nullptr, nullptr);
	BOOST_CHECK(options.GetPostDownloadRename());

	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("PostDownloadRename=no");
	Options optionsNo(&cmdOpts, nullptr);
	BOOST_CHECK(!optionsNo.GetPostDownloadRename());
}

BOOST_AUTO_TEST_CASE(DuplicateCollisionUniqueNameTest)
{
	fs::path tempDir = fs::temp_directory_path() / "nzbget_test_dupe_collision";
	fs::create_directories(tempDir);

	fs::path existingFile = tempDir / "Feature.mkv";
	std::ofstream(existingFile.string()) << "existing";

	CString uniqueName = FileSystem::MakeUniqueFilename(tempDir.string().c_str(), "Feature.mkv");
	BOOST_CHECK(strstr(uniqueName, "Feature.duplicate1.mkv") != nullptr);

	fs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerUtf8PathPreservationTest)
{
	fs::path tempDir = fs::temp_directory_path() / fs::u8path("nzbget_test_utf8_média");
	fs::create_directories(tempDir);

	fs::path videoFile = tempDir / fs::u8path("vidéo_12345.mkv");
	std::ofstream(videoFile) << "fake video data";

	CollectionAnalyzer::AnalysisResult result = CollectionAnalyzer::AnalyzeDirectory(tempDir);
	BOOST_CHECK_EQUAL(result.mainVideo.filename, "vidéo_12345.mkv");
	BOOST_CHECK(result.mainVideo.filename.find('?') == std::string::npos);

	fs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerBuildPlanTest)
{
	fs::path tempDir = fs::temp_directory_path() / "nzbget_test_build_plan";
	fs::create_directories(tempDir);

	// 32-hex obfuscated main video
	fs::path mainVideo = tempDir / "1234567890abcdef1234567890abcdef.mkv";
	std::ofstream(mainVideo.string()) << "main video data (large)";

	// Sample
	fs::path sample = tempDir / "sample.mkv";
	std::ofstream(sample.string()) << "sample";

	// Subtitle with language tag
	fs::path sub = tempDir / "random_sub.en.srt";
	std::ofstream(sub.string()) << "sub";

	CollectionAnalyzer::RenamePlan plan = CollectionAnalyzer::BuildPlan(tempDir, "Clean.Movie.2026", ".zip, .rar");

	BOOST_CHECK(plan.canRename);
	BOOST_CHECK(!plan.isAmbiguousCollection);
	BOOST_CHECK(!plan.isDiscStructure);
	BOOST_REQUIRE_EQUAL(plan.actions.size(), 3u);

	bool foundMain = false, foundSample = false, foundSub = false;
	for (const auto& action : plan.actions)
	{
		if (action.newFilename == "Clean.Movie.2026.mkv") foundMain = true;
		if (action.newFilename == "Clean.Movie.2026-sample.mkv") foundSample = true;
		if (action.newFilename == "Clean.Movie.2026.en.srt") foundSub = true;
	}
	BOOST_CHECK(foundMain);
	BOOST_CHECK(foundSample);
	BOOST_CHECK(foundSub);

	fs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerCueSheetDiscProtectionTest)
{
	std::vector<CollectionAnalyzer::FileEntry> files = {
		{ "/path/Album.cue", "Album.cue", "Album", ".cue", 1024ULL },
		{ "/path/Album.bin", "Album.bin", "Album", ".bin", 650000000ULL }
	};

	CollectionAnalyzer::AnalysisResult result = CollectionAnalyzer::Analyze(files);

	BOOST_CHECK(result.isDiscStructure);
	BOOST_CHECK(!result.CanRename());
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerClutterSkipTest)
{
	fs::path tempDir = fs::temp_directory_path() / "nzbget_test_clutter_skip";
	fs::create_directories(tempDir / "@eaDir");

	// Real video
	fs::path realVideo = tempDir / "real_video.mkv";
	std::ofstream(realVideo.string()) << "video";

	// AppleDouble resource fork sidecar
	fs::path appleDouble = tempDir / "._real_video.mkv";
	std::ofstream(appleDouble.string()) << "resource fork";

	// Desktop clutter
	fs::path dsStore = tempDir / ".DS_Store";
	std::ofstream(dsStore.string()) << "desktop";

	CollectionAnalyzer::AnalysisResult result = CollectionAnalyzer::AnalyzeDirectory(tempDir);

	// Only real_video.mkv should be found as video; ._real_video.mkv and .DS_Store must be ignored
	BOOST_CHECK_EQUAL(result.mainVideo.filename, "real_video.mkv");

	fs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerCleanVideoSidecarPairingTest)
{
	fs::path tempDir = fs::temp_directory_path() / "nzbget_test_clean_video_sidecars";
	fs::create_directories(tempDir);

	// Already clean video (not obfuscated)
	fs::path mainVideo = tempDir / "Show.S01E01.720p.mkv";
	std::ofstream(mainVideo) << "clean video data";

	// Subtitle
	fs::path sub = tempDir / "sub.en.srt";
	std::ofstream(sub) << "subtitle";

	// Target name from NZB/metadata differs slightly (e.g. show title)
	CollectionAnalyzer::RenamePlan plan = CollectionAnalyzer::BuildPlan(tempDir, "Show.S01E01", ".zip, .rar");

	BOOST_CHECK(plan.canRename);
	BOOST_REQUIRE_EQUAL(plan.actions.size(), 1u); // Only subtitle needs renaming

	// Subtitle must pair to the main video's actual stem, not targetName
	BOOST_CHECK_EQUAL(plan.actions[0].newFilename, "Show.S01E01.720p.en.srt");

	fs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerObfuscatedNzbTitleWithCleanVideoTest)
{
	fs::path tempDir = fs::temp_directory_path() / "nzbget_test_obf_nzb_clean_video";
	fs::create_directories(tempDir);

	// Clean video already on disk
	fs::path mainVideo = tempDir / "Show.S01E01.720p.mkv";
	std::ofstream(mainVideo) << "clean video";

	// Obfuscated subtitle
	fs::path sub = tempDir / "1234567890abcdef1234567890abcdef.en.srt";
	std::ofstream(sub) << "sub";

	// NZB title is an obfuscated hash without metadata
	CollectionAnalyzer::RenamePlan plan = CollectionAnalyzer::BuildPlan(
		tempDir, "1a2b3c4d5e6f7g8h9i0j1k2l", ".zip, .rar");

	BOOST_CHECK(plan.canRename);
	BOOST_REQUIRE_EQUAL(plan.actions.size(), 1u);
	// Subtitle must be successfully planned to pair with the clean video, not blocked by the NZB title
	BOOST_CHECK_EQUAL(plan.actions[0].newFilename, "Show.S01E01.720p.en.srt");

	fs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerAudioCollectionTest)
{
	fs::path tempDir = fs::temp_directory_path() / "nzbget_test_audio_collection";
	fs::create_directories(tempDir);

	// 1. Single audio file: not an ambiguous multi-file collection
	fs::path track1 = tempDir / "track01.flac";
	std::ofstream(track1) << "audio data";

	CollectionAnalyzer::AnalysisResult singleResult = CollectionAnalyzer::AnalyzeDirectory(tempDir);
	BOOST_CHECK(singleResult.hasAudio);
	BOOST_CHECK(!singleResult.isAmbiguousCollection);
	BOOST_CHECK(!singleResult.CanRename());

	// 2. Multi-track audio album: ambiguous collection
	fs::path track2 = tempDir / "track02.flac";
	std::ofstream(track2) << "audio data 2";

	CollectionAnalyzer::AnalysisResult multiResult = CollectionAnalyzer::AnalyzeDirectory(tempDir);
	BOOST_CHECK(multiResult.hasAudio);
	BOOST_CHECK(multiResult.isAmbiguousCollection);
	BOOST_CHECK(!multiResult.CanRename());

	fs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerExtensionlessVideoSniffingTest)
{
	fs::path tempDir = fs::temp_directory_path() / "nzbget_test_extless_video";
	fs::create_directories(tempDir);

	// Create an obfuscated file with NO extension, containing MKV EBML header bytes
	fs::path video = tempDir / "33f5e57280cf9321825f9243daa1dcb0";
	{
		std::ofstream ofs(video, std::ios::binary);
		uint8_t mkvHeader[] = {
			0x1A, 0x45, 0xDF, 0xA3, 0xA3, 0x42, 0x86, 0x81,
			0x01, 0x42, 0xF7, 0x81, 0x01, 0x42, 0xF2, 0x81,
			0x04, 0x42, 0xF3, 0x81, 0x08, 0x42, 0x82, 0x88,
			'm', 'a', 't', 'r', 'o', 's', 'k', 'a'
		};
		ofs.write(reinterpret_cast<const char*>(mkvHeader), sizeof(mkvHeader));
		// Pad with dummy data to give it non-zero size
		std::vector<char> pad(1024, 'X');
		ofs.write(pad.data(), pad.size());
	}

	// Accompanying obfuscated subtitle
	fs::path sub = tempDir / "33f5e57280cf9321825f9243daa1dcb0.en.srt";
	std::ofstream(sub) << "1\n00:00:01,000 --> 00:00:02,000\nHello\n";

	CollectionAnalyzer::RenamePlan plan = CollectionAnalyzer::BuildPlan(
		tempDir, "Release.Title.S01E09.1080p", ".zip, .rar");

	BOOST_CHECK(plan.canRename);
	BOOST_REQUIRE_EQUAL(plan.actions.size(), 2u);

	BOOST_CHECK_EQUAL(plan.actions[0].oldFilename, "33f5e57280cf9321825f9243daa1dcb0");
	BOOST_CHECK_EQUAL(plan.actions[0].newFilename, "Release.Title.S01E09.1080p.mkv");

	BOOST_CHECK_EQUAL(plan.actions[1].oldFilename, "33f5e57280cf9321825f9243daa1dcb0.en.srt");
	BOOST_CHECK_EQUAL(plan.actions[1].newFilename, "Release.Title.S01E09.1080p.en.srt");

	fs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerExtensionlessBookSniffingTest)
{
	fs::path tempDir = fs::temp_directory_path() / "nzbget_test_extless_book";
	fs::create_directories(tempDir);

	// Create an obfuscated file with NO extension containing EPUB magic bytes
	fs::path book = tempDir / "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4";
	{
		std::ofstream ofs(book, std::ios::binary);
		std::vector<uint8_t> epubHeader(80, 0x00);
		epubHeader[0] = 0x50; epubHeader[1] = 0x4B; epubHeader[2] = 0x03; epubHeader[3] = 0x04;
		const char* mime = "mimetypeapplication/epub+zip";
		std::memcpy(epubHeader.data() + 30, mime, std::strlen(mime));
		ofs.write(reinterpret_cast<const char*>(epubHeader.data()), epubHeader.size());
	}

	CollectionAnalyzer::RenamePlan plan = CollectionAnalyzer::BuildPlan(
		tempDir, "Author.Novel.Title.2026", ".zip, .rar");

	BOOST_CHECK(plan.canRename);
	BOOST_REQUIRE_EQUAL(plan.actions.size(), 1u);
	BOOST_CHECK_EQUAL(plan.actions[0].oldFilename, "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4");
	BOOST_CHECK_EQUAL(plan.actions[0].newFilename, "Author.Novel.Title.2026.epub");

	fs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerMultiBookAmbiguousTest)
{
	fs::path tempDir = fs::temp_directory_path() / "nzbget_test_multi_book";
	fs::create_directories(tempDir);

	fs::path book1 = tempDir / "Volume1.epub";
	fs::path book2 = tempDir / "Volume2.epub";
	std::ofstream(book1) << "book 1";
	std::ofstream(book2) << "book 2";

	CollectionAnalyzer::AnalysisResult result = CollectionAnalyzer::AnalyzeDirectory(tempDir);
	BOOST_CHECK(result.isAmbiguousCollection);
	BOOST_CHECK(!result.CanRename());

	fs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerDvdVideoTsTest)
{
	std::vector<CollectionAnalyzer::FileEntry> files = {
		{ "/path/VIDEO_TS/VIDEO_TS.BUP", "VIDEO_TS.BUP", "VIDEO_TS", ".BUP", 1024ULL },
		{ "/path/VIDEO_TS/VTS_01_1.VOB", "VTS_01_1.VOB", "VTS_01_1", ".VOB", 1073741824ULL },
		{ "/path/VIDEO_TS/VTS_01_0.IFO", "VTS_01_0.IFO", "VTS_01_0", ".IFO", 2048ULL }
	};

	CollectionAnalyzer::AnalysisResult result = CollectionAnalyzer::Analyze(files);

	BOOST_CHECK(result.isDiscStructure);
	BOOST_CHECK(!result.CanRename());
	BOOST_CHECK(result.mainVideo.filename.empty());
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerDvdAvchdTest)
{
	fs::path tempDir = fs::temp_directory_path() / "nzbget_test_dvd_avchd";
	fs::create_directories(tempDir / "AVCHD" / "BDMV" / "STREAM");

	fs::path streamFile = tempDir / "AVCHD" / "BDMV" / "STREAM" / "00001.m2ts";
	std::ofstream(streamFile) << "fake m2ts data";

	CollectionAnalyzer::AnalysisResult result = CollectionAnalyzer::AnalyzeDirectory(tempDir);

	BOOST_CHECK(result.isDiscStructure);
	BOOST_CHECK(!result.CanRename());

	fs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerMusicAlbumLargerTest)
{
	std::vector<CollectionAnalyzer::FileEntry> files = {
		{ "/path/01-intro.flac", "01-intro.flac", "01-intro", ".flac", 8000000ULL },
		{ "/path/02-main.flac", "02-main.flac", "02-main", ".flac", 45000000ULL },
		{ "/path/03-solo.flac", "03-solo.flac", "03-solo", ".flac", 38000000ULL },
		{ "/path/04-outro.flac", "04-outro.flac", "04-outro", ".flac", 12000000ULL },
		{ "/path/05-bonus.flac", "05-bonus.flac", "05-bonus", ".flac", 25000000ULL }
	};

	CollectionAnalyzer::AnalysisResult result = CollectionAnalyzer::Analyze(files);

	BOOST_CHECK(result.hasAudio);
	BOOST_CHECK(result.isAmbiguousCollection);
	BOOST_CHECK(!result.CanRename());
	BOOST_CHECK(result.mainVideo.filename.empty());
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerMixedAudioVideoTest)
{
	std::vector<CollectionAnalyzer::FileEntry> files = {
		{ "/path/feature.mkv", "feature.mkv", "feature", ".mkv", 4000000000ULL },
		{ "/path/commentary.dts", "commentary.dts", "commentary", ".dts", 600000000ULL },
		{ "/path/score.flac", "score.flac", "score", ".flac", 80000000ULL },
		{ "/path/extras.flac", "extras.flac", "extras", ".flac", 50000000ULL }
	};

	CollectionAnalyzer::AnalysisResult result = CollectionAnalyzer::Analyze(files);

	// Video dominates: audio tracks are ignored for rename purposes
	BOOST_CHECK(result.hasAudio);
	BOOST_CHECK(!result.isAmbiguousCollection);
	BOOST_CHECK(result.CanRename());
	BOOST_CHECK_EQUAL(result.mainVideo.filename, "feature.mkv");
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerPdfSniffingTest)
{
	fs::path tempDir = fs::temp_directory_path() / "nzbget_test_pdf_sniff";
	fs::create_directories(tempDir);

	// Create an obfuscated file with NO extension containing PDF magic bytes
	fs::path book = tempDir / "deadbeef0102030405060708deadbeef01";
	{
		std::ofstream ofs(book, std::ios::binary);
		// PDF: %PDF-1.x
		const char* pdfMagic = "%PDF-1.7";
		ofs.write(pdfMagic, 9);
		std::vector<char> pad(512, 'X');
		ofs.write(pad.data(), pad.size());
	}

	CollectionAnalyzer::RenamePlan plan = CollectionAnalyzer::BuildPlan(
		tempDir, "Technical.Reference.Guide.2026", ".zip, .rar");

	BOOST_CHECK(plan.canRename);
	BOOST_REQUIRE_EQUAL(plan.actions.size(), 1u);
	BOOST_CHECK_EQUAL(plan.actions[0].oldFilename, "deadbeef0102030405060708deadbeef01");
	BOOST_CHECK_EQUAL(plan.actions[0].newFilename, "Technical.Reference.Guide.2026.pdf");

	fs::remove_all(tempDir);
}

BOOST_AUTO_TEST_CASE(CollectionAnalyzerBookVsVideoTest)
{
	std::vector<CollectionAnalyzer::FileEntry> files = {
		{ "/path/feature.mkv", "feature.mkv", "feature", ".mkv", 4000000000ULL },
		{ "/path/accompanying.epub", "accompanying.epub", "accompanying", ".epub", 500000ULL }
	};

	CollectionAnalyzer::AnalysisResult result = CollectionAnalyzer::Analyze(files);

	// Video wins: book goes to otherFiles, not mainBook
	BOOST_CHECK(!result.isAmbiguousCollection);
	BOOST_CHECK(result.CanRename());
	BOOST_CHECK_EQUAL(result.mainVideo.filename, "feature.mkv");
	BOOST_CHECK(result.mainBook.filename.empty());
}

BOOST_AUTO_TEST_SUITE_END()
