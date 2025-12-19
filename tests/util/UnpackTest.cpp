/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2025 Denis <denis@nzbget.com>
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
#include <boost/filesystem.hpp>

#include "Unpack.h"

namespace fs = boost::filesystem;
using namespace Unpack;

BOOST_AUTO_TEST_CASE(IsSupportedTests)
{
	BOOST_CHECK(SevenZip::IsSupported("Unpack.7z"));
	BOOST_CHECK(SevenZip::IsSupported("Unpack.zip"));
	BOOST_CHECK(SevenZip::IsSupported("Unpack.tar"));
	BOOST_CHECK(SevenZip::IsSupported("Unpack.gz"));
	BOOST_CHECK(SevenZip::IsSupported("Unpack.bz2"));

	BOOST_CHECK(SevenZip::IsSupported("Unpack.tar.gz"));
	BOOST_CHECK(SevenZip::IsSupported("Unpack.tgz"));
	BOOST_CHECK(SevenZip::IsSupported("Unpack.txz"));

	BOOST_CHECK(SevenZip::IsSupported("Unpack.7z.001"));

	BOOST_CHECK(SevenZip::IsSupported("Unpack.ZIP"));
	BOOST_CHECK(SevenZip::IsSupported("DATA.TXZ"));

	BOOST_CHECK(!SevenZip::IsSupported("Unpack.7z.002"));
	BOOST_CHECK(!SevenZip::IsSupported("document.docx"));
	BOOST_CHECK(!SevenZip::IsSupported("image.jpg"));
	BOOST_CHECK(!SevenZip::IsSupported("Unpack.zip.bak"));

	BOOST_CHECK(Unrar::IsSupported("Unpack.rar"));
	BOOST_CHECK(Unrar::IsSupported("Unpack.part1.rar"));
	BOOST_CHECK(Unrar::IsSupported("Unpack.part01.rar"));
	BOOST_CHECK(Unrar::IsSupported("Unpack.part001.rar"));

	BOOST_CHECK(Unrar::IsSupported("Unpack.RAR"));
	BOOST_CHECK(Unrar::IsSupported("DATA.Part1.Rar"));

	BOOST_CHECK(!Unrar::IsSupported("Unpack.r01"));
	BOOST_CHECK(!Unrar::IsSupported("Unpack.part2.rar"));
	BOOST_CHECK(!Unrar::IsSupported("Unpack.zip"));
	BOOST_CHECK(!Unrar::IsSupported("not_an_Unpack.txt"));
	BOOST_CHECK(!Unrar::IsSupported("Unpack.tar.gz"));
	BOOST_CHECK(!Unrar::IsSupported("Unpack.rar.bak"));

	BOOST_CHECK(!Unrar::IsSupported("Архив.r01"));
	BOOST_CHECK(!Unrar::IsSupported("Архив.part2.rar"));
	BOOST_CHECK(!Unrar::IsSupported("Архив.zip"));

	BOOST_CHECK(!SevenZip::IsSupported("noextension"));

	BOOST_CHECK(!SevenZip::IsSupported("/some/path/"));

	BOOST_CHECK(SevenZip::IsSupported("archive.nzb.zip"));
	BOOST_CHECK(SevenZip::IsSupported("archive.tar.gz"));
	BOOST_CHECK(!SevenZip::IsSupported("archive.tar.gz.bak"));

	BOOST_CHECK(!Unrar::IsSupported("archive.part02.rar"));
	BOOST_CHECK(Unrar::IsSupported("/path/with/spaces/Unpack.part001.RAR"));

	BOOST_CHECK(SevenZip::IsSupported(fs::path("/tmp/archive.TGZ")));
	BOOST_CHECK(Unrar::IsSupported(fs::path("/tmp/Unpack.part1.rar")));

	BOOST_CHECK(Unrar::IsSupported("Архив.part001.rar"));

	BOOST_CHECK(SevenZip::IsSupported("archive.tar.bz2"));
	BOOST_CHECK(!SevenZip::IsSupported("archive.tar.bz2.backup"));
}

BOOST_AUTO_TEST_CASE(MakeExtractorUnsupportedTest)
{
	BOOST_CHECK_THROW(MakeExtractor("file.txt", "/tmp/out", "", OverwriteMode::Skip), std::runtime_error);
}
