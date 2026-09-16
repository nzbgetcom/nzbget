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
	BOOST_CHECK(!FileTypes::IsRarVolumeExt(".r000"));
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
	BOOST_CHECK(FileTypes::IsDiscStructureExt(".aob"));
	BOOST_CHECK(FileTypes::IsDiscStructureExt(".evo"));
	BOOST_CHECK(FileTypes::IsDiscStructureExt(".bdjo"));

	BOOST_CHECK(!FileTypes::IsDiscStructureExt(".rar"));
	BOOST_CHECK(!FileTypes::IsDiscStructureExt(".mkv"));
	BOOST_CHECK(!FileTypes::IsDiscStructureExt(""));
}

BOOST_AUTO_TEST_CASE(IsDiscStructureDirTest)
{
	BOOST_CHECK(FileTypes::IsDiscStructureDir("BDMV"));
	BOOST_CHECK(FileTypes::IsDiscStructureDir("bdmv"));
	BOOST_CHECK(FileTypes::IsDiscStructureDir("VIDEO_TS"));
	BOOST_CHECK(FileTypes::IsDiscStructureDir("video_ts"));
	BOOST_CHECK(FileTypes::IsDiscStructureDir("AUDIO_TS"));
	BOOST_CHECK(FileTypes::IsDiscStructureDir("audio_ts"));
	BOOST_CHECK(FileTypes::IsDiscStructureDir("HVDVD_TS"));
	BOOST_CHECK(FileTypes::IsDiscStructureDir("hvdvd_ts"));
	BOOST_CHECK(FileTypes::IsDiscStructureDir("AVCHD"));
	BOOST_CHECK(FileTypes::IsDiscStructureDir("avchd"));
	BOOST_CHECK(FileTypes::IsDiscStructureDir("CERTIFICATE"));
	BOOST_CHECK(FileTypes::IsDiscStructureDir("certificate"));

	// Path resiliency
	BOOST_CHECK(FileTypes::IsDiscStructureDir("/downloads/complete/BDMV"));
	BOOST_CHECK(FileTypes::IsDiscStructureDir("C:\\downloads\\VIDEO_TS"));

	BOOST_CHECK(!FileTypes::IsDiscStructureDir("Subs"));
	BOOST_CHECK(!FileTypes::IsDiscStructureDir("Sample"));
	BOOST_CHECK(!FileTypes::IsDiscStructureDir("Season 01"));
	BOOST_CHECK(!FileTypes::IsDiscStructureDir(""));
}

BOOST_AUTO_TEST_CASE(IsDiscDescriptorExtTest)
{
	BOOST_CHECK(FileTypes::IsDiscDescriptorExt(".cue"));
	BOOST_CHECK(FileTypes::IsDiscDescriptorExt(".CUE"));
	BOOST_CHECK(FileTypes::IsDiscDescriptorExt(".mds"));
	BOOST_CHECK(FileTypes::IsDiscDescriptorExt(".ccd"));
	BOOST_CHECK(FileTypes::IsDiscDescriptorExt(".toc"));

	BOOST_CHECK(!FileTypes::IsDiscDescriptorExt(".iso"));
	BOOST_CHECK(!FileTypes::IsDiscDescriptorExt(".bin"));
	BOOST_CHECK(!FileTypes::IsDiscDescriptorExt(""));
}

BOOST_AUTO_TEST_CASE(IsDiscImageExtTest)
{
	BOOST_CHECK(FileTypes::IsDiscImageExt(".iso"));
	BOOST_CHECK(FileTypes::IsDiscImageExt(".ISO"));
	BOOST_CHECK(FileTypes::IsDiscImageExt(".mdf"));
	BOOST_CHECK(FileTypes::IsDiscImageExt(".nrg"));
	BOOST_CHECK(FileTypes::IsDiscImageExt(".cdi"));
	BOOST_CHECK(FileTypes::IsDiscImageExt(".gdi"));

	// Generic container extensions requiring descriptor pairing
	BOOST_CHECK(!FileTypes::IsDiscImageExt(".bin"));
	BOOST_CHECK(!FileTypes::IsDiscImageExt(".img"));

	BOOST_CHECK(!FileTypes::IsDiscImageExt(".mkv"));
	BOOST_CHECK(!FileTypes::IsDiscImageExt(".cue"));
	BOOST_CHECK(!FileTypes::IsDiscImageExt(""));
}

BOOST_AUTO_TEST_CASE(IsGenericDiscImageExtTest)
{
	BOOST_CHECK(FileTypes::IsGenericDiscImageExt(".bin"));
	BOOST_CHECK(FileTypes::IsGenericDiscImageExt(".BIN"));
	BOOST_CHECK(FileTypes::IsGenericDiscImageExt(".img"));
	BOOST_CHECK(FileTypes::IsGenericDiscImageExt(".IMG"));

	BOOST_CHECK(!FileTypes::IsGenericDiscImageExt(".iso"));
	BOOST_CHECK(!FileTypes::IsGenericDiscImageExt(".mkv"));
	BOOST_CHECK(!FileTypes::IsGenericDiscImageExt(""));
}

BOOST_AUTO_TEST_CASE(IsClutterDirTest)
{
	BOOST_CHECK(FileTypes::IsClutterDir("@eaDir"));
	BOOST_CHECK(FileTypes::IsClutterDir("@eadir"));
	BOOST_CHECK(FileTypes::IsClutterDir(".AppleDouble"));
	BOOST_CHECK(FileTypes::IsClutterDir("__MACOSX"));
	BOOST_CHECK(FileTypes::IsClutterDir(".Spotlight-V100"));
	BOOST_CHECK(FileTypes::IsClutterDir(".Trashes"));

	// Path resiliency
	BOOST_CHECK(FileTypes::IsClutterDir("/nas/storage/@eaDir"));
	BOOST_CHECK(FileTypes::IsClutterDir("C:\\downloads\\__MACOSX"));

	BOOST_CHECK(!FileTypes::IsClutterDir("Movies"));
	BOOST_CHECK(!FileTypes::IsClutterDir("BDMV"));
	BOOST_CHECK(!FileTypes::IsClutterDir(""));
}

BOOST_AUTO_TEST_CASE(IsClutterFileTest)
{
	BOOST_CHECK(FileTypes::IsClutterFile(".DS_Store"));
	BOOST_CHECK(FileTypes::IsClutterFile(".ds_store"));
	BOOST_CHECK(FileTypes::IsClutterFile("Thumbs.db"));
	BOOST_CHECK(FileTypes::IsClutterFile("thumbs.db"));
	BOOST_CHECK(FileTypes::IsClutterFile("desktop.ini"));
	BOOST_CHECK(FileTypes::IsClutterFile("ehthumbs.db"));

	// AppleDouble resource fork sidecars
	BOOST_CHECK(FileTypes::IsClutterFile("._Movie.mkv"));
	BOOST_CHECK(FileTypes::IsClutterFile("._track01.flac"));
	BOOST_CHECK(FileTypes::IsClutterFile("/path/to/._Movie.mkv"));
	BOOST_CHECK(FileTypes::IsClutterFile("C:\\downloads\\._track01.flac"));

	// Path resiliency
	BOOST_CHECK(FileTypes::IsClutterFile("/path/to/.DS_Store"));
	BOOST_CHECK(FileTypes::IsClutterFile("C:\\downloads\\Thumbs.db"));

	BOOST_CHECK(!FileTypes::IsClutterFile("._"));
	BOOST_CHECK(!FileTypes::IsClutterFile(".Movie.mkv"));
	BOOST_CHECK(!FileTypes::IsClutterFile("movie.nfo"));
	BOOST_CHECK(!FileTypes::IsClutterFile("cover.jpg"));
	BOOST_CHECK(!FileTypes::IsClutterFile(""));
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
	BOOST_CHECK(FileTypes::IsAudioExt(".m4a"));
	BOOST_CHECK(FileTypes::IsAudioExt(".M4A"));

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

BOOST_AUTO_TEST_CASE(IsNfoExtTest)
{
	BOOST_CHECK(FileTypes::IsNfoExt(".nfo"));
	BOOST_CHECK(FileTypes::IsNfoExt(".NFO"));
	BOOST_CHECK(FileTypes::IsNfoExt(".info"));
	BOOST_CHECK(FileTypes::IsNfoExt(".INFO"));

	BOOST_CHECK(!FileTypes::IsNfoExt(".txt"));
	BOOST_CHECK(!FileTypes::IsNfoExt(".mkv"));
	BOOST_CHECK(!FileTypes::IsNfoExt(".rar"));
	BOOST_CHECK(!FileTypes::IsNfoExt(""));
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

BOOST_AUTO_TEST_CASE(IsBookExtTest)
{
	BOOST_CHECK(FileTypes::IsBookExt(".epub"));
	BOOST_CHECK(FileTypes::IsBookExt(".EPUB"));
	BOOST_CHECK(FileTypes::IsBookExt(".pdf"));
	BOOST_CHECK(FileTypes::IsBookExt(".PDF"));
	BOOST_CHECK(FileTypes::IsBookExt(".mobi"));
	BOOST_CHECK(FileTypes::IsBookExt(".azw3"));
	BOOST_CHECK(FileTypes::IsBookExt(".cbr"));
	BOOST_CHECK(FileTypes::IsBookExt(".cbz"));
	BOOST_CHECK(FileTypes::IsBookExt(".djvu"));
	BOOST_CHECK(FileTypes::IsBookExt(".m4b"));
	BOOST_CHECK(FileTypes::IsBookExt(".M4B"));

	BOOST_CHECK(!FileTypes::IsBookExt(".mkv"));
	BOOST_CHECK(!FileTypes::IsBookExt(".txt"));
	BOOST_CHECK(!FileTypes::IsBookExt(""));
}

BOOST_AUTO_TEST_CASE(IsImageExtTest)
{
	BOOST_CHECK(FileTypes::IsImageExt(".jpg"));
	BOOST_CHECK(FileTypes::IsImageExt(".JPG"));
	BOOST_CHECK(FileTypes::IsImageExt(".jpeg"));
	BOOST_CHECK(FileTypes::IsImageExt(".png"));
	BOOST_CHECK(FileTypes::IsImageExt(".gif"));
	BOOST_CHECK(FileTypes::IsImageExt(".webp"));
	BOOST_CHECK(FileTypes::IsImageExt(".bmp"));
	BOOST_CHECK(FileTypes::IsImageExt(".tif"));
	BOOST_CHECK(FileTypes::IsImageExt(".tiff"));

	BOOST_CHECK(!FileTypes::IsImageExt(".mkv"));
	BOOST_CHECK(!FileTypes::IsImageExt(".mp4"));
	BOOST_CHECK(!FileTypes::IsImageExt(""));
}

BOOST_AUTO_TEST_CASE(SniffExtensionTest)
{
	// Empty / short / unknown
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(std::span<const uint8_t>()), "");
	uint8_t shortBuf[] = { 0x1A, 0x45 };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(shortBuf), "");
	uint8_t unknownBuf[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(unknownBuf), "");

	// MKV (EBML)
	uint8_t mkvBuf[] = { 0x1A, 0x45, 0xDF, 0xA3, 0x01, 0x00, 0x00, 0x00, 'm', 'a', 't', 'r', 'o', 's', 'k', 'a' };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(mkvBuf), ".mkv");

	// WebM (EBML with webm docType)
	uint8_t webmBuf[] = { 0x1A, 0x45, 0xDF, 0xA3, 0x01, 0x00, 0x00, 0x00, 'w', 'e', 'b', 'm' };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(webmBuf), ".webm");

	// MP4 (ftyp isom)
	uint8_t mp4Buf[] = { 0x00, 0x00, 0x00, 0x20, 'f', 't', 'y', 'p', 'i', 's', 'o', 'm' };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(mp4Buf), ".mp4");

	// M4V (ftyp M4V )
	uint8_t m4vBuf[] = { 0x00, 0x00, 0x00, 0x20, 'f', 't', 'y', 'p', 'M', '4', 'V', ' ' };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(m4vBuf), ".m4v");

	// M4A (ftyp M4A )
	uint8_t m4aBuf[] = { 0x00, 0x00, 0x00, 0x20, 'f', 't', 'y', 'p', 'M', '4', 'A', ' ' };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(m4aBuf), ".m4a");

	// M4B (ftyp M4B )
	uint8_t m4bBuf[] = { 0x00, 0x00, 0x00, 0x20, 'f', 't', 'y', 'p', 'M', '4', 'B', ' ' };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(m4bBuf), ".m4b");

	// MOV (ftyp qt  )
	uint8_t movBuf[] = { 0x00, 0x00, 0x00, 0x20, 'f', 't', 'y', 'p', 'q', 't', ' ', ' ' };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(movBuf), ".mov");

	// AVI (RIFF....AVI )
	uint8_t aviBuf[] = { 'R', 'I', 'F', 'F', 0x00, 0x00, 0x00, 0x00, 'A', 'V', 'I', ' ' };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(aviBuf), ".avi");

	// WAV (RIFF....WAVE)
	uint8_t wavBuf[] = { 'R', 'I', 'F', 'F', 0x00, 0x00, 0x00, 0x00, 'W', 'A', 'V', 'E' };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(wavBuf), ".wav");

	// WEBP (RIFF....WEBP)
	uint8_t webpBuf[] = { 'R', 'I', 'F', 'F', 0x00, 0x00, 0x00, 0x00, 'W', 'E', 'B', 'P' };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(webpBuf), ".webp");

	// MPEG-TS (188 bytes interval)
	std::vector<uint8_t> tsBuf(200, 0x00);
	tsBuf[0] = 0x47;
	tsBuf[188] = 0x47;
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(tsBuf), ".ts");

	// MPEG-TS (3 packets verified in larger buffer)
	std::vector<uint8_t> tsBuf512(512, 0x00);
	tsBuf512[0] = 0x47;
	tsBuf512[188] = 0x47;
	tsBuf512[376] = 0x47;
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(tsBuf512), ".ts");

	// MPEG-TS: 3rd packet missing in 512-byte buffer -> must not match .ts
	std::vector<uint8_t> falseTsBuf(512, 0x00);
	falseTsBuf[0] = 0x47;
	falseTsBuf[188] = 0x47;
	falseTsBuf[376] = 0x00;
	BOOST_CHECK_NE(FileTypes::SniffExtension(falseTsBuf), ".ts");

	// WMV
	uint8_t wmvBuf[] = { 0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0x00, 0x00 };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(wmvBuf), ".wmv");

	// FLAC
	uint8_t flacBuf[] = { 'f', 'L', 'a', 'C', 0x00, 0x00 };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(flacBuf), ".flac");

	// MP3 (ID3)
	uint8_t mp3Id3Buf[] = { 'I', 'D', '3', 0x03, 0x00, 0x00 };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(mp3Id3Buf), ".mp3");

	// MP3 (MPEG audio frame sync 0xFF 0xFB)
	uint8_t mp3SyncBuf[] = { 0xFF, 0xFB, 0x90, 0x64 };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(mp3SyncBuf), ".mp3");

	// OGG
	uint8_t oggBuf[] = { 'O', 'g', 'g', 'S', 0x00, 0x02 };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(oggBuf), ".ogg");

	// PDF
	uint8_t pdfBuf[] = { '%', 'P', 'D', 'F', '-', '1', '.', '5' };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(pdfBuf), ".pdf");

	// EPUB (ZIP with mimetype)
	std::vector<uint8_t> epubBuf(80, 0x00);
	epubBuf[0] = 0x50; epubBuf[1] = 0x4B; epubBuf[2] = 0x03; epubBuf[3] = 0x04;
	const char* epubMime = "mimetypeapplication/epub+zip";
	std::memcpy(epubBuf.data() + 30, epubMime, std::strlen(epubMime));
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(epubBuf), ".epub");

	// Generic ZIP
	uint8_t zipBuf[] = { 0x50, 0x4B, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00 };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(zipBuf), ".zip");

	// MOBI
	std::vector<uint8_t> mobiBuf(80, 0x00);
	std::memcpy(mobiBuf.data() + 60, "BOOKMOBI", 8);
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(mobiBuf), ".mobi");

	// JPEG
	uint8_t jpgBuf[] = { 0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F' };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(jpgBuf), ".jpg");

	// PNG
	uint8_t pngBuf[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00 };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(pngBuf), ".png");

	// GIF
	uint8_t gifBuf[] = { 'G', 'I', 'F', '8', '9', 'a', 0x01, 0x00 };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(gifBuf), ".gif");

	// GIF with 0x47 at byte 188 (must NOT be misclassified as MPEG-TS .ts!)
	std::vector<uint8_t> gifCollisionBuf(256, 0x00);
	std::memcpy(gifCollisionBuf.data(), "GIF89a", 6);
	gifCollisionBuf[188] = 0x47;
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(gifCollisionBuf), ".gif");

	// BMP
	std::vector<uint8_t> bmpBuf(20, 0x00);
	bmpBuf[0] = 'B'; bmpBuf[1] = 'M';
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(bmpBuf), ".bmp");

	// Fake BMP: starts with "BM" but has non-zero reserved bytes -> must NOT match .bmp
	std::vector<uint8_t> fakeBmpBuf(20, 0x00);
	fakeBmpBuf[0] = 'B'; fakeBmpBuf[1] = 'M';
	fakeBmpBuf[6] = 0x01;
	BOOST_CHECK_NE(FileTypes::SniffExtension(fakeBmpBuf), ".bmp");

	// RAR
	uint8_t rarBuf[] = { 'R', 'a', 'r', '!', 0x1A, 0x07, 0x00 };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(rarBuf), ".rar");

	// 7z
	uint8_t szBuf[] = { '7', 'z', 0xBC, 0xAF, 0x27, 0x1C };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(szBuf), ".7z");

	// Gzip
	uint8_t gzBuf[] = { 0x1F, 0x8B, 0x08, 0x00 };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(gzBuf), ".gz");

	// Bzip2
	uint8_t bz2Buf[] = { 'B', 'Z', 'h', '9' };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(bz2Buf), ".bz2");

	// XZ
	uint8_t xzBuf[] = { 0xFD, '7', 'z', 'X', 'Z', 0x00 };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(xzBuf), ".xz");

	// POSIX tar
	std::vector<uint8_t> tarBuf(300, 0x00);
	std::memcpy(tarBuf.data() + 257, "ustar", 5);
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(tarBuf), ".tar");

	// Unix compress
	uint8_t zBuf[] = { 0x1F, 0x9D };
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(zBuf), ".Z");

	// ASF with video stream
	std::vector<uint8_t> asfVideoBuf(256, 0x00);
	// ASF Header Object GUID (first 8 bytes)
	uint8_t asfGuid[] = { 0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11 };
	std::memcpy(asfVideoBuf.data(), asfGuid, 8);
	// ASF_Video_Media GUID somewhere in the buffer
	uint8_t videoGuid[] = { 0xC0, 0xEF, 0x19, 0xBC, 0x4D, 0x5B, 0xCF, 0x11, 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B };
	std::memcpy(asfVideoBuf.data() + 80, videoGuid, 16);
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(asfVideoBuf), ".wmv");

	// ASF with audio stream only
	std::vector<uint8_t> asfAudioBuf(256, 0x00);
	std::memcpy(asfAudioBuf.data(), asfGuid, 8);
	// ASF_Audio_Media GUID somewhere in the buffer
	uint8_t audioGuid[] = { 0x40, 0x9E, 0x69, 0xF8, 0x4D, 0x5B, 0xCF, 0x11, 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B };
	std::memcpy(asfAudioBuf.data() + 80, audioGuid, 16);
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(asfAudioBuf), ".wma");

	// ASF with no stream type found (fallback to .wmv)
	std::vector<uint8_t> asfUnknownBuf(16, 0x00);
	std::memcpy(asfUnknownBuf.data(), asfGuid, 8);
	BOOST_CHECK_EQUAL(FileTypes::SniffExtension(asfUnknownBuf), ".wmv");
}

BOOST_AUTO_TEST_SUITE_END()
