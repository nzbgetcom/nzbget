/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2023-2026 Denis <denis@nzbget.com>
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
#include "Deobfuscation.h"

BOOST_AUTO_TEST_SUITE(QueueTest)

using namespace Deobfuscation;

BOOST_AUTO_TEST_CASE(IsExcessivelyObfuscatedTest)
{
	BOOST_CHECK(IsExcessivelyObfuscated("2c0837e5fa42c8cfb5d5e583168a2af4.10"));
	BOOST_CHECK(IsExcessivelyObfuscated("5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"));
	BOOST_CHECK(IsExcessivelyObfuscated("2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));
	BOOST_CHECK(IsExcessivelyObfuscated("a4c7d1f239b71a.a1c0a8b1790e65c9430d5a601037a4.7893"));
	BOOST_CHECK(IsExcessivelyObfuscated("a1b2c3d4e5f678.901234567890abcdef01234567890123.4567"));
	BOOST_CHECK(IsExcessivelyObfuscated("abc.xyz.a1b2c3d4e5f678.mkv"));
	BOOST_CHECK(IsExcessivelyObfuscated("b00bs.a1b2c3d4e5f678.mkv"));
	BOOST_CHECK(IsExcessivelyObfuscated("ac4rcq47pkqt4flatz2xfac4rcq47pkqt4flatz2xf4567"));
	BOOST_CHECK(IsExcessivelyObfuscated("yfjEpzFbQommamTW"));
	BOOST_CHECK(IsExcessivelyObfuscated("MQHeRbSCIoPs"));
	BOOST_CHECK(IsExcessivelyObfuscated("n1iY94U6fTpMVY9GPD"));
	BOOST_CHECK(IsExcessivelyObfuscated("nzqymzflnjiyztgyntcynzzytq"));

	BOOST_CHECK(!IsExcessivelyObfuscated("Not.obfuscated.rar"));
	BOOST_CHECK(!IsExcessivelyObfuscated("Movie2"));
	BOOST_CHECK(!IsExcessivelyObfuscated("SomeMovie"));
	BOOST_CHECK(!IsExcessivelyObfuscated("Movie"));
	BOOST_CHECK(!IsExcessivelyObfuscated("Movie1998"));

	BOOST_CHECK(!IsExcessivelyObfuscated(
		"-Abc12Abc12Abc12Abc12Abc12Abc12Abc12Abc12Abc12Abc12Abc12Abc12!"
	));

	BOOST_CHECK(!IsExcessivelyObfuscated("a1b2c3d4e5f678.901234567890abcdef01234567890123.rar"));
	BOOST_CHECK(!IsExcessivelyObfuscated("a1b2c3d4e5f678.901234567890abcdef01234567890123.r00"));
	BOOST_CHECK(!IsExcessivelyObfuscated("2fpJZyw12WSJz8JunjkxpZcw0XIZKKMP.7z.15"));
	BOOST_CHECK(!IsExcessivelyObfuscated("2fpJZyw12WSJz8JunjkxpZcw0XIZKKMP.7z.015"));
	BOOST_CHECK(!IsExcessivelyObfuscated("a1b2c3d4e5f678.901234567890abcdef01234567890123.zip"));
	BOOST_CHECK(!IsExcessivelyObfuscated("a1b2c3d4e5f678.901234567890abcdef01234567890123.par2"));
	BOOST_CHECK(!IsExcessivelyObfuscated("ac4rcq47pkqt4flatz2xf.rar"));
	BOOST_CHECK(!IsExcessivelyObfuscated("2fpJZyw12WSJz8JunjkxpZcw0XIZKKMP.7z.01"));
	BOOST_CHECK(!IsExcessivelyObfuscated("abcdef0123456789abcdef.7z"));
	BOOST_CHECK(!IsExcessivelyObfuscated("abcdef0123456789abcdef.7z.15"));
	BOOST_CHECK(!IsExcessivelyObfuscated("abcdef0123456789abcdef.7z.015"));
	BOOST_CHECK(!IsExcessivelyObfuscated("abcdef0123456789abcdef.rar"));
	BOOST_CHECK(!IsExcessivelyObfuscated("abcdef0123456789abcdef.zip"));
	BOOST_CHECK(!IsExcessivelyObfuscated("abcdef0123456789abcdef.par2"));
	BOOST_CHECK(!IsExcessivelyObfuscated("abcdef0123456789abcdef.r00"));
	BOOST_CHECK(!IsExcessivelyObfuscated("abcdef0123456789abcdef.RAR"));
	BOOST_CHECK(!IsExcessivelyObfuscated("abcdef0123456789abcdef.PAR2"));
	BOOST_CHECK(!IsExcessivelyObfuscated("abcdef0123456789abcdef.s00"));
	BOOST_CHECK(!IsExcessivelyObfuscated("abcdef0123456789abcdef.z99"));
	BOOST_CHECK(!IsExcessivelyObfuscated("abcdef0123456789abcdef.gz"));
	BOOST_CHECK(!IsExcessivelyObfuscated("abcdef0123456789abcdef.tar"));
	BOOST_CHECK(!IsExcessivelyObfuscated("abcdef0123456789abcdef.sfv"));

	BOOST_CHECK(IsExcessivelyObfuscated("abcdef0123456789abcdef.r000"));
	BOOST_CHECK(IsExcessivelyObfuscated("abcdef0123456789abcdef.mkv.015"));
	BOOST_CHECK(IsExcessivelyObfuscated("abcdef0123456789abcdef.015"));

	BOOST_CHECK(IsExcessivelyObfuscated("a1b2c3d4e5f6789.rar"));
	BOOST_CHECK(IsExcessivelyObfuscated("a1b2c3d4e5f6789-abc.rar"));
	BOOST_CHECK(!IsExcessivelyObfuscated("a1b2c3d4e5f6789a.rar"));
	BOOST_CHECK(IsExcessivelyObfuscated("a1b2c3d4e5f6789a.mkv"));

	BOOST_CHECK(!IsExcessivelyObfuscated("ac4rcq47pkqt4flatz2xf.z00"));
	BOOST_CHECK(!IsExcessivelyObfuscated("ac4rcq47pkqt4flatz2xf.s99"));
	BOOST_CHECK(!IsExcessivelyObfuscated("ac4rcq47pkqt4flatz2xf.r50"));

	BOOST_CHECK(IsExcessivelyObfuscated("Backup_12345S67-89"));
	BOOST_CHECK(IsExcessivelyObfuscated("Backup_1234567890S01-02.mkv"));
	BOOST_CHECK(!IsExcessivelyObfuscated("Backup_1234S67-89"));
	BOOST_CHECK(!IsExcessivelyObfuscated("Backup_Series.S01E01"));

	BOOST_CHECK(IsExcessivelyObfuscated("123456_78"));
	BOOST_CHECK(IsExcessivelyObfuscated("987654_32.mkv"));
	BOOST_CHECK(!IsExcessivelyObfuscated("12345_78"));
	BOOST_CHECK(!IsExcessivelyObfuscated("2024_2025"));
	BOOST_CHECK(!IsExcessivelyObfuscated("01_02_03"));

	BOOST_CHECK(IsExcessivelyObfuscated("File-0123456789ab.cafe"));

	BOOST_CHECK(!IsExcessivelyObfuscated("2012"));
	BOOST_CHECK(!IsExcessivelyObfuscated("300"));
	BOOST_CHECK(!IsExcessivelyObfuscated("1917"));
	BOOST_CHECK(!IsExcessivelyObfuscated("9"));
	BOOST_CHECK(!IsExcessivelyObfuscated("21"));

	BOOST_CHECK(IsExcessivelyObfuscated("HFg3BYe1unWxVw.mkv"));
	BOOST_CHECK(IsExcessivelyObfuscated("l5PcmaKxDxcUaSM"));

	BOOST_CHECK(!IsExcessivelyObfuscated("CamelCaseTitle"));
	BOOST_CHECK(!IsExcessivelyObfuscated("TigerManTwo"));
	BOOST_CHECK(!IsExcessivelyObfuscated("CamelCase1999"));

	BOOST_CHECK(IsExcessivelyObfuscated("abcdefghij12"));

	BOOST_CHECK(IsExcessivelyObfuscated("HFg3BYe1unWxVw-release"));
	BOOST_CHECK(IsExcessivelyObfuscated("xY7z8K9mN2pQ3rS4t-group"));
	BOOST_CHECK(IsExcessivelyObfuscated("a1b2c3d4e5f6-group"));

	BOOST_CHECK(!IsExcessivelyObfuscated("Some-Movie"));
	BOOST_CHECK(!IsExcessivelyObfuscated("The-Movie"));

	BOOST_CHECK(!IsExcessivelyObfuscated("Strike4Force2"));
	BOOST_CHECK(!IsExcessivelyObfuscated("Strike4Force"));
	BOOST_CHECK(!IsExcessivelyObfuscated("Rise4Glory"));

	BOOST_CHECK(!IsExcessivelyObfuscated("Show.2024.01.02.1080p.x264-Group"));
	BOOST_CHECK(!IsExcessivelyObfuscated("Show.2024.S01E01.1080p.WEB-DL.x264-Group"));

	BOOST_CHECK(IsExcessivelyObfuscated("HFg3BYe.1unWxVw"));
	BOOST_CHECK(IsExcessivelyObfuscated("HFg3BYe-1unWxVw"));
	BOOST_CHECK(IsExcessivelyObfuscated("HFg3BYe_1unWxVw"));

	BOOST_CHECK(!IsExcessivelyObfuscated("NightHawk2022"));
	BOOST_CHECK(!IsExcessivelyObfuscated("Sentinel2024"));
	BOOST_CHECK(!IsExcessivelyObfuscated("Champion2000"));
	BOOST_CHECK(!IsExcessivelyObfuscated("Spartan2004"));

	BOOST_CHECK(!IsExcessivelyObfuscated("TigerOnTheRun"));
	BOOST_CHECK(!IsExcessivelyObfuscated("BattleInBerlin"));
	BOOST_CHECK(!IsExcessivelyObfuscated("ChaseToNowhere"));
	BOOST_CHECK(!IsExcessivelyObfuscated("RunOfTheMill"));
	BOOST_CHECK(!IsExcessivelyObfuscated("FallAtDawn"));
	BOOST_CHECK(!IsExcessivelyObfuscated("TigerOnTheRun2024"));
	BOOST_CHECK(!IsExcessivelyObfuscated("BattleInBerlin2023"));

	BOOST_CHECK(IsExcessivelyObfuscated("deadbeefcafe1234"));
	BOOST_CHECK(IsExcessivelyObfuscated("1234567890123456"));
	BOOST_CHECK(IsExcessivelyObfuscated("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
	BOOST_CHECK(IsExcessivelyObfuscated("abcdef0123456789"));

	BOOST_CHECK(!IsExcessivelyObfuscated("xyz"));
	BOOST_CHECK(!IsExcessivelyObfuscated("The.Show.Name.mkv"));
	BOOST_CHECK(!IsExcessivelyObfuscated("The_Last_Of_The_Words"));
	BOOST_CHECK(!IsExcessivelyObfuscated("StarTrekIntoDarkness"));

	BOOST_CHECK(!IsExcessivelyObfuscated("Movie2024Extended"));
	BOOST_CHECK(!IsExcessivelyObfuscated("Rise4Glory2023Fall"));
	BOOST_CHECK(IsExcessivelyObfuscated("Movie9999Extended"));

	BOOST_CHECK(!IsExcessivelyObfuscated("abcdef0123456789abcdef.part01.rar"));
	BOOST_CHECK(!IsExcessivelyObfuscated("abcdef0123456789abcdef.part123.par2"));
	BOOST_CHECK(!IsExcessivelyObfuscated("abcdef0123456789abcdef.vol00+01.par2"));
	BOOST_CHECK(!IsExcessivelyObfuscated("abcdef0123456789abcdef.vol10+20.par2"));

	BOOST_CHECK(!IsExcessivelyObfuscated("american.gangster.mkv"));
	BOOST_CHECK(!IsExcessivelyObfuscated("Fahrenheit.451.mkv"));

	BOOST_CHECK(!IsExcessivelyObfuscated("SpiderManNoWayHome.mkv"));
	BOOST_CHECK(!IsExcessivelyObfuscated("MissionImpossibleIII.mkv"));

	BOOST_CHECK(!IsExcessivelyObfuscated("INTERSTELLAR.mkv"));
	BOOST_CHECK(!IsExcessivelyObfuscated("History.of.telecommunications.mkv"));
	BOOST_CHECK(!IsExcessivelyObfuscated("Documentary.about.counterrevolutionary.theme.mkv"));
	BOOST_CHECK(!IsExcessivelyObfuscated("Doku.kraftfahrzeugversicherung.mkv"));
	BOOST_CHECK(!IsExcessivelyObfuscated("Doku.geschwindigkeitsbegrenzung.mkv"));
	BOOST_CHECK(!IsExcessivelyObfuscated("Release.2024.01.02.mkv"));
	BOOST_CHECK(IsExcessivelyObfuscated("Release.1234567890123456.mkv"));

	BOOST_CHECK(IsExcessivelyObfuscated("aBcDeF1234567890abcdef1234.S01E01.mkv"));
	BOOST_CHECK(IsExcessivelyObfuscated("abcdef1234567890abcdef1234567890.1080p.x264-GROUP"));
	BOOST_CHECK(IsExcessivelyObfuscated("ABCDEF1234567890ABCDEF1234567890.S01E01.mkv"));

	BOOST_CHECK(IsExcessivelyObfuscated("ABCDEFGHIJK001.mkv"));
	BOOST_CHECK(IsExcessivelyObfuscated("abcdefghijklm001.mkv"));
	BOOST_CHECK(IsExcessivelyObfuscated("ABCDEFGHIJ01.mkv"));
	BOOST_CHECK(IsExcessivelyObfuscated("abcdefghijkl01.mkv"));

	BOOST_CHECK(IsExcessivelyObfuscated("ABC"));
	BOOST_CHECK(IsExcessivelyObfuscated("AbC-XyZ"));
	BOOST_CHECK(IsExcessivelyObfuscated("ABC-XYZ"));
}

BOOST_AUTO_TEST_CASE(DeobfuscationTest)
{
	BOOST_CHECK_EQUAL(Deobfuscate(""), "");
	BOOST_CHECK_EQUAL(Deobfuscate("\"A\""), "A");
	BOOST_CHECK_EQUAL(Deobfuscate("Not obfuscated"), "Not obfuscated");
	BOOST_CHECK_EQUAL(Deobfuscate("\"filename.mkv yEnc (1/1)"), "filename.mkv yEnc (1/1)");
	BOOST_CHECK_EQUAL(Deobfuscate("filename.mkv yEnc\""), "");
	BOOST_CHECK_EQUAL(
		Deobfuscate("Any.Show.2024.S01E01.Die.verborgene.Hand.GERMAN.5.1.DL.EAC3.2160p.WEB-DL.DV.HDR.x265-TvR.vol127+128.par2 (1/0)"),
		"Any.Show.2024.S01E01.Die.verborgene.Hand.GERMAN.5.1.DL.EAC3.2160p.WEB-DL.DV.HDR.x265-TvR.vol127+128.par2"
	);

	BOOST_CHECK_EQUAL(
		Deobfuscate("[PRiVATE]-[WtFnZb]-[setup_app_-_reforced_161554.339115__54385_-1.bin]-[1/10] - \"\" yEnc  4288754174 (1/8377)"),
		"setup_app_-_reforced_161554.339115__54385_-1.bin"
	);

	BOOST_CHECK_EQUAL(
		Deobfuscate("[PRiVATE]-[WtFnZb]-[1/series/Any.Show.S01E01.Pilot.1080p.DSNP.WEBRip.DDP.5.1.H.265.-EDGE2020.mkv]-[1/7] - \"\" yEnc  225628476 (1/315)"),
		"Any.Show.S01E01.Pilot.1080p.DSNP.WEBRip.DDP.5.1.H.265.-EDGE2020.mkv"
	);

	BOOST_CHECK_EQUAL(
		Deobfuscate("[PRiVATE]-[WtFnZb]-[Movie_(1999)_DTS-HD_MA_5.1_-RELEASE_[TBoP].mkv]-[3/15] - \"\" yEnc 9876543210 (2/12345)"),
		"Movie_(1999)_DTS-HD_MA_5.1_-RELEASE_[TBoP].mkv"
	);

	BOOST_CHECK_EQUAL(
		Deobfuscate("[PRiVATE]-[WtFnZb]-[00101.mpls]-[163/591] - \"\" yEnc (2/12345)"),
		"00101.mpls"
	);

	BOOST_CHECK_EQUAL(
		Deobfuscate("[PRiVATE]-[WtFnZb]-[24]-[12/filename.ext] - \"\" yEnc (2/12345)"),
		"filename.ext"
	);

	BOOST_CHECK_EQUAL(
		Deobfuscate("[PRiVATE]-[WtFnZb]-[24]-[filename] - \"\" yEnc (2/12345)"),
		"filename"
	);

	BOOST_CHECK_EQUAL(
		Deobfuscate("[N3wZ] \\6aZWVk237607\\::[PRiVATE]-[WtFnZb]-[The.Show.S01E02.1080p.NF.WEB-DL.DDP5.1.Atmos.H.264-playWEB.mkv]-[2/8] - \"\" yEnc 2241590477 (1/3128)"),
		"The.Show.S01E02.1080p.NF.WEB-DL.DDP5.1.Atmos.H.264-playWEB.mkv"
	);

	BOOST_CHECK_EQUAL(
		Deobfuscate("[N3wZ] _mC3M3U14246___[PRiVATE]-[EnCrYpTnZb]-[[SubsPlease].Some.file.name.-.20.(1080p).[64032D13].mkv]-[1_1] - \"\" yEnc  1454715270 (1/2030)"),
		"[SubsPlease].Some.file.name.-.20.(1080p).[64032D13].mkv"
	);

	BOOST_CHECK_EQUAL(
		Deobfuscate("\"2c0837e5fa42c8cfb5d5e583168a2af4.10\" yEnc (1/111)"),
		"2c0837e5fa42c8cfb5d5e583168a2af4.10"
	);

	BOOST_CHECK_EQUAL(
		Deobfuscate("[02/11] - \"Some.Show.S01E18.Terminal.EAC3.2.0.1080p.WEBRip.x265-iVy.part1.rar\" yEnc(1/144)"),
		"Some.Show.S01E18.Terminal.EAC3.2.0.1080p.WEBRip.x265-iVy.part1.rar"
	);

	BOOST_CHECK_EQUAL(
		Deobfuscate("Re: Artist Band's The Album-Thanks much - Band, John - Artist - The Album.mp3 (2/3)"),
		"Artist Band's The Album-Thanks much - Band, John - Artist - The Album.mp3"
	);

	BOOST_CHECK_EQUAL(Deobfuscate("Re: A (2/3)"), "A");
	BOOST_CHECK_EQUAL(Deobfuscate("Re: A"), "A");
	BOOST_CHECK_EQUAL(Deobfuscate("[34/44] - id.bdmv yEnc (1/1) 104"), "id.bdmv");
}

BOOST_AUTO_TEST_SUITE_END()
