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

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <fstream>

#include "Validators.h"

namespace fs = boost::filesystem;
using namespace SystemHealth;

BOOST_AUTO_TEST_SUITE(ValidatorsTests)

BOOST_AUTO_TEST_CASE(TestRequiredOption)
{
	auto ok = RequiredOption("Opt", "value");
	BOOST_CHECK(ok.IsOk());

	auto err = RequiredOption("Opt", "");
	BOOST_CHECK(err.IsError());
}

BOOST_AUTO_TEST_CASE(TestCheckPositiveNum)
{
	auto ok = CheckPositiveNum("Num", 5);
	BOOST_CHECK(ok.IsOk());

	auto err = CheckPositiveNum("Num", -1);
	BOOST_CHECK(err.IsError());
}

BOOST_AUTO_TEST_CASE(TestFileDirectoryChecks)
{
	fs::path tmp = fs::temp_directory_path() / fs::unique_path();
	fs::create_directory(tmp);
	fs::path file = tmp / "f.txt";
	std::ofstream(file.string()) << "x";

	auto fe = SystemHealth::File::Exists(file);
	BOOST_CHECK(fe.IsOk());

	auto fd = SystemHealth::Directory::Exists(tmp);
	BOOST_CHECK(fd.IsOk());

	auto fr = SystemHealth::File::Readable(file);
	BOOST_CHECK(fr.IsOk());

	auto fw = SystemHealth::File::Writable(file);
	BOOST_CHECK(fw.IsOk());

	fs::remove_all(tmp);
}

BOOST_AUTO_TEST_CASE(TestNetworkChecks)
{
	auto ok = SystemHealth::Network::ValidHostname("example.com");
	BOOST_CHECK(ok.IsOk());

	auto err = SystemHealth::Network::ValidHostname(std::string(300, 'a'));
	BOOST_CHECK(err.IsError());

	auto okPort = SystemHealth::Network::ValidPort(80);
	BOOST_CHECK(okPort.IsOk());
	auto badPort = SystemHealth::Network::ValidPort(0);
	BOOST_CHECK(badPort.IsError());
}

BOOST_AUTO_TEST_SUITE_END()
