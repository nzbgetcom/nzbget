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
#include "CollectionAnalyzer.h"
#include "PostDownloadRenamer.h"
#include <algorithm>
#include <cctype>

namespace
{
	PostDownloadRenamer::Candidate MakeTestCandidate(fs::path path, std::string filename, std::string stem, std::string ext, uintmax_t size)
	{
		PostDownloadRenamer::Candidate c;
		c.path = std::move(path);
		c.parentDir = c.path.parent_path();
		c.filename = std::move(filename);
		c.stem = std::move(stem);
		c.ext = ext;
		c.extLower = ext;
		std::transform(c.extLower.begin(), c.extLower.end(), c.extLower.begin(),
			[](unsigned char ch) { return std::tolower(ch); });
		c.size = size;
		return c;
	}
}

BOOST_AUTO_TEST_SUITE(CollectionAnalyzerTest)

BOOST_AUTO_TEST_CASE(SingletonFileTest)
{
	std::vector<PostDownloadRenamer::Candidate> candidates = {
		MakeTestCandidate("/path/to/file.mkv", "file.mkv", "file", ".mkv", 1000)
	};
	PostDownloadRenamer::CollectionAnalyzer analyzer(candidates);
	BOOST_CHECK(!analyzer.ShouldSkip(candidates[0]));
}

BOOST_AUTO_TEST_CASE(DominantPairTest)
{
	std::vector<PostDownloadRenamer::Candidate> candidates = {
		MakeTestCandidate("/path/to/big.mkv", "big.mkv", "big", ".mkv", 4000),
		MakeTestCandidate("/path/to/small.mkv", "small.mkv", "small", ".mkv", 1000)
	};
	PostDownloadRenamer::CollectionAnalyzer analyzer(candidates);
	BOOST_CHECK(!analyzer.ShouldSkip(candidates[0]));
	BOOST_CHECK(!analyzer.ShouldSkip(candidates[1]));
}

BOOST_AUTO_TEST_CASE(EqualSizePairSkippedTest)
{
	std::vector<PostDownloadRenamer::Candidate> candidates = {
		MakeTestCandidate("/path/to/file1.mkv", "file1.mkv", "file1", ".mkv", 1000),
		MakeTestCandidate("/path/to/file2.mkv", "file2.mkv", "file2", ".mkv", 1000)
	};
	PostDownloadRenamer::CollectionAnalyzer analyzer(candidates);
	BOOST_CHECK(analyzer.ShouldSkip(candidates[0]));
	BOOST_CHECK(analyzer.ShouldSkip(candidates[1]));
}

BOOST_AUTO_TEST_CASE(AudioCarveOutTest)
{
	std::vector<PostDownloadRenamer::Candidate> candidates = {
		MakeTestCandidate("/path/to/track1.mp3", "track1.mp3", "track1", ".mp3", 1000),
		MakeTestCandidate("/path/to/track2.mp3", "track2.mp3", "track2", ".mp3", 1000)
	};
	PostDownloadRenamer::CollectionAnalyzer analyzer(candidates);
	// Ambiguous audio should never be skipped (tags carry identity).
	BOOST_CHECK(!analyzer.ShouldSkip(candidates[0]));
	BOOST_CHECK(!analyzer.ShouldSkip(candidates[1]));
}

BOOST_AUTO_TEST_CASE(MixedCaseExtensionTest)
{
	std::vector<PostDownloadRenamer::Candidate> candidates = {
		MakeTestCandidate("/path/to/file1.mkv", "file1.mkv", "file1", ".mkv", 1000),
		MakeTestCandidate("/path/to/file2.MKV", "file2.MKV", "file2", ".MKV", 1000)
	};
	PostDownloadRenamer::CollectionAnalyzer analyzer(candidates);
	// Should be grouped together case-insensitively and skipped as an equal pair.
	BOOST_CHECK(analyzer.ShouldSkip(candidates[0]));
	BOOST_CHECK(analyzer.ShouldSkip(candidates[1]));
}

BOOST_AUTO_TEST_CASE(AnchorRequirementTest)
{
	std::vector<PostDownloadRenamer::Candidate> candidates = {
		MakeTestCandidate("/path/dir/file1.mkv", "file1.mkv", "file1", ".mkv", 1000),
		MakeTestCandidate("/path/dir/file2.mkv", "file2.mkv", "file2", ".mkv", 1000),
		MakeTestCandidate("/path/dir/sub.eng.srt", "sub.eng.srt", "sub.eng", ".srt", 100)
	};
	PostDownloadRenamer::CollectionAnalyzer analyzer(candidates);
	// Primaries are skipped (equal size), so companion subtitle must also be skipped (no anchor).
	BOOST_CHECK(analyzer.ShouldSkip(candidates[0]));
	BOOST_CHECK(analyzer.ShouldSkip(candidates[1]));
	BOOST_CHECK(analyzer.ShouldSkip(candidates[2]));
}

BOOST_AUTO_TEST_SUITE_END()
