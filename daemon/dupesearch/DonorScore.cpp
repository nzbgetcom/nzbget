/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Denis <denis@nzbget.com>
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
#include <cmath>
#include <numeric>
#include "DonorScore.h"

int DonorScore::Base(int pickScore)
{
	return std::max(0, pickScore - 1000);
}

int DonorScore::Target(double alive, bool twin)
{
	double share = alive < 0 ? 1.0 : std::min(1.0, alive);
	// nearbyint: halves go to even, as the proxy's round() did
	return std::min(Top, 9 + (int)std::nearbyint(80 * share) + (twin ? 1 : 0));
}

int DonorScore::Ranks::Take(double alive, bool twin)
{
	int score = Target(alive, twin);
	while (score > Dead + 1 && m_used.count(score))
	{
		score--;
	}
	m_used.insert(score);
	return score;
}

std::vector<int> DonorScore::Rerank(const std::vector<Entry>& entries, long long pickBytes)
{
	std::vector<int> scores;
	std::vector<size_t> live;
	for (size_t i = 0; i < entries.size(); i++)
	{
		scores.push_back(entries[i].score);
		if (entries[i].score > Dead)
		{
			live.push_back(i);
		}
	}

	auto percent = [&](const Entry& e) { return (int)std::nearbyint(100 * (e.alive < 0 ? 1.0 : e.alive)); };
	auto distance = [&](const Entry& e) { return e.bytes > pickBytes ? e.bytes - pickBytes : pickBytes - e.bytes; };
	std::stable_sort(live.begin(), live.end(), [&](size_t a, size_t b)
		{
			const Entry& x = entries[a];
			const Entry& y = entries[b];
			if (percent(x) != percent(y)) return percent(x) > percent(y);
			if (x.twin != y.twin) return x.twin;
			if (distance(x) != distance(y)) return distance(x) < distance(y);
			return x.grabs > y.grabs;
		});

	int previous = 100;
	for (size_t i : live)
	{
		int want = std::max(Dead + 1, std::min(Target(entries[i].alive, entries[i].twin), previous - 1));
		previous = want;
		scores[i] = want;
	}
	return scores;
}
