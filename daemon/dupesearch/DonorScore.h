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


#ifndef DONORSCORE_H
#define DONORSCORE_H

#include <set>
#include <vector>

/*
 * The DupeScore of a duplicate the search queues. nzbget tries duplicates by
 * the highest score first, so the most whole posting must score highest:
 * below the pick (which keeps its score), above the dead ones (base + 1).
 * Scores are offsets from a base (the pick's score minus 1000), so they
 * stay below the pick however high a client scored it.
 */
class DonorScore
{
public:
	// the score of a posting known dead
	static constexpr int Dead = 1;
	static constexpr int Top = 90;

	/* the base of the donor scores under a pick with this score */
	static int Base(int pickScore);

	/* the offset for a posting alive by this share (negative: unknown, taken
	 * as whole): 9 + 80 * share, one more for a byte-identical twin of the
	 * pick (it wins a tie: borrowing articles and recreating whole files
	 * work best with it), at most Top */
	static int Target(double alive, bool twin);

	/* unique offsets: a taken one counts down (90, 89, ...) */
	class Ranks
	{
	public:
		int Take(double alive, bool twin);
		void Release(int score) { m_used.erase(score); }
		void Use(int score) { m_used.insert(score); }
	private:
		std::set<int> m_used;
	};

	struct Entry
	{
		int score = 0;
		double alive = -1;
		bool twin = false;
		long long bytes = 0;
		int grabs = 0;
	};

	/* the offsets for the live entries once every check is in: most alive
	 * first (whole percent), then twin, closest to pickBytes, most grabbed;
	 * strictly falling, each at most its Target. Entries scored Dead keep
	 * their score. Parallel to the entries. */
	static std::vector<int> Rerank(const std::vector<Entry>& entries, long long pickBytes);
};

#endif
