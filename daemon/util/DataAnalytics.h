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

#include <vector>

class DataAnalytics final
{
public:
	DataAnalytics() = default;
	~DataAnalytics() = default;

	double GetMin() const;
	double GetMax() const;
	double GetAvg() const;
	double GetMedian() const;
	double GetPercentile25() const;
	double GetPercentile75() const;
	double GetPercentile90() const;

	void Compute();
	void AddMeasurement(double measuremen);

private:
	double CalculatePercentile(double percentile);

	double m_min = 0.0;
	double m_max = 0.0;
	double m_avg = 0.0;
	double m_median = 0.0;
	double m_percentile25 = 0.0;
	double m_percentile75 = 0.0;
	double m_percentile90 = 0.0;
	std::vector<double> m_measurements;
};
