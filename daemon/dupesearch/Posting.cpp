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
#include <map>
#include <set>
#include <zlib.h>
#include <fstream>
#include <mutex>
#include <sys/stat.h>
#include "Posting.h"
#include "NzbReader.h"

Posting::Sketch Posting::MakeSketch(const std::vector<std::string>& messageIds)
{
	std::set<uint32_t> hashes;
	for (const std::string& id : messageIds)
	{
		hashes.insert((uint32_t)crc32(0, (const Bytef*)id.data(), (uInt)id.size()));
	}
	Sketch sketch;
	for (uint32_t hash : hashes)
	{
		if (sketch.size() >= SketchSize)
		{
			break;
		}
		sketch.push_back(hash);
	}
	return sketch;
}

bool Posting::SketchOfFile(const std::string& path, Sketch& sketch)
{
	static std::mutex mutex;
	static std::map<std::string, std::pair<std::pair<long long, long long>, Sketch>> cache;

	struct stat info;
	if (stat(path.c_str(), &info) != 0)
	{
		return false;
	}
	std::pair<long long, long long> stamp((long long)info.st_mtime, (long long)info.st_size);
	{
		std::lock_guard<std::mutex> guard(mutex);
		auto it = cache.find(path);
		if (it != cache.end() && it->second.first == stamp)
		{
			sketch = it->second.second;
			return true;
		}
	}

	std::ifstream file(path, std::ios::binary);
	std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	NzbSummary summary;
	if (!NzbReader::Parse(data, summary))
	{
		return false;
	}
	sketch = MakeSketch(summary.messageIds);

	std::lock_guard<std::mutex> guard(mutex);
	if (cache.size() > 4096)
	{
		cache.clear();
	}
	cache[path] = { stamp, sketch };
	return true;
}

bool Posting::SameSketch(const Sketch& a, const Sketch& b)
{
	size_t k = std::min(a.size(), b.size());
	if (!k)
	{
		return false;
	}
	Sketch common;
	std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(common));
	return common.size() >= std::max((size_t)1, k / 8);
}

bool Posting::SamePosting(const std::vector<std::string>& a, const std::vector<std::string>& b)
{
	std::vector<std::string> common;
	std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(common));
	return (double)common.size() > SamePostingShare * (double)std::min(a.size(), b.size());
}

std::vector<Posting::Group> Posting::GroupListings(std::vector<Newznab::Result> results)
{
	std::stable_sort(results.begin(), results.end(), [](const Newznab::Result& a, const Newznab::Result& b)
		{ return a.size != b.size ? a.size < b.size : a.date < b.date; });

	std::vector<Group> groups;
	for (const Newznab::Result& result : results)
	{
		if (!groups.empty())
		{
			const Newznab::Result& first = groups.back().front();
			if (result.size && result.date && first.date && result.size == first.size &&
				result.date - first.date <= RelistWindowSec)
			{
				groups.back().push_back(result);
				continue;
			}
		}
		groups.push_back({ result });
	}
	return groups;
}

bool Posting::ListingMismatch(const Newznab::Result& listing, long long nzbBytes)
{
	return listing.size > 0 && std::llabs(nzbBytes - listing.size) > ListingMismatchShare * (double)listing.size;
}

std::vector<Posting::Group> Posting::OrderPostings(const std::vector<Newznab::Result>& candidates,
	long long pickBytes, int maxPostings)
{
	// rank of every listing: closest size first, then most grabs, then newest
	std::vector<size_t> byRank(candidates.size());
	for (size_t i = 0; i < byRank.size(); i++)
	{
		byRank[i] = i;
	}
	auto worse = [&](size_t a, size_t b)
	{
		long long distA = std::llabs(candidates[a].size - pickBytes);
		long long distB = std::llabs(candidates[b].size - pickBytes);
		if (distA != distB)
		{
			return distA < distB;
		}
		if (candidates[a].grabs != candidates[b].grabs)
		{
			return candidates[a].grabs > candidates[b].grabs;
		}
		return candidates[a].date > candidates[b].date;
	};
	std::stable_sort(byRank.begin(), byRank.end(), worse);
	std::map<const Newznab::Result*, size_t> rank;
	for (size_t i = 0; i < byRank.size(); i++)
	{
		rank[&candidates[byRank[i]]] = i;
	}

	// group the listings (GroupListings works on copies; map back by link and date)
	std::vector<Group> groups = GroupListings(candidates);
	auto rankOf = [&](const Newznab::Result& result)
	{
		for (size_t i = 0; i < candidates.size(); i++)
		{
			if (candidates[i].link == result.link && candidates[i].size == result.size &&
				candidates[i].date == result.date)
			{
				return rank[&candidates[i]];
			}
		}
		return (size_t)0;
	};
	for (Group& group : groups)
	{
		std::stable_sort(group.begin(), group.end(), [&](const Newznab::Result& a, const Newznab::Result& b)
			{ return rankOf(a) < rankOf(b); });
	}
	std::stable_sort(groups.begin(), groups.end(), [&](const Group& a, const Group& b)
		{ return rankOf(a.front()) < rankOf(b.front()); });

	// one posting of each distinct size before the rest
	std::set<long long> seen;
	std::vector<Group> firsts, rest;
	for (Group& group : groups)
	{
		if (seen.insert(group.front().size).second)
		{
			firsts.push_back(std::move(group));
		}
		else
		{
			rest.push_back(std::move(group));
		}
	}
	std::vector<Group> ordered = std::move(firsts);
	for (Group& group : rest)
	{
		ordered.push_back(std::move(group));
	}

	if (maxPostings > 0 && ordered.size() > (size_t)(3 * maxPostings))
	{
		ordered.resize(3 * maxPostings);
	}
	return ordered;
}
