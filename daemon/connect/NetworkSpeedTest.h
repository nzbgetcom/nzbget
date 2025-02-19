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

#ifndef NETWORK_SPEED_TEST_H
#define NETWORK_SPEED_TEST_H

#include <array>
#include <chrono>
#include <string_view>

#include "DataAnalytics.h"

namespace Network
{
	class SpeedTest final
	{
	public:
		SpeedTest() = default;
		SpeedTest(const SpeedTest&) = delete;
		SpeedTest operator=(const SpeedTest&) = delete;

		double RunTest() noexcept(false);

	private:
		struct TestSpec
		{
			const char* name;
			uint32_t bytes;
			int iterations;
			std::chrono::seconds timeout;
		};

		enum BytesToDownload : uint32_t
		{
			KiB = 1024,
			KiB100 = 1024 * 100,
			MiB1 = 1024 * 1024,
			MiB10 = 1024 * 1024 * 10,
			MiB25 = 1024 * 1024 * 25,
			MiB100 = 1024 * 1024 * 100,
			MiB250 = 1024 * 1024 * 250,
			GiB1 = 1024 * 1024 * 1024,
		};

		static constexpr std::array<TestSpec, 7> m_testSpecs
		{ {
			{ "100 KiB", KiB100, 10, std::chrono::seconds{1} },
			{ "1 MiB", MiB1, 8, std::chrono::seconds{2} },
			{ "10 MiB", MiB10, 6, std::chrono::seconds{2} },
			{ "25 MiB", MiB25, 4, std::chrono::seconds{2} },
			{ "100 MiB", MiB100, 3, std::chrono::seconds{3} },
			{ "250 MiB", MiB250, 2, std::chrono::seconds{4} },
			{ "1 GiB", GiB1, 2, std::chrono::seconds{5} },
		} };
		static constexpr size_t m_bufferSize = 8 * KiB;
		static constexpr std::string_view m_host = "speed.cloudflare.com";
		static constexpr std::string_view m_path = "__down";
		static constexpr std::string_view m_query = "?bytes=";
		static constexpr std::string_view m_errMsg = "No speed data was collected during the network speed test";

		bool RunTest(TestSpec testSpec);
		std::pair<uint32_t, std::chrono::duration<double>> ExecuteTest(std::string request, std::chrono::seconds timeout);
		std::string MakeRequest(uint32_t filesize) const;
		double CalculateSpeedInMbps(uint32_t bytes, double timeSec) const;
		bool IsRequiredMoreTests() const;
		bool HasExceededMaxFailures(int failures, int iterations) const;
		double GetFinalSpeed() const;

#ifdef DISABLE_TLS
		bool m_useTls = false;
		int m_port = 80;
#else
		bool m_useTls = true;
		int m_port = 443;
#endif
		std::vector<DataAnalytics> m_dataset;
	};
}

#endif
