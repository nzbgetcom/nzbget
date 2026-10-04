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
	std::ifstream file(m_statePath);
	std::string line;
	while (std::getline(file, line))
	{
		std::stringstream stream(line);
		std::string when, hashes;
		if (!std::getline(stream, when, '\t') || !std::getline(stream, hashes, '\t'))
		{
			continue;
		}
		Posting::Sketch sketch;
		std::stringstream list(hashes);
		std::string hash;
		while (std::getline(list, hash, ','))
		{
			sketch.push_back((uint32_t)strtoul(hash.c_str(), nullptr, 10));
		}
		if (!sketch.empty())
		{
			m_dead.emplace_back((time_t)atoll(when.c_str()), std::move(sketch));
		}
	}
	Prune(Util::CurrentTime());
}

// with m_mutex held
void DeadPostings::Prune(time_t now)
{
	m_dead.erase(std::remove_if(m_dead.begin(), m_dead.end(),
		[now](const std::pair<time_t, Posting::Sketch>& entry) { return now - entry.first >= DeadTtlSec; }),
		m_dead.end());
}

// with m_mutex held: a temporary file, renamed, so a crash can't leave half of the file
void DeadPostings::Save()
{
	if (m_statePath.empty())
	{
		return;
	}
	std::string temp = m_statePath + ".new";
	{
		std::ofstream file(temp, std::ios::trunc);
		for (const auto& entry : m_dead)
		{
			file << (long long)entry.first << '\t';
			for (size_t i = 0; i < entry.second.size(); i++)
			{
				file << (i ? "," : "") << entry.second[i];
			}
			file << "\t\n";
		}
		if (!file.good())
		{
			warn("Could not save the DupeSearch dead postings to %s", temp.c_str());
			return;
		}
	}
	FileSystem::MoveFile(temp.c_str(), m_statePath.c_str());
}

bool DeadPostings::IsDead(const Posting::Sketch& sketch, time_t now)
{
	std::lock_guard<std::mutex> guard(m_mutex);
	now = now ? now : Util::CurrentTime();
	for (const auto& entry : m_dead)
	{
		if (now - entry.first < DeadTtlSec && Posting::SameSketch(sketch, entry.second))
		{
			return true;
		}
	}
	return false;
}

void DeadPostings::Add(const Posting::Sketch& sketch, time_t now)
{
	std::lock_guard<std::mutex> guard(m_mutex);
	now = now ? now : Util::CurrentTime();
	Prune(now);
	m_dead.emplace_back(now, sketch);
	Save();
}
