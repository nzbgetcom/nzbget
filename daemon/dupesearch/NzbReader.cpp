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
#include <regex>
#include "NzbReader.h"
#include "XmlReader.h"

#include "DupeUtil.h"

using DupeUtil::Lower;
using DupeUtil::Trim;

namespace
{

// the file name of a subject: the first quoted string, else the whole subject
std::string NameOfSubject(const std::string& subject)
{
	size_t open = subject.find('"');
	if (open != std::string::npos)
	{
		size_t close = subject.find('"', open + 1);
		if (close != std::string::npos && close > open + 1)
		{
			return Trim(subject.substr(open + 1, close - open - 1));
		}
	}
	return Trim(subject);
}

bool IsParName(const std::string& name)
{
	static const std::regex regex("\\.par2$|\\.vol\\d+[+-]\\d+", std::regex::icase);
	return std::regex_search(name, regex);
}

class Reader : public XmlReader::Handler
{
public:
	void Start(const std::string& name, const XmlReader::Attrs& attrs) override
	{
		m_text.clear();
		if (name == "meta" && !XmlReader::Attr(attrs, "type").empty())
		{
			m_metaType = Lower(XmlReader::Attr(attrs, "type"));
		}
		else if (name == "file")
		{
			files++;
			m_inFile = true;
			if (poster.empty())
			{
				poster = XmlReader::Attr(attrs, "poster");
			}
			m_name = NameOfSubject(XmlReader::Attr(attrs, "subject"));
		}
		else if (name == "segment" && m_inFile)
		{
			m_segmentBytes = atoll(XmlReader::Attr(attrs, "bytes").c_str());
			m_inSegment = true;
		}
	}

	void End(const std::string& name) override
	{
		if (name == "meta" && !m_metaType.empty())
		{
			meta[m_metaType] = Trim(m_text);
			m_metaType.clear();
		}
		else if (name == "segment" && m_inSegment)
		{
			sizes[m_name] += m_segmentBytes;
			ids.insert(Trim(m_text));
			m_inSegment = false;
		}
		else if (name == "group" && m_inFile)
		{
			std::string group = Trim(m_text);
			if (!group.empty())
			{
				groups.insert(group);
			}
		}
		else if (name == "file")
		{
			m_inFile = false;
		}
		m_text.clear();
	}

	void Text(const std::string& text) override
	{
		m_text += text;
	}

	int files = 0;
	std::string poster;
	std::map<std::string, long long> sizes;
	std::set<std::string> ids;
	std::set<std::string> groups;
	std::map<std::string, std::string> meta;

private:
	std::string m_text;
	std::string m_metaType;
	std::string m_name;
	long long m_segmentBytes = 0;
	bool m_inFile = false;
	bool m_inSegment = false;
};

}

std::string NzbSummary::Fingerprint() const
{
	// FNV-1a over the sorted message-ids
	uint64_t hash = 1469598103934665603ULL;
	for (const std::string& id : messageIds)
	{
		for (unsigned char ch : id)
		{
			hash = (hash ^ ch) * 1099511628211ULL;
		}
		hash = (hash ^ '\n') * 1099511628211ULL;
	}
	char buffer[32];
	snprintf(buffer, sizeof(buffer), "%016llx", (unsigned long long)hash);
	return buffer;
}

bool NzbReader::Parse(const std::string& data, NzbSummary& summary)
{
	summary = NzbSummary();
	Reader reader;
	if (!XmlReader::Parse(data, reader) || !reader.files || reader.ids.empty())
	{
		return false;
	}

	summary.files = reader.files;
	summary.poster = reader.poster;
	summary.meta = reader.meta;
	summary.messageIds.assign(reader.ids.begin(), reader.ids.end());
	summary.groups.assign(reader.groups.begin(), reader.groups.end());

	std::string mainName;
	long long mainSize = -1;
	std::string anyName;
	long long anySize = -1;
	for (const auto& entry : reader.sizes)
	{
		summary.totalBytes += entry.second;
		summary.filenames.insert(Lower(entry.first));
		if (entry.second > anySize)
		{
			anySize = entry.second;
			anyName = entry.first;
		}
		if (!IsParName(entry.first) && entry.second > mainSize)
		{
			mainSize = entry.second;
			mainName = entry.first;
		}
	}
	summary.mainName = mainSize >= 0 ? mainName : anyName;
	return true;
}
