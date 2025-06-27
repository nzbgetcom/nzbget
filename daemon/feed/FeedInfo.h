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


#ifndef FEEDINFO_H
#define FEEDINFO_H

#include <string>
#include "Util.h"
#include "DownloadInfo.h"

class FeedInfo
{
public:
	enum EStatus
	{
		fsUndefined,
		fsRunning,
		fsFinished,
		fsFailed
	};

	FeedInfo(
		int id,
		const char* name,
		const char* url,
		bool backlog,
		int interval,
		const char* filter,
		bool pauseNzb,
		const char* category,
		int priority,
		const char* extensions
	);
	int GetId() { return m_id; }
	const char* GetName() const { return m_name.c_str(); }
	const char* GetUrl() const { return m_url.c_str(); }
	int GetInterval() { return m_interval; }
	const char* GetFilter() const { return m_filter.c_str(); }
	uint32 GetFilterHash() { return m_filterHash; }
	bool GetPauseNzb() { return m_pauseNzb; }
	const char* GetCategory() const { return m_category.c_str(); }
	int GetPriority() { return m_priority; }
	const char* GetExtensions() const { return m_extensions.c_str(); }
	time_t GetLastUpdate() { return m_lastUpdate; }
	void SetLastUpdate(time_t lastUpdate) { m_lastUpdate = lastUpdate; }
	time_t GetNextUpdate() { return m_nextUpdate; }
	void SetNextUpdate(time_t nextUpdate) { m_nextUpdate = nextUpdate; }
	int GetLastInterval() { return m_lastInterval; }
	void SetLastInterval(int lastInterval) { m_lastInterval = lastInterval; }
	bool GetPreview() { return m_preview; }
	void SetPreview(bool preview) { m_preview = preview; }
	EStatus GetStatus() { return m_status; }
	void SetStatus(EStatus Status) { m_status = Status; }
	const char* GetOutputFilename() const { return m_outputFilename.c_str(); }
	void SetOutputFilename(const char* outputFilename) { m_outputFilename = outputFilename ? outputFilename : ""; }
	bool GetFetch() { return m_fetch; }
	void SetFetch(bool fetch) { m_fetch = fetch; }
	bool GetForce() { return m_force; }
	void SetForce(bool force) { m_force = force; }
	bool GetBacklog() { return m_backlog; }
	void SetBacklog(bool backlog) { m_backlog = backlog; }

private:
	int m_id;
	std::string m_name;
	std::string m_url;
	std::string m_category;
	std::string m_extensions;
	std::string m_filter;
	std::string m_outputFilename;
	time_t m_lastUpdate = 0;
	time_t m_nextUpdate = 0;
	uint32 m_filterHash;
	EStatus m_status = fsUndefined;
	int m_interval;
	int m_priority;
	int m_lastInterval = 0;
	bool m_backlog;
	bool m_pauseNzb;
	bool m_preview = false;
	bool m_fetch = false;
	bool m_force = false;
};

typedef std::deque<std::unique_ptr<FeedInfo>> Feeds;

class FeedFilterHelper
{
public:
	virtual std::unique_ptr<RegEx>& GetRegEx(int id) = 0;
	virtual void CalcDupeStatus(const char* title, const char* dupeKey, char* statusBuf, int bufLen) = 0;
};

class FeedItemInfo
{
public:
	enum EStatus
	{
		isUnknown,
		isBacklog,
		isFetched,
		isNew
	};

	enum EMatchStatus
	{
		msIgnored,
		msAccepted,
		msRejected
	};

	class Attr
	{
	public:
		Attr(const char* name, const char* value)
			: m_name(name ? name : "")
			, m_value(value ? value : "") 
		{ }
		const char* GetName() const { return m_name.c_str(); }
		const char* GetValue() const { return m_value.c_str(); }
	private:
		std::string m_name;
		std::string m_value;
	};

	typedef std::deque<Attr> AttributesBase;

	class Attributes: public AttributesBase
	{
	public:
		Attr* Find(const char* name);
	};

	FeedItemInfo() {}
	FeedItemInfo(FeedItemInfo&&) = delete; // catch performance issues
	void SetFeedFilterHelper(FeedFilterHelper* feedFilterHelper) { m_feedFilterHelper = feedFilterHelper; }
	const char* GetTitle() const { return m_title.c_str(); }
	void SetTitle(const char* title) { m_title = title ? title : ""; }
	const char* GetFilename() const { return m_filename.c_str(); }
	void SetFilename(const char* filename) { m_filename = filename ? filename : ""; }
	const char* GetUrl() const { return m_url.c_str(); }
	void SetUrl(const char* url) { m_url = url ? url : ""; }
	int64 GetSize() { return m_size; }
	void SetSize(int64 size) { m_size = size; }
	const char* GetCategory() const { return m_category.c_str(); }
	void SetCategory(const char* category) { m_category = category ? category : ""; }
	int GetImdbId() { return m_imdbId; }
	void SetImdbId(int imdbId) { m_imdbId = imdbId; }
	int GetRageId() { return m_rageId; }
	void SetRageId(int rageId) { m_rageId = rageId; }
	int GetTvdbId() { return m_tvdbId; }
	void SetTvdbId(int tvdbId) { m_tvdbId = tvdbId; }
	int GetTvmazeId() { return m_tvmazeId; }
	void SetTvmazeId(int tvmazeId) { m_tvmazeId = tvmazeId; }
	const char* GetDescription() const { return m_description.c_str(); }
	void SetDescription(const char* description) { m_description = description ? description: ""; }
	const char* GetSeason() const { return m_season.c_str(); }
	void SetSeason(const char* season);
	const char* GetEpisode() const { return m_episode.c_str(); }
	void SetEpisode(const char* episode);
	int GetSeasonNum();
	int GetEpisodeNum();
	const char* GetAddCategory() const { return m_addCategory.c_str(); }
	void SetAddCategory(const char* addCategory) { m_addCategory = addCategory ? addCategory : ""; }
	bool GetPauseNzb() { return m_pauseNzb; }
	void SetPauseNzb(bool pauseNzb) { m_pauseNzb = pauseNzb; }
	int GetPriority() { return m_priority; }
	void SetPriority(int priority) { m_priority = priority; }
	time_t GetTime() { return m_time; }
	void SetTime(time_t time) { m_time = time; }
	EStatus GetStatus() { return m_status; }
	void SetStatus(EStatus status) { m_status = status; }
	EMatchStatus GetMatchStatus() { return m_matchStatus; }
	void SetMatchStatus(EMatchStatus matchStatus) { m_matchStatus = matchStatus; }
	int GetMatchRule() { return m_matchRule; }
	void SetMatchRule(int matchRule) { m_matchRule = matchRule; }
	const char* GetDupeKey() const { return m_dupeKey.c_str(); }
	void SetDupeKey(const char* dupeKey) { m_dupeKey = dupeKey ? dupeKey : ""; }
	void AppendDupeKey(const char* extraDupeKey);
	void BuildDupeKey(const char* rageId, const char* tvdbId, const char* tvmazeId, const char* series);
	int GetDupeScore() { return m_dupeScore; }
	void SetDupeScore(int dupeScore) { m_dupeScore = dupeScore; }
	EDupeMode GetDupeMode() { return m_dupeMode; }
	void SetDupeMode(EDupeMode dupeMode) { m_dupeMode = dupeMode; }
	const char* GetDupeStatus();
	Attributes* GetAttributes() { return &m_attributes; }

private:
	std::string m_title;
	std::string m_filename;
	std::string m_url;
	std::string m_category;
	std::string m_description;
	std::string m_season;
	std::string m_episode;
	std::string m_addCategory;
	std::string m_dupeKey;
	std::string m_dupeStatus;
	time_t m_time = 0;
	int64 m_size = 0;
	int m_imdbId = 0;
	int m_rageId = 0;
	int m_tvdbId = 0;
	int m_tvmazeId = 0;
	int m_seasonNum = 0;
	int m_episodeNum = 0;
	bool m_seasonEpisodeParsed = false;
	bool m_pauseNzb = false;
	int m_priority = 0;
	EStatus m_status = isUnknown;
	EMatchStatus m_matchStatus = msIgnored;
	int m_matchRule = 0;
	int m_dupeScore = 0;
	EDupeMode m_dupeMode = dmScore;
	FeedFilterHelper* m_feedFilterHelper = nullptr;
	Attributes m_attributes;

	int ParsePrefixedInt(const char *value);
	void ParseSeasonEpisode();
};

typedef std::deque<FeedItemInfo> FeedItemList;

class FeedHistoryInfo
{
public:
	enum EStatus
	{
		hsUnknown,
		hsBacklog,
		hsFetched
	};

	FeedHistoryInfo(const char* url, EStatus status, time_t lastSeen) 
		: m_url{ url ? url : "" }
		, m_status{ status }
		, m_lastSeen{ lastSeen } {}
	const char* GetUrl() const { return m_url.c_str(); }
	EStatus GetStatus() { return m_status; }
	void SetStatus(EStatus Status) { m_status = Status; }
	time_t GetLastSeen() { return m_lastSeen; }
	void SetLastSeen(time_t lastSeen) { m_lastSeen = lastSeen; }

private:
	std::string m_url;
	EStatus m_status;
	time_t m_lastSeen;
};

typedef std::deque<FeedHistoryInfo> FeedHistoryBase;

class FeedHistory : public FeedHistoryBase
{
public:
	void Remove(const char* url);
	FeedHistoryInfo* Find(const char* url);
};

#endif
