/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2024-2026 Denis <denis@nzbget.com>
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
#include "Benchmark.h"
#include "FileSystem.h"

namespace
{
struct BenchTempDir
{
	explicit BenchTempDir(const std::string& name)
	{
		path = fs::temp_directory_path() /
			fs::u8path(name + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		fs::create_directories(path);
	}

	~BenchTempDir()
	{
		fs::error_code ec;
		fs::remove_all(path, ec);
	}

	std::string Str() const { return fs::u8string(path); }

	size_t FileCount() const
	{
		return std::distance(fs::directory_iterator(path), fs::directory_iterator());
	}

	fs::path path;
};

constexpr uint64_t MiB = 1024ull * 1024ull;
constexpr uint64_t GiB = 1024ull * MiB;
}

BOOST_AUTO_TEST_SUITE(UtilTest)

BOOST_AUTO_TEST_CASE(BenchmarkRejectsTooBigBufferTest)
{
	Benchmark::DiskBenchmark db;
	size_t tooBigBuffer = 1024ul * 1024ul * 1024ul;

	BOOST_CHECK_THROW(
		db.Run("./", tooBigBuffer, 1024, std::chrono::seconds(1)), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(BenchmarkFailsOnInvalidPathTest)
{
	Benchmark::DiskBenchmark db;

	BOOST_CHECK_THROW(
		db.Run("InvalidPath", 1024, 1024, std::chrono::seconds(1)), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(BenchmarkReportsMissingDirectoryTest)
{
	BenchTempDir dir("nzbget_bench_");
	const std::string missing = dir.Str() + "/missing";
	Benchmark::DiskBenchmark db;

	try
	{
		db.Run(missing, 1024, 1024, std::chrono::seconds(1));
		BOOST_FAIL("exception expected");
	}
	catch (const std::runtime_error& e)
	{
		BOOST_CHECK(std::string(e.what()).find("Directory does not exist") != std::string::npos);
	}

	BOOST_CHECK(!fs::exists(fs::u8path(missing)));
}

BOOST_AUTO_TEST_CASE(BenchmarkMeasuresWriteAndReadTest)
{
	BenchTempDir dir("nzbget_bench_");
	Benchmark::DiskBenchmark db;

	auto res = db.Run(dir.Str(), 64 * 1024, 4 * MiB, std::chrono::seconds(5));

	BOOST_CHECK_EQUAL(res.writeBytes, 4 * MiB);
	BOOST_CHECK_EQUAL(res.readBytes, res.writeBytes);
	BOOST_CHECK(res.writeMs > 0.0);
	BOOST_CHECK(res.readMs > 0.0);
	BOOST_CHECK_EQUAL(dir.FileCount(), 0u);
}

BOOST_AUTO_TEST_CASE(BenchmarkUtf8DirectoryTest)
{
	BenchTempDir dir("nzbget_l\xC3\xA4_\xE4\xBD\xA0\xE5\xA5\xBD_");
	Benchmark::DiskBenchmark db;

	auto res = db.Run(dir.Str(), 0, MiB, std::chrono::seconds(5));

	BOOST_CHECK(res.writeBytes >= MiB);
	BOOST_CHECK_EQUAL(res.readBytes, res.writeBytes);
	BOOST_CHECK_EQUAL(dir.FileCount(), 0u);
}

BOOST_AUTO_TEST_CASE(BenchmarkStopsOnTimeoutTest)
{
	BenchTempDir dir("nzbget_bench_");
	Benchmark::DiskBenchmark db;

	auto res = db.Run(dir.Str(), 4096, 1024 * GiB, std::chrono::seconds(1));

	BOOST_CHECK(res.writeBytes > 0);
	BOOST_CHECK(res.writeBytes < 1024 * GiB);
	BOOST_CHECK(res.readBytes > 0);
	BOOST_CHECK(res.readBytes <= res.writeBytes);
	BOOST_CHECK_EQUAL(dir.FileCount(), 0u);
}

BOOST_AUTO_TEST_CASE(BenchmarkZeroBlockSizeUsesDefaultTest)
{
	BenchTempDir dir("nzbget_bench_");
	Benchmark::DiskBenchmark db;

	auto res = db.Run(dir.Str(), 0, 1, std::chrono::seconds(5));

	BOOST_CHECK_EQUAL(res.writeBytes, Benchmark::DiskBenchmark::DEFAULT_BLOCK_SIZE);
	BOOST_CHECK_EQUAL(res.readBytes, res.writeBytes);
}

BOOST_AUTO_TEST_CASE(BenchmarkOddBlockSizeTest)
{
	BenchTempDir dir("nzbget_bench_");
	Benchmark::DiskBenchmark db;

	auto res = db.Run(dir.Str(), 1000, 10000, std::chrono::seconds(5));

	BOOST_CHECK_EQUAL(res.writeBytes, 10000u);
	BOOST_CHECK_EQUAL(res.readBytes, 10000u);
}

BOOST_AUTO_TEST_CASE(BenchmarkLimitByFreeSpaceTest)
{
	using Benchmark::DiskBenchmark;
	const uint64_t reserve = DiskBenchmark::FREE_SPACE_RESERVE;

	BOOST_CHECK_EQUAL(DiskBenchmark::LimitByFreeSpace(10 * GiB, 0), 0u);
	BOOST_CHECK_EQUAL(DiskBenchmark::LimitByFreeSpace(10 * GiB, reserve), 0u);
	BOOST_CHECK_EQUAL(DiskBenchmark::LimitByFreeSpace(10 * GiB, reserve + GiB), GiB);
	BOOST_CHECK_EQUAL(DiskBenchmark::LimitByFreeSpace(MiB, reserve + GiB), MiB);
}

BOOST_AUTO_TEST_SUITE_END()
