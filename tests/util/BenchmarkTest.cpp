/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2024 Denis <denis@nzbget.com>
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

#include <exception>
#include "Benchmark.h"

BOOST_AUTO_TEST_CASE(BenchmarkTest)
{
	Benchmark::DiskBenchmark db;
	{
		size_t tooBigBuffer = 1024ul * 1024ul * 1024ul;
		BOOST_CHECK_THROW(
			db.Run("./", tooBigBuffer, 1024, std::chrono::seconds(1)), std::invalid_argument
		);
	}

	{
		BOOST_CHECK_THROW(
			db.Run("InvalidPath", 1024, 1024, std::chrono::seconds(1)), std::runtime_error
		);
	}

	{
		uint64_t  maxFileSize = 1024 * 1024;
		auto [size, duration] = db.Run("./", 1024, maxFileSize, std::chrono::seconds(1));
		BOOST_CHECK(size >= maxFileSize || duration < 1.3);
	}
}
