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
#include "FileTypes.h"

BOOST_AUTO_TEST_SUITE(UtilTest)

BOOST_AUTO_TEST_CASE(IsSevenZipExtTest)
{
	BOOST_CHECK(FileTypes::IsSevenZipExt(".7z"));
	BOOST_CHECK(FileTypes::IsSevenZipExt(".7Z"));
	BOOST_CHECK(FileTypes::IsSevenZipExt(".zip"));
	BOOST_CHECK(FileTypes::IsSevenZipExt(".ZIP"));
	BOOST_CHECK(FileTypes::IsSevenZipExt(".tar"));
	BOOST_CHECK(FileTypes::IsSevenZipExt(".gz"));
	BOOST_CHECK(FileTypes::IsSevenZipExt(".bz"));
	BOOST_CHECK(FileTypes::IsSevenZipExt(".bz2"));
	BOOST_CHECK(FileTypes::IsSevenZipExt(".tgz"));
	BOOST_CHECK(FileTypes::IsSevenZipExt(".txz"));
	BOOST_CHECK(FileTypes::IsSevenZipExt(".xz"));

	BOOST_CHECK(!FileTypes::IsSevenZipExt(".rar"));
	BOOST_CHECK(!FileTypes::IsSevenZipExt(".r00"));
	BOOST_CHECK(!FileTypes::IsSevenZipExt(".001"));
	BOOST_CHECK(!FileTypes::IsSevenZipExt(""));
	BOOST_CHECK(!FileTypes::IsSevenZipExt(".7z.001"));
}

BOOST_AUTO_TEST_CASE(IsRarExtTest)
{
	BOOST_CHECK(FileTypes::IsRarExt(".rar"));
	BOOST_CHECK(FileTypes::IsRarExt(".RAR"));

	BOOST_CHECK(!FileTypes::IsRarExt(".r00"));
	BOOST_CHECK(!FileTypes::IsRarExt(".zip"));
	BOOST_CHECK(!FileTypes::IsRarExt(".7z"));
	BOOST_CHECK(!FileTypes::IsRarExt(""));
}

BOOST_AUTO_TEST_CASE(IsRarVolumeExtTest)
{
	BOOST_CHECK(FileTypes::IsRarVolumeExt(".r00"));
	BOOST_CHECK(FileTypes::IsRarVolumeExt(".r99"));
	BOOST_CHECK(FileTypes::IsRarVolumeExt(".z00"));
	BOOST_CHECK(FileTypes::IsRarVolumeExt(".z99"));
	BOOST_CHECK(FileTypes::IsRarVolumeExt(".R00"));
	BOOST_CHECK(FileTypes::IsRarVolumeExt(".s00"));

	BOOST_CHECK(!FileTypes::IsRarVolumeExt(".rar"));
	BOOST_CHECK(!FileTypes::IsRarVolumeExt(".000"));
	BOOST_CHECK(!FileTypes::IsRarVolumeExt(".a00"));
	BOOST_CHECK(!FileTypes::IsRarVolumeExt(".r0"));
	BOOST_CHECK(!FileTypes::IsRarVolumeExt(".r0000"));
	BOOST_CHECK(!FileTypes::IsRarVolumeExt(""));
}

BOOST_AUTO_TEST_CASE(IsNumericVolumeExtTest)
{
	BOOST_CHECK(FileTypes::IsNumericVolumeExt(".000"));
	BOOST_CHECK(FileTypes::IsNumericVolumeExt(".001"));
	BOOST_CHECK(FileTypes::IsNumericVolumeExt(".999"));

	BOOST_CHECK(!FileTypes::IsNumericVolumeExt(".0000"));
	BOOST_CHECK(!FileTypes::IsNumericVolumeExt(".00"));
	BOOST_CHECK(!FileTypes::IsNumericVolumeExt(".00000"));
	BOOST_CHECK(!FileTypes::IsNumericVolumeExt(".r00"));
	BOOST_CHECK(!FileTypes::IsNumericVolumeExt(".abc"));
	BOOST_CHECK(!FileTypes::IsNumericVolumeExt(""));
}

BOOST_AUTO_TEST_CASE(IsAllDigitsExtTest)
{
	BOOST_CHECK(FileTypes::IsAllDigitsExt(".1"));
	BOOST_CHECK(FileTypes::IsAllDigitsExt(".15"));
	BOOST_CHECK(FileTypes::IsAllDigitsExt(".001"));
	BOOST_CHECK(FileTypes::IsAllDigitsExt(".0000"));
	BOOST_CHECK(FileTypes::IsAllDigitsExt(".999"));

	BOOST_CHECK(!FileTypes::IsAllDigitsExt("."));
	BOOST_CHECK(!FileTypes::IsAllDigitsExt(""));
	BOOST_CHECK(!FileTypes::IsAllDigitsExt(".abc"));
	BOOST_CHECK(!FileTypes::IsAllDigitsExt(".1a"));
}

BOOST_AUTO_TEST_CASE(IsArchiveExtTest)
{
	BOOST_CHECK(FileTypes::IsArchiveExt(".rar"));
	BOOST_CHECK(FileTypes::IsArchiveExt(".7z"));
	BOOST_CHECK(FileTypes::IsArchiveExt(".zip"));
	BOOST_CHECK(FileTypes::IsArchiveExt(".r00"));
	BOOST_CHECK(FileTypes::IsArchiveExt(".001"));

	BOOST_CHECK(!FileTypes::IsArchiveExt(".mkv"));
	BOOST_CHECK(!FileTypes::IsArchiveExt(".mp4"));
	BOOST_CHECK(!FileTypes::IsArchiveExt(".par2"));
	BOOST_CHECK(!FileTypes::IsArchiveExt(""));
}

BOOST_AUTO_TEST_CASE(IsDiscStructureExtTest)
{
	BOOST_CHECK(FileTypes::IsDiscStructureExt(".vob"));
	BOOST_CHECK(FileTypes::IsDiscStructureExt(".bdmv"));
	BOOST_CHECK(FileTypes::IsDiscStructureExt(".mpls"));
	BOOST_CHECK(FileTypes::IsDiscStructureExt(".mpl"));
	BOOST_CHECK(FileTypes::IsDiscStructureExt(".clpi"));
	BOOST_CHECK(FileTypes::IsDiscStructureExt(".cpi"));
	BOOST_CHECK(FileTypes::IsDiscStructureExt(".bdm"));
	BOOST_CHECK(FileTypes::IsDiscStructureExt(".ifo"));
	BOOST_CHECK(FileTypes::IsDiscStructureExt(".bup"));
	BOOST_CHECK(FileTypes::IsDiscStructureExt(".mts"));
	BOOST_CHECK(FileTypes::IsDiscStructureExt(".m2ts"));

	BOOST_CHECK(!FileTypes::IsDiscStructureExt(".rar"));
	BOOST_CHECK(!FileTypes::IsDiscStructureExt(".mkv"));
	BOOST_CHECK(!FileTypes::IsDiscStructureExt(""));
}

BOOST_AUTO_TEST_CASE(IsParityExtTest)
{
	BOOST_CHECK(FileTypes::IsParityExt(".par2"));
	BOOST_CHECK(FileTypes::IsParityExt(".PAR2"));
	BOOST_CHECK(FileTypes::IsParityExt(".sfv"));
	BOOST_CHECK(FileTypes::IsParityExt(".SFV"));

	BOOST_CHECK(!FileTypes::IsParityExt(".rar"));
	BOOST_CHECK(!FileTypes::IsParityExt(".par"));
	BOOST_CHECK(!FileTypes::IsParityExt(""));
}

BOOST_AUTO_TEST_CASE(IsVideoExtTest)
{
	BOOST_CHECK(FileTypes::IsVideoExt(".mkv"));
	BOOST_CHECK(FileTypes::IsVideoExt(".mp4"));
	BOOST_CHECK(FileTypes::IsVideoExt(".avi"));
	BOOST_CHECK(FileTypes::IsVideoExt(".mov"));
	BOOST_CHECK(FileTypes::IsVideoExt(".m2ts"));
	BOOST_CHECK(FileTypes::IsVideoExt(".ts"));
	BOOST_CHECK(FileTypes::IsVideoExt(".m4v"));
	BOOST_CHECK(FileTypes::IsVideoExt(".webm"));
	BOOST_CHECK(FileTypes::IsVideoExt(".flv"));
	BOOST_CHECK(FileTypes::IsVideoExt(".wmv"));
	BOOST_CHECK(FileTypes::IsVideoExt(".divx"));
	BOOST_CHECK(FileTypes::IsVideoExt(".xvid"));

	BOOST_CHECK(!FileTypes::IsVideoExt(".rar"));
	BOOST_CHECK(!FileTypes::IsVideoExt(""));
}

BOOST_AUTO_TEST_CASE(IsAudioExtTest)
{
	BOOST_CHECK(FileTypes::IsAudioExt(".mp3"));
	BOOST_CHECK(FileTypes::IsAudioExt(".flac"));
	BOOST_CHECK(FileTypes::IsAudioExt(".aac"));
	BOOST_CHECK(FileTypes::IsAudioExt(".ogg"));
	BOOST_CHECK(FileTypes::IsAudioExt(".wav"));
	BOOST_CHECK(FileTypes::IsAudioExt(".dts"));
	BOOST_CHECK(FileTypes::IsAudioExt(".ac3"));
	BOOST_CHECK(FileTypes::IsAudioExt(".mka"));
	BOOST_CHECK(FileTypes::IsAudioExt(".opus"));
	BOOST_CHECK(FileTypes::IsAudioExt(".wma"));
	BOOST_CHECK(FileTypes::IsAudioExt(".eac3"));

	BOOST_CHECK(!FileTypes::IsAudioExt(".rar"));
	BOOST_CHECK(!FileTypes::IsAudioExt(""));
}

BOOST_AUTO_TEST_CASE(IsSubtitleExtTest)
{
	BOOST_CHECK(FileTypes::IsSubtitleExt(".srt"));
	BOOST_CHECK(FileTypes::IsSubtitleExt(".sub"));
	BOOST_CHECK(FileTypes::IsSubtitleExt(".idx"));
	BOOST_CHECK(FileTypes::IsSubtitleExt(".ass"));
	BOOST_CHECK(FileTypes::IsSubtitleExt(".ssa"));
	BOOST_CHECK(FileTypes::IsSubtitleExt(".smi"));
	BOOST_CHECK(FileTypes::IsSubtitleExt(".sup"));
	BOOST_CHECK(FileTypes::IsSubtitleExt(".pgs"));
	BOOST_CHECK(FileTypes::IsSubtitleExt(".vtt"));

	BOOST_CHECK(!FileTypes::IsSubtitleExt(".rar"));
	BOOST_CHECK(!FileTypes::IsSubtitleExt(""));
}

BOOST_AUTO_TEST_CASE(IsSampleStemTest)
{
	BOOST_CHECK(FileTypes::IsSampleStem("sample"));
	BOOST_CHECK(FileTypes::IsSampleStem("SAMPLE"));
	BOOST_CHECK(FileTypes::IsSampleStem("Sample"));
	BOOST_CHECK(FileTypes::IsSampleStem("file-sample"));
	BOOST_CHECK(FileTypes::IsSampleStem("file.sample"));
	BOOST_CHECK(FileTypes::IsSampleStem("file_sample"));
	BOOST_CHECK(FileTypes::IsSampleStem("not-a-sample"));
	BOOST_CHECK(FileTypes::IsSampleStem("Movie.Name.2020.1080p.BluRay.x264-Group-sample"));

	BOOST_CHECK(!FileTypes::IsSampleStem("samples"));
	BOOST_CHECK(!FileTypes::IsSampleStem("mysample"));
	BOOST_CHECK(!FileTypes::IsSampleStem("sam"));
	BOOST_CHECK(!FileTypes::IsSampleStem(""));
}

BOOST_AUTO_TEST_CASE(IsSevenZipFileTest)
{
	// Simple extensions
	BOOST_CHECK(FileTypes::IsSevenZipFile("archive.7z"));
	BOOST_CHECK(FileTypes::IsSevenZipFile("archive.zip"));
	BOOST_CHECK(FileTypes::IsSevenZipFile("archive.tar"));
	BOOST_CHECK(FileTypes::IsSevenZipFile("archive.tgz"));
	BOOST_CHECK(FileTypes::IsSevenZipFile("archive.txz"));

	// Compound: .tar.gz, .tar.bz2, .tar.xz
	BOOST_CHECK(FileTypes::IsSevenZipFile("archive.tar.gz"));
	BOOST_CHECK(FileTypes::IsSevenZipFile("archive.tar.bz2"));
	BOOST_CHECK(FileTypes::IsSevenZipFile("archive.tar.xz"));

	// Split 7z
	BOOST_CHECK(FileTypes::IsSevenZipFile("archive.7z.001"));

	// Case insensitive
	BOOST_CHECK(FileTypes::IsSevenZipFile("archive.ZIP"));
	BOOST_CHECK(FileTypes::IsSevenZipFile("archive.TAR.GZ"));

	// With path
	BOOST_CHECK(FileTypes::IsSevenZipFile("/tmp/archive.7z"));
	BOOST_CHECK(FileTypes::IsSevenZipFile("/home/user/my archive.zip"));

	// Not 7z
	BOOST_CHECK(!FileTypes::IsSevenZipFile("archive.rar"));
	BOOST_CHECK(!FileTypes::IsSevenZipFile("archive.mkv"));
	BOOST_CHECK(!FileTypes::IsSevenZipFile("archive.7z.002"));
	BOOST_CHECK(!FileTypes::IsSevenZipFile("noextension"));
}

BOOST_AUTO_TEST_CASE(IsRarFileTest)
{
	// Simple .rar
	BOOST_CHECK(FileTypes::IsRarFile("archive.rar"));
	BOOST_CHECK(FileTypes::IsRarFile("archive.RAR"));

	// RAR volumes
	BOOST_CHECK(FileTypes::IsRarFile("archive.r00"));
	BOOST_CHECK(FileTypes::IsRarFile("archive.z99"));
	BOOST_CHECK(FileTypes::IsRarFile("archive.R00"));

	// Split .rar
	BOOST_CHECK(FileTypes::IsRarFile("archive.part01.rar"));
	BOOST_CHECK(FileTypes::IsRarFile("archive.part1.rar"));
	BOOST_CHECK(FileTypes::IsRarFile("archive.part001.rar"));

	// With path
	BOOST_CHECK(FileTypes::IsRarFile("/tmp/archive.rar"));

	// Not RAR
	BOOST_CHECK(!FileTypes::IsRarFile("archive.7z"));
	BOOST_CHECK(!FileTypes::IsRarFile("archive.zip"));
	BOOST_CHECK(!FileTypes::IsRarFile("archive.mkv"));
	BOOST_CHECK(!FileTypes::IsRarFile("archive.001"));
	BOOST_CHECK(!FileTypes::IsRarFile("noextension"));
}

BOOST_AUTO_TEST_CASE(IsArchiveFileTest)
{
	BOOST_CHECK(FileTypes::IsArchiveFile("archive.rar"));
	BOOST_CHECK(FileTypes::IsArchiveFile("archive.7z"));
	BOOST_CHECK(FileTypes::IsArchiveFile("archive.zip"));
	BOOST_CHECK(FileTypes::IsArchiveFile("archive.r00"));
	BOOST_CHECK(FileTypes::IsArchiveFile("archive.7z.001"));

	BOOST_CHECK(!FileTypes::IsArchiveFile("file.mkv"));
	BOOST_CHECK(!FileTypes::IsArchiveFile("file.par2"));
	BOOST_CHECK(!FileTypes::IsArchiveFile("noextension"));
}

BOOST_AUTO_TEST_CASE(IsSampleFileTest)
{
	BOOST_CHECK(FileTypes::IsSampleFile("sample.mkv"));
	BOOST_CHECK(FileTypes::IsSampleFile("SAMPLE.avi"));
	BOOST_CHECK(FileTypes::IsSampleFile("file-sample.mkv"));
	BOOST_CHECK(FileTypes::IsSampleFile("file.sample.mkv"));
	BOOST_CHECK(FileTypes::IsSampleFile("file_sample.mkv"));
	// With path
	BOOST_CHECK(FileTypes::IsSampleFile("/tmp/sample.mkv"));

	BOOST_CHECK(!FileTypes::IsSampleFile("samples.mkv"));
	BOOST_CHECK(!FileTypes::IsSampleFile("mysample.mkv"));
	BOOST_CHECK(!FileTypes::IsSampleFile("file.mkv"));
}

BOOST_AUTO_TEST_SUITE_END()
