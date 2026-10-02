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

#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <chrono>
#include <cstdint>
#include <string>

namespace Benchmark
{
	struct Result
	{
		uint64_t writeBytes = 0;
		double writeMs = 0.0;
		uint64_t readBytes = 0;
		double readMs = 0.0;
	};

	// Sequential write followed by sequential read through the regular
	// buffered file API, i.e. the way the rest of the program accesses files.
	class DiskBenchmark final
	{
	public:
		static constexpr size_t DEFAULT_BLOCK_SIZE = size_t{1024} * 1024;
		static constexpr size_t MAX_BLOCK_SIZE = size_t{512} * 1024 * 1024;
		static constexpr uint64_t FREE_SPACE_RESERVE = 100ull * 1024 * 1024;

		// Returns the test file size that keeps FreeSpaceReserve untouched.
		static uint64_t LimitByFreeSpace(uint64_t requested, uint64_t available);

		// blockSizeBytes == 0 selects DefaultBlockSize.
		// The timeout applies to each phase (write, read) separately.
		Result Run(
			const std::string& dir,
			size_t blockSizeBytes,
			uint64_t maxFileSizeBytes,
			std::chrono::seconds timeout) const;
	};
}

#endif
