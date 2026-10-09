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
#include <atomic>
#include <map>
#include <regex>
#include <set>
#include <thread>
#include "Newznab.h"
#include "HttpGet.h"
#include "DonorHealth.h"
#include "ReleaseName.h"
#include "XmlReader.h"
#include "Log.h"
#include "Util.h"

#include "DupeUtil.h"

using DupeUtil::Lower;
using DupeUtil::Trim;

namespace
{

long long ToInt(const std::string& text)
{
	return atoll(Trim(text).c_str());
}

class PageReader : public XmlReader::Handler
{
public:
	explicit PageReader(Newznab::Page& page) : m_page(page) {}

	void Start(const std::string& name, const XmlReader::Attrs& attrs) override
	{
		m_depth++;
		m_text.clear();
		if (m_depth == 1 && name == "error")
		{
			m_page.error = true;
			m_page.errorCode = atoi(XmlReader::Attr(attrs, "code").c_str());
			m_page.errorText = XmlReader::Attr(attrs, "description");
		}
		else if (name == "item")
		{
			m_inItem = true;
			m_item = Newznab::Result();
			m_attrSize = 0;
			m_elementSize = 0;
		}
		else if (m_inItem && name == "attr")
		{
			std::string key = XmlReader::Attr(attrs, "name");
			std::string value = XmlReader::Attr(attrs, "value");
			if (key == "size")
			{
				m_attrSize = ToInt(value);
			}
			else if (key == "grabs")
			{
				m_item.grabs = (int)ToInt(value);
			}
			else if (key == "usenetdate")
			{
				m_item.date = Newznab::ParseDate(value);
			}
			else if (key == "hydraIndexerName")
			{
				m_item.indexer = value;
			}
		}
	}

	void End(const std::string& name) override
	{
		if (m_inItem)
		{
			if (name == "title")
			{
				m_item.title = Trim(m_text);
			}
			else if (name == "link")
			{
				m_item.link = Trim(m_text);
			}
			else if (name == "size")
			{
				m_elementSize = ToInt(m_text);
			}
			else if (name == "item")
			{
				m_item.size = m_attrSize > 0 ? m_attrSize : m_elementSize;
				m_page.items.push_back(m_item);
				m_inItem = false;
			}
		}
		m_depth--;
		m_text.clear();
	}

	void Text(const std::string& text) override
	{
		m_text += text;
	}

private:
	Newznab::Page& m_page;
	Newznab::Result m_item;
	std::string m_text;
	int m_depth = 0;
	bool m_inItem = false;
	long long m_attrSize = 0;
	long long m_elementSize = 0;
};

std::string UrlEncode(const std::string& text)
{
	static const char* hex = "0123456789ABCDEF";
	std::string out;
	for (unsigned char ch : text)
	{
		if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~')
		{
			out += (char)ch;
		}
		else if (ch == ' ')
		{
			out += '+';
		}
		else
		{
			out += '%';
			out += hex[ch >> 4];
			out += hex[ch & 15];
		}
	}
	return out;
}

std::string DigitsOnly(const std::string& text)
{
	std::string out;
	for (char ch : text)
	{
		if (isdigit((unsigned char)ch))
		{
			out += ch;
		}
	}
	return out;
}

// days since 1970-01-01 of a civil date
long long DaysFromCivil(long long year, int month, int day)
{
	year -= month <= 2;
	long long era = (year >= 0 ? year : year - 399) / 400;
	long long yoe = year - era * 400;
	long long doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
	long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + doe - 719468;
}

}

bool Newznab::ParseResponse(const std::string& xml, Page& page)
{
	page = Page();
	PageReader reader(page);
	return XmlReader::Parse(xml, reader);
}

time_t Newznab::ParseDate(const std::string& text)
{
	// no date is this long, and std::regex recurses per character (a run of a
	// megabyte of spaces from an indexer ran the thread out of stack)
	if (text.size() > 64)
	{
		return 0;
	}
	// ISO 8601: 2025-06-10T01:10:05Z, 2025-06-10T03:10:05+02:00, 2025-06-10 01:10:05 (UTC)
	static const std::regex iso(
		"^\\s*(\\d{4})-(\\d{2})-(\\d{2})[Tt ](\\d{2}):(\\d{2}):(\\d{2})(?:\\.\\d+)?\\s*(Z|z|[+-]\\d{2}:?\\d{2})?\\s*$");
	// RFC 822/2822: [Tue, ]10 Jun 2025 01:10:05 +0000, also with a 2-digit year or a zone name
	static const std::regex rfc(
		"^\\s*(?:[A-Za-z]{3},\\s*)?(\\d{1,2})\\s+([A-Za-z]{3})[a-z]*\\s+(\\d{4}|\\d{2})\\s+(\\d{1,2}):(\\d{2}):(\\d{2})\\s*([+-]\\d{4}|[A-Za-z]+)?\\s*$");

	int year, month, day, hour, minute, second;
	std::string zone;
	std::smatch match;
	if (std::regex_match(text, match, iso))
	{
		year = atoi(match[1].str().c_str());
		month = atoi(match[2].str().c_str());
		day = atoi(match[3].str().c_str());
		hour = atoi(match[4].str().c_str());
		minute = atoi(match[5].str().c_str());
		second = atoi(match[6].str().c_str());
		zone = match[7].matched ? match[7].str() : "";
		zone.erase(std::remove(zone.begin(), zone.end(), ':'), zone.end());
		if (month < 1 || month > 12)
		{
			return 0;
		}
	}
	else if (std::regex_match(text, match, rfc))
	{
		static const char* months[] = { "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec" };
		std::string monthName = Lower(match[2].str());
		month = 0;
		for (int i = 0; i < 12; i++)
		{
			if (monthName == months[i])
			{
				month = i + 1;
			}
		}
		if (!month)
		{
			return 0;
		}
		year = atoi(match[3].str().c_str());
		if (match[3].length() == 2)
		{
			year += year < 50 ? 2000 : 1900;	// RFC 2822 section 4.3
		}
		day = atoi(match[1].str().c_str());
		hour = atoi(match[4].str().c_str());
		minute = atoi(match[5].str().c_str());
		second = atoi(match[6].str().c_str());
		zone = match[7].matched ? match[7].str() : "";
	}
	else
	{
		return 0;
	}

	if (day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60)
	{
		return 0;
	}

	long long seconds = DaysFromCivil(year, month, day) * 86400 + hour * 3600 + minute * 60 + second;

	// an offset like +0200, or a zone name (RFC 822 section 5.1); military letters
	// count as UTC (RFC 2822 section 4.3); another name can't be placed
	int offset = 0;
	if (zone.size() == 5 && (zone[0] == '+' || zone[0] == '-'))
	{
		offset = (atoi(zone.substr(1, 2).c_str()) * 3600 + atoi(zone.substr(3, 2).c_str()) * 60) *
			(zone[0] == '+' ? 1 : -1);
	}
	else if (!zone.empty())
	{
		std::string name = zone;
		for (char& ch : name)
		{
			ch = (char)toupper((unsigned char)ch);
		}
		static const std::map<std::string, int> zones = { { "UT", 0 }, { "UTC", 0 }, { "GMT", 0 }, { "Z", 0 },
			{ "EST", -5 }, { "EDT", -4 }, { "CST", -6 }, { "CDT", -5 }, { "MST", -7 }, { "MDT", -6 },
			{ "PST", -8 }, { "PDT", -7 } };
		auto it = zones.find(name);
		if (it != zones.end())
		{
			offset = it->second * 3600;
		}
		else if (name.size() != 1 || name == "J")
		{
			return 0;
		}
	}
	return (time_t)(seconds - offset);
}

std::vector<Newznab::Params> Newznab::BuildQueries(const std::string& title, const std::string& imdb,
	const std::string& tvdb)
{
	std::string normalized = ReleaseName::Normalize(title);
	std::string full = normalized;
	std::replace(full.begin(), full.end(), '.', ' ');
	std::string shortQuery = ReleaseName::ShortQuery(title);
	std::string group = ReleaseName::Parse(title).group;

	std::vector<Params> queries;
	queries.push_back({ { "t", "search" }, { "q", full } });
	if (!shortQuery.empty() && shortQuery != full)
	{
		queries.push_back({ { "t", "search" }, { "q", shortQuery } });
	}
	if (!shortQuery.empty() && !group.empty())
	{
		queries.push_back({ { "t", "search" }, { "q", shortQuery + " " + group } });
	}

	std::string imdbId = DigitsOnly(imdb);
	if (!imdbId.empty())
	{
		queries.push_back({ { "t", "movie" }, { "imdbid", imdbId } });
	}

	std::string tvdbId = DigitsOnly(tvdb);
	std::smatch match;
	static const std::regex episode("(?:^|\\.)s(\\d+)e(\\d+)(?:\\.|$)");
	if (!tvdbId.empty() && std::regex_search(normalized, match, episode))
	{
		queries.push_back({ { "t", "tvsearch" }, { "tvdbid", tvdbId },
			{ "season", std::to_string(atoi(match[1].str().c_str())) },
			{ "ep", std::to_string(atoi(match[2].str().c_str())) } });
	}
	return queries;
}

std::string Newznab::BuildUrl(const std::string& base, const Params& params, const std::string& apiKey)
{
	std::string url = base;
	// an address without a path ("http://host:5076") is the indexer's web page,
	// which answers every search with HTML (B91: every search "isn't XML"): the
	// api of a Newznab indexer is at "/api"
	size_t scheme = url.find("://");
	if (scheme != std::string::npos)
	{
		size_t hostEnd = std::min(url.find_first_of("/?", scheme + 3), url.size());
		size_t queryStart = std::min(url.find('?', hostEnd), url.size());
		std::string path = url.substr(hostEnd, queryStart - hostEnd);
		if (path.empty() || path == "/")
		{
			url = url.substr(0, hostEnd) + "/api" + url.substr(queryStart);
		}
	}
	url += url.find('?') == std::string::npos ? '?' : '&';
	for (const auto& param : params)
	{
		url += UrlEncode(param.first) + "=" + UrlEncode(param.second) + "&";
	}
	url += "apikey=" + UrlEncode(apiKey);
	return url;
}

std::string Newznab::Mask(const std::string& text)
{
	static const std::regex apiKey("(apikey=)[^&\\s]+", std::regex::icase);
	static const std::regex credentials("//[^/@\\s:]+:[^/@\\s]+@");
	// cut first: std::regex recurses per character of a match
	std::string masked = std::regex_replace(text.substr(0, 1024), apiKey, "$1***");
	return std::regex_replace(masked, credentials, "//***@");
}

std::vector<Newznab::Result> Newznab::Search(const std::string& base, const std::string& apiKey,
	const std::vector<Params>& queries, time_t deadline, SearchStats* stats)
{
	std::vector<std::vector<Result>> found(queries.size());
	std::atomic<size_t> next{0};
	std::atomic<int> failed{0};
	std::atomic<int> pages{0};

	auto worker = [&]()
	{
		for (size_t index = next++; index < queries.size(); index = next++)
		{
			std::string label;
			for (const auto& param : queries[index])
			{
				label += (label.empty() ? "" : " ") + param.first + "=" + param.second;
			}

			for (int page = 0; page < MaxPages; page++)
			{
				if (Util::CurrentTime() >= deadline)
				{
					detail("DupeSearch: no time left for the search %s", label.c_str());
					break;
				}

				Params params = queries[index];
				params.emplace_back("limit", std::to_string(PageSize));
				params.emplace_back("offset", std::to_string(page * PageSize));
				std::string infoName = "DupeSearch " + label + " page " + std::to_string(page + 1);
				long long deadlineMs = DonorHealth::NowMs() + (long long)(deadline - Util::CurrentTime()) * 1000;
				HttpGet::Reply reply = HttpGet::Fetch(BuildUrl(base, params, apiKey), infoName, 16 * 1024 * 1024,
					std::max(1LL, deadlineMs));
				// no answer at all (a dropped connection) is asked once more: one
				// lost query lost every result only it had
				if (!reply.ok && reply.status == 0 && !HttpGet::Stopped() && Util::CurrentTime() < deadline)
				{
					reply = HttpGet::Fetch(BuildUrl(base, params, apiKey), infoName, 16 * 1024 * 1024,
						std::max(1LL, deadlineMs));
				}
				pages++;

				Page result;
				if (!reply.ok)
				{
					warn("DupeSearch: the search %s failed (HTTP %i)", label.c_str(), reply.status);
					failed++;
					break;
				}
				if (!ParseResponse(reply.body, result))
				{
					warn("DupeSearch: the answer to the search %s isn't XML (is option DupeSearchUrl the "
						"indexer's api address, like http://host:5076/api?)", label.c_str());
					failed++;
					break;
				}
				if (result.error)
				{
					warn("DupeSearch: the indexer answered the search %s with error %i: %s",
						label.c_str(), result.errorCode, Mask(result.errorText).c_str());
					failed++;
					break;
				}

				found[index].insert(found[index].end(), result.items.begin(), result.items.end());
				if ((int)result.items.size() < PageSize)
				{
					break;
				}
			}
		}
	};

	std::vector<std::thread> threads;
	size_t count = std::min(queries.size(), (size_t)Parallel);
	for (size_t i = 0; i < count; i++)
	{
		threads.emplace_back(worker);
	}
	for (std::thread& thread : threads)
	{
		thread.join();
	}

	std::vector<Result> merged;
	std::set<std::string> links;
	for (const std::vector<Result>& results : found)
	{
		for (const Result& result : results)
		{
			if (!result.link.empty() && links.insert(result.link).second)
			{
				merged.push_back(result);
			}
		}
	}

	if (stats)
	{
		stats->queries = (int)queries.size();
		stats->failed = failed;
		stats->pages = pages;
	}
	return merged;
}
