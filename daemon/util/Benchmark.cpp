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

#include <exception>
#include <random>
#include "FileSystem.h"
#include "Benchmark.h"

namespace Benchmark
{
	using namespace std::chrono;

	std::pair<uint64_t, double> DiskBenchmark::Run(
		const std::string& dir,
		size_t bufferSizeBytes,
		uint64_t maxFileSizeBytes,
		seconds timeout) const noexcept(false)
	{
		ValidateBufferSize(bufferSizeBytes);

		const std::string filename = dir + PATH_SEPARATOR + GetUniqueFilename();
		std::ofstream file = OpenFile(filename);

		std::vector<char> buffer;
		UseBuffer(file, buffer, bufferSizeBytes);

		size_t dataSize = bufferSizeBytes > 0 ? bufferSizeBytes : 1024;
		std::vector<char> data = GenerateRandomCharsVec(dataSize);

		nanoseconds timeoutNS = duration_cast<nanoseconds>(timeout);

		return RunBench(file, filename, data, maxFileSizeBytes, timeoutNS);
	}

	std::pair<uint64_t, double> DiskBenchmark::RunBench(
		std::ofstream& file,
		const std::string& filename,
		const std::vector<char>& data,
		uint64_t maxFileSizeBytes,
		std::chrono::nanoseconds timeoutNS) const noexcept(false)
	{
		uint64_t totalWritten = 0;

		auto start = steady_clock::now();
		try
		{
			while (totalWritten < maxFileSizeBytes && (steady_clock::now() - start) < timeoutNS)
			{
				file.write(data.data(), data.size());
				totalWritten += data.size();
			}
		}
		catch (const std::exception& e)
		{
			CleanUp(file, filename);

			std::string errMsg = "Failed to write data to file " + filename + ". " + e.what();
			throw std::runtime_error(errMsg);
		}
		auto finish = steady_clock::now();
		double elapsed = duration<double, std::milli>(finish - start).count();

		CleanUp(file, filename);

		return { totalWritten, elapsed };
	}

	void DiskBenchmark::DeleteFile(const std::string& filename) const noexcept(false)
	{
		if (!FileSystem::DeleteFile(filename.c_str()))
		{
			std::string errMsg = "Failed to delete " + filename + " test file";
			throw std::runtime_error(errMsg);
		}
	}

	void DiskBenchmark::CleanUp(
		std::ofstream& file,
		const std::string& filename) const noexcept(false)
	{
		file.close();
		DeleteFile(filename);
	}

	std::string DiskBenchmark::GetUniqueFilename() const noexcept(false)
	{
		return std::to_string(
			high_resolution_clock::now().time_since_epoch().count()
		) + ".bin";
	}

	std::ofstream DiskBenchmark::OpenFile(const std::string& filename) const noexcept(false)
	{
		std::ofstream file(filename, std::ios::binary);

		if (!file.is_open())
		{
			std::string errMsg = std::string("Failed to create test file: ") + strerror(errno);
			throw std::runtime_error(errMsg);
		}

		file.exceptions(std::ofstream::badbit | std::ofstream::failbit);
		file.sync_with_stdio(false);

		return file;
	}

	void DiskBenchmark::ValidateBufferSize(size_t bufferSz) const noexcept(false)
	{
		if (bufferSz > m_maxBufferSize)
		{
			throw std::invalid_argument("The buffer size is too big");
		}
	}

	void DiskBenchmark::UseBuffer(
		std::ofstream& file,
		std::vector<char>& buffer,
		size_t buffSize
	) const noexcept(false)
	{
		if (buffSize > 0)
		{
			buffer.resize(buffSize);
			file.rdbuf()->pubsetbuf(buffer.data(), buffSize);
		}
		else
		{
			file.rdbuf()->pubsetbuf(nullptr, 0);
		}
	}

	std::vector<char> DiskBenchmark::GenerateRandomCharsVec(size_t size) const noexcept(false)
	{
		if (size == 0)
		{
			return {};
		}

		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> distrib(0, 127);

		std::vector<char> v(size);
		std::generate(begin(v), end(v), [&distrib, &gen]()
			{
				return static_cast<char>(distrib(gen));
			});

		return v;
	}
}
