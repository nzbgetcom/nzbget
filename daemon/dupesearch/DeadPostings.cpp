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

#include <fstream>
#include <sstream>
#include "DeadPostings.h"
#include "DupeUtil.h"
#include "FileSystem.h"
#include "Log.h"
#include "Util.h"

void DeadPostings::SetStatePath(const std::string& path)
{
	std::lock_guard<std::mutex> guard(m_mutex);
	m_statePath = path;
}

void DeadPostings::Load()
{
	std::lock_guard<std::mutex> guard(m_mutex);
	std::ifstream file(fs::u8path(m_statePath));
	std::string line;
	while (std::getline(file, line))
	{
		std::stringstream stream(line);
		std::string when, hashes;
		if (!std::getline(stream, when, '\t') || !std::getline(stream, hashes, '\t'))
		{
			continue;
		}
		// a line that doesn't parse in full is skipped: a corrupt hash must not become 0
		char* end = nullptr;
		long long stamp = strtoll(when.c_str(), &end, 10);
		if (when.empty() || *end)
		{
			continue;
		}
		Posting::Sketch sketch;
		std::stringstream list(hashes);
		std::string hash;
		bool ok = true;
		while (ok && std::getline(list, hash, ','))
		{
			unsigned long value = strtoul(hash.c_str(), &end, 10);
			ok = !hash.empty() && !*end && value <= 0xFFFFFFFFUL;
			sketch.push_back((uint32_t)value);
		}
		// SameSketch intersects sorted lists
		std::sort(sketch.begin(), sketch.end());
		sketch.erase(std::unique(sketch.begin(), sketch.end()), sketch.end());
		if (ok && !sketch.empty())
		{
			m_dead.emplace_back((time_t)stamp, std::move(sketch));
		}
	}
	Prune(Util::CurrentTime());
}

// with m_mutex held
void DeadPostings::Prune(time_t now)
{
	// expired, or dated more than a day ahead (clock skew or a corrupt time, which
	// would otherwise never expire)
	auto stale = [now](const std::pair<time_t, Posting::Sketch>& entry)
		{ return now - entry.first >= DeadTtlSec || entry.first - now > 24 * 3600; };
	m_dead.erase(std::remove_if(m_dead.begin(), m_dead.end(), stale), m_dead.end());
	m_unsaved.erase(std::remove_if(m_unsaved.begin(), m_unsaved.end(), stale), m_unsaved.end());
}

// with m_mutex held: a temporary file, renamed, so a crash can't leave half of the file
void DeadPostings::Save()
{
	if (m_statePath.empty())
	{
		return;
	}
	std::ostringstream text;
	for (const auto& entry : m_dead)
	{
		text << (long long)entry.first << '\t';
		for (size_t i = 0; i < entry.second.size(); i++)
		{
			text << (i ? "," : "") << entry.second[i];
		}
		text << "\t\n";
	}
	if (!DupeUtil::WriteAtomic(m_statePath, text.str()))
	{
		warn("Could not save the DupeSearch dead postings to %s", m_statePath.c_str());
	}
}

bool DeadPostings::IsDead(const Posting::Sketch& sketch, time_t now)
{
	std::lock_guard<std::mutex> guard(m_mutex);
	now = now ? now : Util::CurrentTime();
	for (const auto* list : { &m_dead, &m_unsaved })
	{
		for (const auto& entry : *list)
		{
			if (now - entry.first < DeadTtlSec && Posting::SameSketch(sketch, entry.second))
			{
				return true;
			}
		}
	}
	return false;
}

void DeadPostings::Add(const Posting::Sketch& sketch, time_t now, bool save)
{
	std::lock_guard<std::mutex> guard(m_mutex);
	now = now ? now : Util::CurrentTime();
	Prune(now);
	(save ? m_dead : m_unsaved).emplace_back(now, sketch);
	if (save)
	{
		Save();
	}
}
