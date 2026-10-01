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

#include <algorithm>
#include <fstream>
#include <random>
#include <stdexcept>
#include <vector>
#include "FileSystem.h"
#include "Benchmark.h"

namespace Benchmark
{
	using namespace std::chrono;

	namespace
	{
		void FillRandom(std::vector<char>& data)
		{
			std::mt19937_64 gen(std::random_device{}());
			for (char& c : data)
			{
				c = static_cast<char>(gen());
			}
		}

		// Deletes the test file on scope exit, also when the test fails.
		class TestFileRemover
		{
		public:
			explicit TestFileRemover(fs::path path) : m_path(std::move(path)) {}
			TestFileRemover(const TestFileRemover&) = delete;
			TestFileRemover& operator=(const TestFileRemover&) = delete;
			~TestFileRemover()
			{
				fs::error_code ec;
				fs::remove(m_path, ec);
			}

		private:
			fs::path m_path;
		};

		std::string GetUniqueFilename()
		{
			return "nzbget_disktest_" + std::to_string(
				high_resolution_clock::now().time_since_epoch().count()) + ".bin";
		}

		uint64_t ResolveMaxFileSize(const fs::path& dir, size_t blockSize, uint64_t requested)
		{
			fs::error_code ec;
			fs::space_info space = fs::space(dir, ec);
			if (ec)
			{
				throw std::runtime_error("Could not determine free disk space for " + fs::u8string(dir) + ": " + ec.message());
			}

			if (space.available < DiskBenchmark::FREE_SPACE_RESERVE + blockSize)
			{
				throw std::runtime_error("Not enough free disk space for the test");
			}

			return DiskBenchmark::LimitByFreeSpace(requested, space.available);
		}

		double ElapsedMs(steady_clock::time_point start)
		{
			return duration<double, std::milli>(steady_clock::now() - start).count();
		}
	}

	uint64_t DiskBenchmark::LimitByFreeSpace(uint64_t requested, uint64_t available)
	{
		if (available <= FREE_SPACE_RESERVE)
		{
			return 0;
		}
		return std::min(requested, available - FREE_SPACE_RESERVE);
	}

	Result DiskBenchmark::Run(
		const std::string& dir,
		size_t blockSizeBytes,
		uint64_t maxFileSizeBytes,
		seconds timeout) const
	{
		if (blockSizeBytes > MAX_BLOCK_SIZE)
		{
			throw std::invalid_argument("The buffer size is too big");
		}

		const size_t blockSize = blockSizeBytes == 0 ? DEFAULT_BLOCK_SIZE : blockSizeBytes;
		const fs::path dirPath = fs::u8path(dir);

		fs::error_code ec;
		if (!fs::is_directory(dirPath, ec))
		{
			throw std::runtime_error("Directory does not exist: " + dir);
		}

		const uint64_t maxBytes = ResolveMaxFileSize(dirPath, blockSize, maxFileSizeBytes);
		const nanoseconds timeoutNS = duration_cast<nanoseconds>(timeout);
		const fs::path filePath = dirPath / fs::u8path(GetUniqueFilename());

		std::vector<char> buffer(blockSize);
		FillRandom(buffer);

		Result result;
		const TestFileRemover remover(filePath);

		{
			std::ofstream file(filePath, std::ios::binary);
			if (!file)
			{
				throw std::runtime_error("Failed to create test file");
			}

			auto start = steady_clock::now();
			while (result.writeBytes < maxBytes && steady_clock::now() - start < timeoutNS)
			{
				const size_t bytesToWrite = static_cast<size_t>(std::min<uint64_t>(blockSize, maxBytes - result.writeBytes));
				if (!file.write(buffer.data(), bytesToWrite))
				{
					throw std::runtime_error("Failed to write data to the test file");
				}
				result.writeBytes += bytesToWrite;
			}
			file.close();
			if (file.fail())
			{
				throw std::runtime_error("Failed to write data to the test file");
			}
			result.writeMs = ElapsedMs(start);
		}

		{
			std::ifstream file(filePath, std::ios::binary);
			if (!file)
			{
				throw std::runtime_error("Failed to open test file for reading");
			}

			auto start = steady_clock::now();
			while (steady_clock::now() - start < timeoutNS)
			{
				file.read(buffer.data(), blockSize);
				const auto read = static_cast<uint64_t>(file.gcount());
				result.readBytes += read;
				if (!file)
				{
					break;
				}
			}
			result.readMs = ElapsedMs(start);
		}

		fs::remove(filePath, ec);
		if (ec)
		{
			throw std::runtime_error("Failed to delete test file " + fs::u8string(filePath) + ": " + ec.message());
		}

		return result;
	}
}
