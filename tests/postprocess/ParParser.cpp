/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 NZBGet Contributors
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
#include "ParParser.h"

BOOST_AUTO_TEST_SUITE(PostprocessTest)

BOOST_AUTO_TEST_CASE(ParParserStandardFormatsTest)
{
	int baseLen = -1;
	int blocks = -1;

	// Main index file
	BOOST_CHECK(ParParser::ParseParFilename("movie.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 5);
	BOOST_CHECK_EQUAL(blocks, 0);

	// Standard PAR2 recovery volume (.volNN+MM)
	BOOST_CHECK(ParParser::ParseParFilename("movie.vol00+04.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 5);
	BOOST_CHECK_EQUAL(blocks, 4);

	// Explicit zero block count
	BOOST_CHECK(ParParser::ParseParFilename("movie.vol00+00.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 5);
	BOOST_CHECK_EQUAL(blocks, 0);

	// Legacy dash format with start and count (.volNN-MM)
	BOOST_CHECK(ParParser::ParseParFilename("movie.vol00-04.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 5);
	BOOST_CHECK_EQUAL(blocks, 4);

	// Uppercase / mixed-case filenames
	BOOST_CHECK(ParParser::ParseParFilename("MOVIE.VOL01+12.PAR2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 5);
	BOOST_CHECK_EQUAL(blocks, 12);

	// Non-par2 or too short filenames
	BOOST_CHECK(!ParParser::ParseParFilename("", true, &baseLen, &blocks));
	BOOST_CHECK(!ParParser::ParseParFilename(".par2", true, &baseLen, &blocks));
	BOOST_CHECK(!ParParser::ParseParFilename("movie.rar", true, &baseLen, &blocks));
	BOOST_CHECK(!ParParser::ParseParFilename("movie.vol-01.par2.part1", true, &baseLen, &blocks));
}

BOOST_AUTO_TEST_CASE(ParParserUnknownBlockCountVolumesTest)
{
	int baseLen = -1;
	int blocks = 0;

	// .vol-00 (0-indexed volume without block count)
	BOOST_CHECK(ParParser::ParseParFilename("movie.vol-00.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 5);
	BOOST_CHECK_EQUAL(blocks, ParParser::UNKNOWN_BLOCKS);

	// .vol-01
	BOOST_CHECK(ParParser::ParseParFilename("movie.vol-01.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 5);
	BOOST_CHECK_EQUAL(blocks, ParParser::UNKNOWN_BLOCKS);

	// Dash in base name + .vol-01
	BOOST_CHECK(ParParser::ParseParFilename("my-movie.vol-01.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 8);
	BOOST_CHECK_EQUAL(blocks, ParParser::UNKNOWN_BLOCKS);

	// Dots in base name + .vol-01
	BOOST_CHECK(ParParser::ParseParFilename("my.movie.2026.vol-01.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 13);
	BOOST_CHECK_EQUAL(blocks, ParParser::UNKNOWN_BLOCKS);

	// .vol01 (volume number without separator or block count)
	BOOST_CHECK(ParParser::ParseParFilename("movie.vol01.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 5);
	BOOST_CHECK_EQUAL(blocks, ParParser::UNKNOWN_BLOCKS);

	// Uppercase variants
	BOOST_CHECK(ParParser::ParseParFilename("MOVIE.VOL-02.PAR2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 5);
	BOOST_CHECK_EQUAL(blocks, ParParser::UNKNOWN_BLOCKS);

	BOOST_CHECK(ParParser::ParseParFilename("Movie.Vol03.Par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 5);
	BOOST_CHECK_EQUAL(blocks, ParParser::UNKNOWN_BLOCKS);
}

BOOST_AUTO_TEST_CASE(ParParserUnconfirmedFilenameAndOverflowTest)
{
	int baseLen = -1;
	int blocks = 0;

	// Raw Usenet subject with trailing text after .par2
	const char* subject1 = "[1/5] - \"movie.vol-01.par2\" yEnc (1/20)";
	BOOST_CHECK(ParParser::ParseParFilename(subject1, false, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(std::string(subject1, baseLen), "[1/5] - \"movie");
	BOOST_CHECK_EQUAL(blocks, ParParser::UNKNOWN_BLOCKS);

	const char* subject2 = "[1/5] - \"movie.vol01.par2\" yEnc (1/20)";
	BOOST_CHECK(ParParser::ParseParFilename(subject2, false, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(std::string(subject2, baseLen), "[1/5] - \"movie");
	BOOST_CHECK_EQUAL(blocks, ParParser::UNKNOWN_BLOCKS);

	BOOST_CHECK(ParParser::ParseParFilename("movie.vol-01.par2.part1", false, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 5);
	BOOST_CHECK_EQUAL(blocks, ParParser::UNKNOWN_BLOCKS);

	// Huge block counts must clamp cleanly to INT_MAX without UB
	BOOST_CHECK(ParParser::ParseParFilename("movie.vol00+99999999999999999999999.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 5);
	BOOST_CHECK_EQUAL(blocks, std::numeric_limits<int>::max());

	BOOST_CHECK(ParParser::ParseParFilename("movie.vol00+3000000000.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 5);
	BOOST_CHECK_EQUAL(blocks, std::numeric_limits<int>::max());
}

BOOST_AUTO_TEST_CASE(ParParserFalsePositivesTest)
{
	int baseLen = -1;
	int blocks = -1;

	// Must NOT truncate base name or mark as recovery volume
	BOOST_CHECK(ParParser::ParseParFilename("Kill.Bill.Volume1.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 17);
	BOOST_CHECK_EQUAL(blocks, 0);

	BOOST_CHECK(ParParser::ParseParFilename("movie.vol.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 9);
	BOOST_CHECK_EQUAL(blocks, 0);

	BOOST_CHECK(ParParser::ParseParFilename("movie.vol-.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 10);
	BOOST_CHECK_EQUAL(blocks, 0);

	BOOST_CHECK(ParParser::ParseParFilename("movie.vol-cut.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 13);
	BOOST_CHECK_EQUAL(blocks, 0);

	BOOST_CHECK(ParParser::ParseParFilename("movie.vol-01-cut.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 16);
	BOOST_CHECK_EQUAL(blocks, 0);

	BOOST_CHECK(ParParser::ParseParFilename("movie.vol01abc.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 14);
	BOOST_CHECK_EQUAL(blocks, 0);

	BOOST_CHECK(ParParser::ParseParFilename("movie.vol01+.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 12);
	BOOST_CHECK_EQUAL(blocks, 0);

	BOOST_CHECK(ParParser::ParseParFilename("movie.vol01+04abc.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 17);
	BOOST_CHECK_EQUAL(blocks, 0);

	// Leading .vol with no base name prefix must not truncate to empty baseNameLen
	BOOST_CHECK(ParParser::ParseParFilename(".vol-01.par2", true, &baseLen, &blocks));
	BOOST_CHECK_EQUAL(baseLen, 7);
	BOOST_CHECK_EQUAL(blocks, 0);
}

BOOST_AUTO_TEST_CASE(ParParserSameParCollectionTest)
{
	BOOST_CHECK(ParParser::SameParCollection("movie.par2", "movie.vol00+04.par2", true));
	BOOST_CHECK(ParParser::SameParCollection("movie.par2", "movie.vol-00.par2", true));
	BOOST_CHECK(ParParser::SameParCollection("movie.par2", "movie.vol-01.par2", true));
	BOOST_CHECK(ParParser::SameParCollection("movie.par2", "movie.vol01.par2", true));
	BOOST_CHECK(ParParser::SameParCollection("my-movie.par2", "my-movie.vol-01.par2", true));
	BOOST_CHECK(ParParser::SameParCollection("movie.vol-00.par2", "MOVIE.VOL-01.PAR2", true));

	BOOST_CHECK(!ParParser::SameParCollection("movie.par2", "movie.volume1.par2", true));
	BOOST_CHECK(!ParParser::SameParCollection("movie.par2", "movie.vol-cut.par2", true));
	BOOST_CHECK(!ParParser::SameParCollection("movie.par2", "other.vol-01.par2", true));
}

BOOST_AUTO_TEST_SUITE_END()
