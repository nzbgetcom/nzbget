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

BOOST_AUTO_TEST_SUITE_END()
