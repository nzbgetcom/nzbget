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

#include <sstream>
#include "Util.h"
#include "NetworkSpeedTest.h"
#include "Connection.h"
#include "Log.h"

using namespace std::chrono;

namespace Network
{
	double SpeedTest::RunTest() noexcept(false)
	{
		info("Download speed test starting...");

		for (TestSpec test : m_testSpecs)
		{
			if (!RunTest(test)) break;
			if (!IsRequiredMoreTests()) break;
		}

		if (m_dataset.empty())
		{
			error("%s", m_errMsg.data());
			throw std::runtime_error(m_errMsg.data());
		}

		double finalSpeed = GetFinalSpeed();

		info("Download speed %.2f Mbps", finalSpeed);

		return finalSpeed;
	}

	bool SpeedTest::RunTest(TestSpec testSpec)
	{
		DataAnalytics data;
		auto [name, size, iterations, timeout] = testSpec;
		int failures = 0;

		while (--iterations)
		{
			auto [downloaded, elapsedTime] = ExecuteTest(MakeRequest(size), timeout);
			if (elapsedTime == seconds{ 0 } || downloaded < size)
			{
				++failures;
				if (HasExceededMaxFailures(failures, iterations))
				{
					break;
				}

				continue;
			}

			double speed = CalculateSpeedInMbps(downloaded, elapsedTime.count());
			data.AddMeasurement(speed);

			info("Downloaded %s at %.2f Mbps", name, speed);
		}

		data.Compute();
		m_dataset.push_back(std::move(data));

		return true;
	}

	std::pair<uint32_t, duration<double>> SpeedTest::ExecuteTest(std::string request, seconds timeout)
	{
		Connection connection(m_host.data(), m_port, m_useTls);
		connection.SetTimeout(1);

		if (!connection.Connect())
		{
			return { 0, seconds{0} };
		}

		if (!connection.Send(request.c_str(), request.size()))
		{
			return { 0, seconds{0} };
		}

		uint32_t totalRecieved = 0;
		char buffer[m_bufferSize];
		auto start = steady_clock::now();
		while (int recieved = connection.TryRecv(buffer, m_bufferSize))
		{
			if (recieved <= 0 || (steady_clock::now() - start) >= timeout)
			{
				break;
			}

			totalRecieved += recieved;
		}
		auto finish = steady_clock::now();

		return { totalRecieved, duration<double>(finish - start) };
	}

	double SpeedTest::CalculateSpeedInMbps(uint32_t bytes, double timeSec) const
	{
		double bits = bytes * 8.0;
		double speed = bits / timeSec;
		double mbps = speed / 1000000.0;
		return mbps;
	}

	bool SpeedTest::IsRequiredMoreTests() const
	{
		if (m_dataset.size() < 2)
			return true;

		const DataAnalytics& curr = m_dataset.back();
		const DataAnalytics& prev = m_dataset[m_dataset.size() - 2];

		if (curr.GetMedian() > prev.GetPercentile25())
		{
			return true;
		}

		return false;
	}

	bool SpeedTest::HasExceededMaxFailures(int failures, int iterations) const
	{
		return failures > std::max(iterations / 2, 2);
	}

	double SpeedTest::GetFinalSpeed() const
	{
		const auto& dataAnalytics = m_dataset.front();

		double maxSpeed = dataAnalytics.GetMedian();
		for (const auto& analytics : m_dataset)
		{
			double speed = analytics.GetPercentile90();
			if (maxSpeed < speed)
			{
				maxSpeed = speed;
			}
		}

		return maxSpeed;
	}

	std::string SpeedTest::MakeRequest(uint32_t filesize) const
	{
		std::ostringstream request;

		request << "GET /" << m_path;
		request << m_query << std::to_string(filesize);
		request << " HTTP/1.1\r\n";
		request << "Host: " << m_host << "\r\n";
		request << "Connection: close\r\n\r\n";

		return request.str();
	}
}
