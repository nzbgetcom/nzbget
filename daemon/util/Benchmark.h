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

#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <chrono>
#include <string>
#include <fstream>

namespace Benchmark
{
	class DiskBenchmark final
	{
	public:
		std::pair<uint64_t, double> Run(
			const std::string& dir,
			size_t bufferSizeBytes,
			uint64_t maxFileSizeBytes,
			std::chrono::seconds timeout) const noexcept(false);

	private:
		std::pair<uint64_t, double> RunBench(
			std::ofstream& file,
			const std::string& filename,
			const std::vector<char>& data,
			uint64_t maxFileSizeBytes,
			std::chrono::nanoseconds timeoutNS) const noexcept(false);

		void DeleteFile(const std::string& filename) const noexcept(false);

		std::string GetUniqueFilename() const noexcept(false);

		void CleanUp(
			std::ofstream& file,
			const std::string& filename) const noexcept(false);

		std::ofstream OpenFile(const std::string& filename) const noexcept(false);

		void ValidateBufferSize(size_t bufferSz) const noexcept(false);

		void UseBuffer(
			std::ofstream& file,
			std::vector<char>& buffer,
			size_t buffSize
		) const noexcept(false);

		std::vector<char> GenerateRandomCharsVec(size_t size) const noexcept(false);

		const uint32_t m_maxBufferSize = 1024 * 1024 * 512;
	};
}

#endif
