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

#include "FileTypes.h"

BOOST_AUTO_TEST_SUITE(UtilTest)

BOOST_AUTO_TEST_CASE(IsSevenZipExtTest)
{
	BOOST_CHECK(FileTypes::IsSevenZipExt(".7z"));
	BOOST_CHECK(FileTypes::IsSevenZipExt(".zip"));
	BOOST_CHECK(FileTypes::IsSevenZipExt(".tar"));
	BOOST_CHECK(FileTypes::IsSevenZipExt(".gz"));
	BOOST_CHECK(FileTypes::IsSevenZipExt(".bz"));
	BOOST_CHECK(FileTypes::IsSevenZipExt(".bz2"));
	BOOST_CHECK(FileTypes::IsSevenZipExt(".tgz"));
	BOOST_CHECK(FileTypes::IsSevenZipExt(".txz"));

	// Bare ".xz" is intentionally supported (single-file xz-compressed archives),
	// in addition to the ".tar.xz"-style ".txz" extension. Locked in by this test
	// so it isn't silently dropped or re-added by future refactors.
	BOOST_CHECK(FileTypes::IsSevenZipExt(".xz"));

	// case-insensitive
	BOOST_CHECK(FileTypes::IsSevenZipExt(".XZ"));
	BOOST_CHECK(FileTypes::IsSevenZipExt(".ZIP"));

	BOOST_CHECK(!FileTypes::IsSevenZipExt(".rar"));
	BOOST_CHECK(!FileTypes::IsSevenZipExt(".mkv"));
	BOOST_CHECK(!FileTypes::IsSevenZipExt(""));
}

BOOST_AUTO_TEST_CASE(IsRarExtTest)
{
	BOOST_CHECK(FileTypes::IsRarExt(".rar"));
	BOOST_CHECK(FileTypes::IsRarExt(".RAR"));
	BOOST_CHECK(!FileTypes::IsRarExt(".r00"));
	BOOST_CHECK(!FileTypes::IsRarExt(".zip"));
	BOOST_CHECK(!FileTypes::IsRarExt(""));
}

BOOST_AUTO_TEST_CASE(IsRarVolumeExtTest)
{
	BOOST_CHECK(FileTypes::IsRarVolumeExt(".r00"));
	BOOST_CHECK(FileTypes::IsRarVolumeExt(".r99"));
	BOOST_CHECK(FileTypes::IsRarVolumeExt(".z01"));
	BOOST_CHECK(FileTypes::IsRarVolumeExt(".R00"));
	BOOST_CHECK(FileTypes::IsRarVolumeExt(".r100"));

	BOOST_CHECK(!FileTypes::IsRarVolumeExt(".rar"));
	BOOST_CHECK(!FileTypes::IsRarVolumeExt(".a00"));
	BOOST_CHECK(!FileTypes::IsRarVolumeExt(".r0"));
	BOOST_CHECK(!FileTypes::IsRarVolumeExt(""));
}

BOOST_AUTO_TEST_CASE(IsNumericVolumeExtTest)
{
	BOOST_CHECK(FileTypes::IsNumericVolumeExt(".000"));
	BOOST_CHECK(FileTypes::IsNumericVolumeExt(".001"));
	BOOST_CHECK(FileTypes::IsNumericVolumeExt(".999"));
	BOOST_CHECK(FileTypes::IsNumericVolumeExt(".0000"));

	BOOST_CHECK(!FileTypes::IsNumericVolumeExt(".00"));
	BOOST_CHECK(!FileTypes::IsNumericVolumeExt(".abc"));
	BOOST_CHECK(!FileTypes::IsNumericVolumeExt(""));
}

BOOST_AUTO_TEST_CASE(IsAllDigitsExtTest)
{
	BOOST_CHECK(FileTypes::IsAllDigitsExt(".1"));
	BOOST_CHECK(FileTypes::IsAllDigitsExt(".15"));
	BOOST_CHECK(FileTypes::IsAllDigitsExt(".001"));
	BOOST_CHECK(FileTypes::IsAllDigitsExt(".0000"));
	BOOST_CHECK(FileTypes::IsAllDigitsExt(".123456"));

	BOOST_CHECK(!FileTypes::IsAllDigitsExt(""));
	BOOST_CHECK(!FileTypes::IsAllDigitsExt("."));
	BOOST_CHECK(!FileTypes::IsAllDigitsExt(".a"));
	BOOST_CHECK(!FileTypes::IsAllDigitsExt(".1a"));
	BOOST_CHECK(!FileTypes::IsAllDigitsExt("1"));
}

BOOST_AUTO_TEST_CASE(IsArchiveExtTest)
{
	BOOST_CHECK(FileTypes::IsArchiveExt(".7z"));
	BOOST_CHECK(FileTypes::IsArchiveExt(".xz"));
	BOOST_CHECK(FileTypes::IsArchiveExt(".rar"));
	BOOST_CHECK(FileTypes::IsArchiveExt(".r00"));
	BOOST_CHECK(FileTypes::IsArchiveExt(".001"));

	BOOST_CHECK(!FileTypes::IsArchiveExt(".mkv"));
	BOOST_CHECK(!FileTypes::IsArchiveExt(".par2"));
	BOOST_CHECK(!FileTypes::IsArchiveExt(""));
}

BOOST_AUTO_TEST_CASE(IsDiscStructureExtTest)
{
	BOOST_CHECK(FileTypes::IsDiscStructureExt(".vob"));
	BOOST_CHECK(FileTypes::IsDiscStructureExt(".bdmv"));
	BOOST_CHECK(FileTypes::IsDiscStructureExt(".ifo"));
	BOOST_CHECK(!FileTypes::IsDiscStructureExt(".mkv"));
}

BOOST_AUTO_TEST_CASE(IsParityExtTest)
{
	BOOST_CHECK(FileTypes::IsParityExt(".par2"));
	BOOST_CHECK(FileTypes::IsParityExt(".sfv"));
	BOOST_CHECK(FileTypes::IsParityExt(".PAR2"));
	BOOST_CHECK(!FileTypes::IsParityExt(".par"));
}

BOOST_AUTO_TEST_CASE(IsVideoExtTest)
{
	BOOST_CHECK(FileTypes::IsVideoExt(".mkv"));
	BOOST_CHECK(FileTypes::IsVideoExt(".mp4"));
	BOOST_CHECK(FileTypes::IsVideoExt(".MKV"));
	BOOST_CHECK(!FileTypes::IsVideoExt(".mp3"));
}

BOOST_AUTO_TEST_CASE(IsAudioExtTest)
{
	BOOST_CHECK(FileTypes::IsAudioExt(".mp3"));
	BOOST_CHECK(FileTypes::IsAudioExt(".flac"));
	BOOST_CHECK(!FileTypes::IsAudioExt(".mp4"));
}

BOOST_AUTO_TEST_CASE(IsSubtitleExtTest)
{
	BOOST_CHECK(FileTypes::IsSubtitleExt(".srt"));
	BOOST_CHECK(FileTypes::IsSubtitleExt(".ass"));
	BOOST_CHECK(!FileTypes::IsSubtitleExt(".mkv"));
}

BOOST_AUTO_TEST_CASE(IsSampleStemTest)
{
	BOOST_CHECK(FileTypes::IsSampleStem("movie-sample"));
	BOOST_CHECK(FileTypes::IsSampleStem("movie.sample"));
	BOOST_CHECK(FileTypes::IsSampleStem("movie_sample"));
	BOOST_CHECK(FileTypes::IsSampleStem("sample"));
	BOOST_CHECK(FileTypes::IsSampleStem("Sample"));
	BOOST_CHECK(!FileTypes::IsSampleStem("moviesample"));
}

BOOST_AUTO_TEST_SUITE_END()
