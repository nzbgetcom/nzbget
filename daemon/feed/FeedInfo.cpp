/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2013-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "FeedInfo.h"
#include "Util.h"

FeedInfo::FeedInfo(
	int id,
	const char* name,
	const char* url,
	bool backlog,
	int interval,
	const char* filter,
	bool pauseNzb,
	const char* category,
	FeedInfo::CategorySource categorySource,
	int priority,
	const char* extensions
)
	: m_id{ id }
	, m_name{ name ? name : "" }
	, m_url{ url ? url : "" }
	, m_filter{ filter ? filter : "" }
	, m_category{ category ? category : "" }
	, m_categorySource{ categorySource }
	, m_extensions{ extensions ? extensions : "" }
	, m_backlog{ backlog}
	, m_interval{ interval }
	, m_pauseNzb{ pauseNzb }
	, m_priority{ priority }
{
	if (m_name.empty())
	{
		m_name = "Feed" + std::to_string(m_id);
	}
	m_filterHash = Util::HashBJ96(m_filter.c_str(), m_filter.size(), 0);
}

FeedItemInfo::Attr* FeedItemInfo::Attributes::Find(const char* name)
{
	for (Attr& attr : this)
	{
		if (!strcasecmp(attr.GetName(), name))
		{
			return &attr;
		}
	}

	return nullptr;
}


void FeedItemInfo::SetSeason(const char* season)
{
	m_season = season ? season : "";
	m_seasonNum = season ? ParsePrefixedInt(season) : 0;
}

void FeedItemInfo::SetEpisode(const char* episode)
{
	m_episode = episode ? episode : "";
	m_episodeNum = episode ? ParsePrefixedInt(episode) : 0;
}

int FeedItemInfo::ParsePrefixedInt(const char *value)
{
	const char* val = value;
	if (!strchr("0123456789", *val))
	{
		val++;
	}
	return atoi(val);
}

void FeedItemInfo::AppendDupeKey(const char* extraDupeKey)
{
	if (!m_dupeKey.empty() && !Util::EmptyStr(extraDupeKey))
	{
		m_dupeKey += "-";
		m_dupeKey += extraDupeKey;
	}
}

void FeedItemInfo::BuildDupeKey(const char* rageId, const char* tvdbId, const char* tvmazeId, const char* series)
{
	int rageIdVal = !Util::EmptyStr(rageId) ? atoi(rageId) : m_rageId;
	int tvdbIdVal = !Util::EmptyStr(tvdbId) ? atoi(tvdbId) : m_tvdbId;
	int tvmazeIdVal = !Util::EmptyStr(tvmazeId) ? atoi(tvmazeId) : m_tvmazeId;

	if (m_imdbId != 0)
	{
		m_dupeKey = "imdb=" + std::to_string(m_imdbId);
	}
	else if (!Util::EmptyStr(series) && GetSeasonNum() != 0 && GetEpisodeNum() != 0)
	{
		m_dupeKey = std::string("series=") + series + "-" + m_season + "-" + m_episode;
	}
	else if (rageIdVal != 0 && GetSeasonNum() != 0 && GetEpisodeNum() != 0)
	{
		m_dupeKey = "rageid=" + std::to_string(rageIdVal) + "-" + m_season + "-" + m_episode;
	}
	else if (tvdbIdVal != 0 && GetSeasonNum() != 0 && GetEpisodeNum() != 0)
	{
		m_dupeKey = "tvdbid=" + std::to_string(tvdbIdVal) + "-" + m_season + "-" + m_episode;
	}
	else if (tvmazeIdVal != 0 && GetSeasonNum() != 0 && GetEpisodeNum() != 0)
	{
		m_dupeKey = "tvmazeid=" + std::to_string(tvmazeIdVal) + "-" + m_season + "-" + m_episode;
	}
	else
	{
		m_dupeKey = "";
	}

	m_dupeKey.shrink_to_fit();
}

int FeedItemInfo::GetSeasonNum()
{
	if (m_season.empty() && !m_seasonEpisodeParsed)
	{
		ParseSeasonEpisode();
	}

	return m_seasonNum;
}

int FeedItemInfo::GetEpisodeNum()
{
	if (m_episode.empty() && !m_seasonEpisodeParsed)
	{
		ParseSeasonEpisode();
	}

	return m_episodeNum;
}

void FeedItemInfo::ParseSeasonEpisode()
{
	if (!m_feedFilterHelper)
	{
		return;
	}

	m_seasonEpisodeParsed = true;

	const char* pattern = "[^[:alnum:]]s?([0-9]+)[ex]([0-9]+(-?e[0-9]+)?)[^[:alnum:]]";

	std::unique_ptr<RegEx>& regEx = m_feedFilterHelper->GetRegEx(1);
	if (!regEx)
	{
		regEx = std::make_unique<RegEx>(pattern, 10);
	}

	if (regEx->Match(m_title.c_str()))
	{
		const char* season = m_title.c_str() + regEx->GetMatchStart(1);
		int seasonNum = Util::StrToNum<int>(season).value_or(0);
		SetSeason(BString<100>("S%02d", seasonNum));

		BString<100> regValue;
		const char* val = m_title.c_str() + regEx->GetMatchStart(2);
		regValue.Set(val, regEx->GetMatchLen(2));

		BString<100> episode("E%s", *regValue);
		Util::ReduceStr(episode, "-", "");
		for (char* p = episode; *p; p++) *p = toupper(*p); // convert string to uppercase e02 -> E02
		SetEpisode(episode);
	}
}

const char* FeedItemInfo::GetDupeStatus()
{
	if (m_dupeStatus.empty())
	{
		BString<1024> statuses;
		m_feedFilterHelper->CalcDupeStatus(m_title.c_str(), m_dupeKey.c_str(), statuses, statuses.Capacity());
		m_dupeStatus = statuses;
	}

	return m_dupeStatus.c_str();
}


void FeedHistory::Remove(const char* url)
{
	for (iterator it = begin(); it != end(); it++)
	{
		FeedHistoryInfo& feedHistoryInfo = *it;
		if (!strcmp(feedHistoryInfo.GetUrl(), url))
		{
			erase(it);
			break;
		}
	}
}

FeedHistoryInfo* FeedHistory::Find(const char* url)
{
	for (FeedHistoryInfo& feedHistoryInfo : this)
	{
		if (!strcmp(feedHistoryInfo.GetUrl(), url))
		{
			return &feedHistoryInfo;
		}
	}

	return nullptr;
}
