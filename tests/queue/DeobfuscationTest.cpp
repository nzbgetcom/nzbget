/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2023-2024 Denis <denis@nzbget.com>
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


#include <boost/test/unit_test.hpp>

#include "Deobfuscation.h"

using namespace Deobfuscation;

BOOST_AUTO_TEST_CASE(IsObfuscatedTest)
{
	BOOST_CHECK(IsStronglyObfuscated("2c0837e5fa42c8cfb5d5e583168a2af4.10"));
	BOOST_CHECK(IsStronglyObfuscated("5KzdcWdGVGUG83Q9jv8KXht4O2k57w.mkv"));
	BOOST_CHECK(IsStronglyObfuscated("2c0837e5fa42c8cfb5d5e583168a2af4.mkv"));
	BOOST_CHECK(IsStronglyObfuscated("a4c7d1f239b71a.a1c0a8b1790e65c9430d5a601037a4.7893"));
	BOOST_CHECK(IsStronglyObfuscated("a1b2c3d4e5f678.901234567890abcdef01234567890123.4567"));
	BOOST_CHECK(IsStronglyObfuscated("abc.xyz.a1b2c3d4e5f678.mkv"));
	BOOST_CHECK(IsStronglyObfuscated("b00bs.a1b2c3d4e5f678.mkv"));
	BOOST_CHECK(IsStronglyObfuscated("Not obfuscated.rar") == false);
	BOOST_CHECK(IsStronglyObfuscated("Not_obfuscated.rar") == false);
	BOOST_CHECK(IsStronglyObfuscated("Not.obfuscated.rar") == false);
}

BOOST_AUTO_TEST_CASE(DeobfuscationTest)
{
	BOOST_CHECK_EQUAL(Deobfuscate(""), "");
	BOOST_CHECK_EQUAL(Deobfuscate("\"A\""), "A");
	BOOST_CHECK_EQUAL(Deobfuscate("Not obfuscated"), "Not obfuscated");

	BOOST_CHECK_EQUAL(
		Deobfuscate("[PRiVATE]-[WtFnZb]-[setup_spellforce_3_-_reforced_161554.339115__54385_-1.bin]-[1/10] - \"\" yEnc  4288754174 (1/8377)"),
		"setup_spellforce_3_-_reforced_161554.339115__54385_-1.bin"
	);

	BOOST_CHECK_EQUAL(
		Deobfuscate("[PRiVATE]-[WtFnZb]-[1/series/American.Dad.S01E01.Pilot.1080p.DSNP.WEBRip.DDP.5.1.H.265.-EDGE2020.mkv]-[1/7] - \"\" yEnc  225628476 (1/315)"),
		"American.Dad.S01E01.Pilot.1080p.DSNP.WEBRip.DDP.5.1.H.265.-EDGE2020.mkv"
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
		Deobfuscate("[N3wZ] \\6aZWVk237607\\::[PRiVATE]-[WtFnZb]-[The.Diplomat.S01E02.Dont.Call.It.a.Kidnapping.1080p.NF.WEB-DL.DDP5.1.Atmos.H.264-playWEB.mkv]-[2/8] - \"\" yEnc 2241590477 (1/3128)"),
		"The.Diplomat.S01E02.Dont.Call.It.a.Kidnapping.1080p.NF.WEB-DL.DDP5.1.Atmos.H.264-playWEB.mkv"
	);

	BOOST_CHECK_EQUAL(
		Deobfuscate("\"2c0837e5fa42c8cfb5d5e583168a2af4.10\" yEnc (1/111)"),
		"2c0837e5fa42c8cfb5d5e583168a2af4.10"
	);

	BOOST_CHECK_EQUAL(
		Deobfuscate("[02/11] - \"Tron.Uprising.S01E18.Terminal.EAC3.2.0.1080p.WEBRip.x265-iVy.part1.rar\" yEnc(1/144)"),
		"Tron.Uprising.S01E18.Terminal.EAC3.2.0.1080p.WEBRip.x265-iVy.part1.rar"
	);

	BOOST_CHECK_EQUAL(
		Deobfuscate("Re: Artist Band's The Album-Thanks much - Band, John - Artist - The Album.mp3 (2/3)"),
		"Artist Band's The Album-Thanks much - Band, John - Artist - The Album.mp3"
	);

	BOOST_CHECK_EQUAL(Deobfuscate("Re: A (2/3)"), "A");
	BOOST_CHECK_EQUAL(Deobfuscate("Re: A"), "Re: A");
}
