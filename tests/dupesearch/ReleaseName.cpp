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
#include <filesystem>
#include <fstream>
#include <sstream>
#include "ReleaseName.h"

namespace fs = std::filesystem;

namespace
{

const char* const Luc = "Lucifer.S02E14.Candy.Morningstar.1080p.DTS-HD.MA.5.1.AVC.REMUX-FraMeSToR";

std::string Replace(std::string text, const std::string& from, const std::string& to)
{
	size_t pos = text.find(from);
	BOOST_REQUIRE(pos != std::string::npos);
	return text.replace(pos, from.size(), to);
}

std::vector<std::vector<std::string>> ReadTsv(const char* name)
{
	std::ifstream file((fs::current_path() / "dupesearch" / name).string());
	BOOST_REQUIRE_MESSAGE(file.good(), "cannot open " << name);
	std::vector<std::vector<std::string>> rows;
	std::string line;
	while (std::getline(file, line))
	{
		std::vector<std::string> fields;
		std::stringstream stream(line);
		std::string field;
		while (std::getline(stream, field, '\t'))
		{
			fields.push_back(field);
		}
		rows.push_back(fields);
	}
	return rows;
}

}

BOOST_AUTO_TEST_SUITE(DupeSearchTest)

BOOST_AUTO_TEST_CASE(ReleaseNameNormalizeTest)
{
	BOOST_CHECK_EQUAL(ReleaseName::Normalize("Lucifer S02E14 1080p BluRay x264-DEFLATE.nzb"),
		"lucifer.s02e14.1080p.bluray.x264.deflate");
	BOOST_CHECK_EQUAL(ReleaseName::Normalize("Some.Movie.2020.1080p.WEB-DL-GRP-xpost [nzbgeek].nzb"),
		"some.movie.2020.1080p.web.dl.grp");
	BOOST_CHECK_EQUAL(ReleaseName::Normalize("A_B--C.mkv"), "a.b.c");
	BOOST_CHECK_EQUAL(ReleaseName::Normalize("Show.S01E01.1080p.WEB.H264-GRP"), "show.s01e01.1080p.web.h264.grp");

	// the codec of "H.265" is not a volume suffix; a split volume is
	BOOST_CHECK_EQUAL(ReleaseName::Normalize("Movie.2020.1080p.WEB.DDP5.1.H.265"), "movie.2020.1080p.web.ddp5.1.h.265");
	BOOST_CHECK_EQUAL(ReleaseName::Normalize("Movie.2020.1080p.WEB-GRP.mkv.001"), "movie.2020.1080p.web.grp");
}

BOOST_AUTO_TEST_CASE(ReleaseNameShortQueryTest)
{
	BOOST_CHECK_EQUAL(ReleaseName::ShortQuery(
		"Lucifer.S02E14.Candy.Morningstar.1080p.DTS-HD.MA.5.1.AVC.REMUX-FraMeST"), "lucifer s02e14 1080p");
	BOOST_CHECK_EQUAL(ReleaseName::ShortQuery("Some.Movie.2020.Extended.2160p.UHD-GRP"), "some movie 2020 2160p");
	BOOST_CHECK_EQUAL(ReleaseName::ShortQuery("NoMarkers-GRP"), "");

	for (const auto& row : ReadTsv("short_query.tsv"))
	{
		BOOST_REQUIRE_EQUAL(row.size(), 2U);
		BOOST_CHECK_EQUAL(ReleaseName::ShortQuery(row[0]), row[1]);
	}
}

BOOST_AUTO_TEST_CASE(ReleaseNameSameReleaseTest)
{
	// formatting and size are ignored
	BOOST_CHECK(ReleaseName::SameRelease(Luc, "Lucifer S02E14 Candy Morningstar 1080p DTS-HD MA 5 1 AVC REMUX-FraMeSToR"));
	BOOST_CHECK(ReleaseName::SameRelease(Luc, "lucifer.s02e14.candy.morningstar.1080p.dts-hd.ma.5.1.avc.remux-framestor.mkv"));

	// another group, resolution, episode, repack, audio, year or network is another release
	BOOST_CHECK(!ReleaseName::SameRelease(Luc, Replace(Luc, "FraMeSToR", "EPSiLON")));
	BOOST_CHECK(!ReleaseName::SameRelease(Luc, Replace(Luc, "1080p", "2160p")));
	BOOST_CHECK(!ReleaseName::SameRelease(Luc, Replace(Luc, "S02E14", "S02E15")));
	BOOST_CHECK(!ReleaseName::SameRelease(Luc, Replace(Luc, "1080p", "REPACK.1080p")));
	BOOST_CHECK(!ReleaseName::SameRelease(Luc, Replace(Luc, "DTS-HD.MA.5.1", "DDP5.1")));
	BOOST_CHECK(!ReleaseName::SameRelease("Movie.2020.1080p.BluRay.x264-GRP", "Movie.2021.1080p.BluRay.x264-GRP"));
	BOOST_CHECK(!ReleaseName::SameRelease("Show.S01E01.1080p.NF.WEB-DL.DDP5.1.H.264-GRP",
		"Show.S01E01.1080p.AMZN.WEB-DL.DDP5.1.H.264-GRP"));

	// an attribute only one name carries is no conflict, but a group is required
	BOOST_CHECK(ReleaseName::SameRelease(Luc, "Lucifer.S02E14.1080p.BluRay.REMUX.AVC-FraMeSToR"));
	BOOST_CHECK(!ReleaseName::SameRelease(Luc, "Lucifer.S02E14.1080p.REMUX"));
}

BOOST_AUTO_TEST_CASE(ReleaseNameHdrTest)
{
	// HDR formats must match exactly: no tag means SDR
	std::string a = "Shrinking.S02E06.2160p.ATVP.WEB-DL.DDPA5.1.HDR.DV.HEVC-NTb";
	BOOST_CHECK(ReleaseName::SameRelease(a, "Shrinking.S02E06.In.a.Lonely.Place.2160p.ATVP.WEB-DL.DDP5.1.DV.HDR.H.265-NTb"));
	BOOST_CHECK(!ReleaseName::SameRelease(a, "Shrinking.S02E06.In.a.Lonely.Place.2160p.ATVP.WEB-DL.DDP5.1.H.265-NTb"));
	BOOST_CHECK(!ReleaseName::SameRelease(a, "Shrinking.S02E06.In.a.Lonely.Place.2160p.ATVP.WEB-DL.DDP5.1.DV.H.265-NTb"));
	BOOST_CHECK(ReleaseName::SameRelease("Shrinking.S01E10.Closure.2160p.ATVP.WEB-DL.DDP5.1.DoVi.H.265-NTb",
		"Shrinking S01E10 Closure 2160p ATVP WEB-DL DDP5 1 DV H 265-NTb"));
}

BOOST_AUTO_TEST_CASE(ReleaseNameCodecTest)
{
	BOOST_CHECK(!ReleaseName::SameRelease("Movie.2020.1080p.WEB.DDP5.1.H.264", "Movie.2020.1080p.WEB.DDP5.1.H.265"));
	BOOST_CHECK(!ReleaseName::SameRelease("Movie.2020.1080p.WEB.DDP5.1.H.264-GRP", "Movie.2020.1080p.WEB.DDP5.1.H.265-GRP"));
	BOOST_CHECK(ReleaseName::SameRelease("Movie.2020.1080p.WEB.DDP5.1.H.265-GRP", "Movie 2020 1080p WEB DDP5 1 x265-GRP"));
}

BOOST_AUTO_TEST_CASE(ReleaseNameReadableTest)
{
	BOOST_CHECK(ReleaseName::Readable("Lucifer.S02E14.1080p.WEB.H264-GRP.mkv"));
	BOOST_CHECK(!ReleaseName::Readable("53a9sd8antphskzkulcik6yrrktj1qft.7z.001"));
	BOOST_CHECK(!ReleaseName::Readable("e630b420289687dc3c66cc3274207902.part01.rar"));
}

// B9: a ranged multi-episode posting is episodes 1 and 2, never episode 1 alone
BOOST_AUTO_TEST_CASE(ReleaseNameEpisodeRangeTest)
{
	std::vector<int> both = { 1, 2 };
	BOOST_CHECK(ReleaseName::Parse("Show.S01E01-E02.1080p.WEB.h264-GRP").episodes == both);
	BOOST_CHECK(ReleaseName::Parse("Show.S01E01-02.1080p.WEB.h264-GRP").episodes == both);
	std::vector<int> three = { 1, 2, 3 };
	BOOST_CHECK(ReleaseName::Parse("Show.S01E01-E03.1080p.WEB.h264-GRP").episodes == three);
	BOOST_CHECK(!ReleaseName::SameRelease("Show.S01E01-E02.1080p.WEB.h264-GRP", "Show.S01E01.1080p.WEB.h264-GRP"));
	BOOST_CHECK(ReleaseName::SameRelease("Show.S01E01-E02.1080p.WEB.h264-GRP", "Show.S01E01E02.1080p.WEB.h264-GRP"));
	// a number after the episode that isn't a range stays title or junk
	BOOST_CHECK(ReleaseName::Parse("Show.S01E01.1080p.WEB.h264-GRP").episodes == std::vector<int>{ 1 });
}

// B10: numbered repacks and propers are repacks and propers
BOOST_AUTO_TEST_CASE(ReleaseNameNumberedRepackTest)
{
	BOOST_CHECK(!ReleaseName::SameRelease("Show.S01E01.REPACK2.1080p.WEB.h264-GRP", "Show.S01E01.1080p.WEB.h264-GRP"));
	BOOST_CHECK(!ReleaseName::SameRelease("Show.S01E01.PROPER2.1080p.WEB.h264-GRP", "Show.S01E01.1080p.WEB.h264-GRP"));
	BOOST_CHECK(ReleaseName::Parse("Show.S01E01.RERIP3.1080p.WEB.h264-GRP").repack);
	BOOST_CHECK(ReleaseName::SameRelease("Show.S01E01.REPACK2.1080p.WEB.h264-GRP", "Show S01E01 REPACK2 1080p WEB h264-GRP"));
}

// B11: every spelling of Dolby Vision is DV
BOOST_AUTO_TEST_CASE(ReleaseNameDolbyVisionSpellingsTest)
{
	for (const char* name : { "Movie.2020.2160p.DolbyVision.WEB.h265-GRP", "Movie.2020.2160p.Dolby-Vision.WEB.h265-GRP",
		"Movie.2020.2160p.Dolby_Vision.WEB.h265-GRP", "Movie 2020 2160p Dolby Vision WEB h265-GRP" })
	{
		BOOST_CHECK_MESSAGE(ReleaseName::Parse(name).hdr == std::set<std::string>{ "dv" }, name);
		BOOST_CHECK_MESSAGE(ReleaseName::SameRelease(name, "Movie.2020.2160p.DV.WEB.h265-GRP"), name);
		BOOST_CHECK_MESSAGE(!ReleaseName::SameRelease(name, "Movie.2020.2160p.WEB.h265-GRP"), name);
	}
}

// B12: a title that starts with a year-like number keeps it as title
BOOST_AUTO_TEST_CASE(ReleaseNameYearTitleTest)
{
	BOOST_CHECK(!ReleaseName::SameRelease("2001.A.Space.Odyssey.1968.1080p.BluRay.x264-GRP", "2001.Maniacs.2005.1080p.BluRay.x264-GRP"));
	ReleaseName::Attrs odyssey = ReleaseName::Parse("2001.A.Space.Odyssey.1968.1080p.BluRay.x264-GRP");
	BOOST_CHECK_EQUAL(odyssey.title, "2001aspaceodyssey");
	BOOST_CHECK_EQUAL(odyssey.year, 1968);
	ReleaseName::Attrs war = ReleaseName::Parse("1917.2019.1080p.BluRay.x264-GRP");
	BOOST_CHECK_EQUAL(war.title, "1917");
	BOOST_CHECK_EQUAL(war.year, 2019);
	BOOST_CHECK_EQUAL(ReleaseName::Parse("2012.1080p.BluRay.x264-GRP").title, "2012");
	BOOST_CHECK(ReleaseName::Readable("1917.2019.1080p.BluRay.x264-GRP.mkv"));
	BOOST_CHECK(!ReleaseName::SameRelease("1917.2019.1080p.BluRay.x264-GRP", "2012.2009.1080p.BluRay.x264-GRP"));
	// a show with a year after its title is unchanged
	ReleaseName::Attrs show = ReleaseName::Parse("The.Paper.2025.S01E01.1080p.WEB.h264-GRP");
	BOOST_CHECK_EQUAL(show.title, "thepaper");
	BOOST_CHECK_EQUAL(show.year, 2025);
}

// B13: interlaced and progressive are different encodes
BOOST_AUTO_TEST_CASE(ReleaseNameInterlacedTest)
{
	BOOST_CHECK(!ReleaseName::SameRelease("Show.S01E01.1080i.HDTV.h264-GRP", "Show.S01E01.1080p.HDTV.h264-GRP"));
	BOOST_CHECK(ReleaseName::SameRelease("Show.S01E01.1080i.HDTV.h264-GRP", "Show S01E01 1080i HDTV h264-GRP"));
}

// 144 live indexer titles for three primaries: the verdict and the normalized name
BOOST_AUTO_TEST_CASE(ReleaseNameVectorsTest)
{
	std::vector<std::vector<std::string>> rows = ReadTsv("release_matching.tsv");
	BOOST_REQUIRE_EQUAL(rows.size(), 144U);
	int mismatches = 0;
	for (const auto& row : rows)
	{
		BOOST_REQUIRE_EQUAL(row.size(), 4U);
		bool expected = row[2] == "1";
		bool actual = ReleaseName::SameRelease(row[0], row[1]);
		if (actual != expected)
		{
			mismatches++;
			BOOST_ERROR("same release of '" << row[0] << "' and '" << row[1] << "': expected "
				<< expected << ", got " << actual);
		}
		BOOST_CHECK_EQUAL(ReleaseName::Normalize(row[1]), row[3]);
	}
	BOOST_CHECK_EQUAL(mismatches, 0);
}

BOOST_AUTO_TEST_SUITE_END()
