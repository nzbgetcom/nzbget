/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2013-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2024-2025 Denis <denis@nzbget.com>
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
#include "FeedFile.h"
#include "Log.h"
#include "DownloadInfo.h"
#include "Options.h"
#include "Util.h"

FeedFile::FeedFile(const char* filename, const char* infoName) 
	: m_fileName(filename ? filename : "")
	, m_infoName(infoName ? infoName : "")
{
	debug("Creating FeedFile");

	m_feedItems = std::make_unique<FeedItemList>();
	m_feedItemInfo = nullptr;
	m_tagContent.clear();
}

void FeedFile::LogDebugInfo()
{
	info(" FeedFile %s", m_fileName.c_str());
}

void FeedFile::ParseSubject(FeedItemInfo& feedItemInfo)
{
	// if title has quotation marks we use only part within quotation marks
	char* p = (char*)feedItemInfo.GetTitle();
	char* start = strchr(p, '\"');
	if (start)
	{
		start++;
		char* end = strchr(start + 1, '\"');
		if (end)
		{
			int len = (int)(end - start);
			char* point = strchr(start + 1, '.');
			if (point && point < end)
			{
				CString filename(start, len);

				char* ext = strrchr(filename, '.');
				if (ext && !strcasecmp(ext, ".par2"))
				{
					*ext = '\0';
				}

				feedItemInfo.SetFilename(filename);
				return;
			}
		}
	}

	feedItemInfo.SetFilename(feedItemInfo.GetTitle());
}


bool FeedFile::Parse()
{
	xmlSAXHandler SAX_handler{};
	SAX_handler.startElement = reinterpret_cast<startElementSAXFunc>(SAX_StartElement);
	SAX_handler.endElement = reinterpret_cast<endElementSAXFunc>(SAX_EndElement);
	SAX_handler.characters = reinterpret_cast<charactersSAXFunc>(SAX_characters);
	SAX_handler.error = reinterpret_cast<errorSAXFunc>(SAX_error);
	SAX_handler.getEntity = reinterpret_cast<getEntitySAXFunc>(SAX_getEntity);

	m_ignoreNextError = false;

	int ret = xmlSAXUserParseFile(&SAX_handler, this, m_fileName.c_str());

	if (ret != 0)
	{
		error("Failed to parse rss feed %s", m_infoName.c_str());
		return false;
	}

	return true;
}

void FeedFile::Parse_StartElement(const char* name, const char **atts)
{
	if (!name)
		return;

	ResetTagContent();

	if (!strcmp("item", name))
	{
		m_feedItems->emplace_back();
		m_feedItemInfo = &m_feedItems->back();
	}
	else if (!strcmp("enclosure", name) && m_feedItemInfo)
	{
		//<enclosure url="http://myindexer.com/fetch/9eeb264aecce961a6e0d" length="150263340" type="application/x-nzb" />
		for (; *atts; atts+=2)
		{
			if (!strcmp("url", atts[0]))
			{
				CString url = atts[1];
				WebUtil::XmlDecode(url);
				m_feedItemInfo->SetUrl(url);
			}
			else if (!strcmp("length", atts[0]))
			{
				int64 size = atoll(atts[1]);
				m_feedItemInfo->SetSize(size);
			}
		}
	}
	else if (m_feedItemInfo && 
		(!strcmp("newznab:attr", name) || !strcmp("nZEDb:attr", name)) &&
		atts[0] && atts[1] && atts[2] && atts[3] &&
		!strcmp("name", atts[0]) && !strcmp("value", atts[2]))
	{
		m_feedItemInfo->GetAttributes()->emplace_back(atts[1], atts[3]);

		//<newznab:attr name="size" value="5423523453534" />
		if (m_feedItemInfo->GetSize() == 0 &&
			!strcmp("size", atts[1]))
		{
			int64 size = atoll(atts[3]);
			m_feedItemInfo->SetSize(size);
		}

		//<newznab:attr name="imdb" value="1588173"/>
		else if (!strcmp("imdb", atts[1]))
		{
			m_feedItemInfo->SetImdbId(atoi(atts[3]));
		}

		//<newznab:attr name="rageid" value="33877"/>
		else if (!strcmp("rageid", atts[1]))
		{
			m_feedItemInfo->SetRageId(atoi(atts[3]));
		}

		//<newznab:attr name="tvdbid" value="33877"/>
		else if (!strcmp("tvdbid", atts[1]))
		{
			m_feedItemInfo->SetTvdbId(atoi(atts[3]));
		}

		//<newznab:attr name="tvmazeid" value="33877"/>
		else if (!strcmp("tvmazeid", atts[1]))
		{
			m_feedItemInfo->SetTvmazeId(atoi(atts[3]));
		}

		//<newznab:attr name="episode" value="E09"/>
		//<newznab:attr name="episode" value="9"/>
		else if (!strcmp("episode", atts[1]))
		{
			m_feedItemInfo->SetEpisode(atts[3]);
		}

		//<newznab:attr name="season" value="S03"/>
		//<newznab:attr name="season" value="3"/>
		else if (!strcmp("season", atts[1]))
		{
			m_feedItemInfo->SetSeason(atts[3]);
		}
	}
}

void FeedFile::Parse_EndElement(const char* name)
{
	if (!name)
		return;

	if (!strcmp("title", name) && m_feedItemInfo)
	{
		m_feedItemInfo->SetTitle(m_tagContent.c_str());
		ResetTagContent();
		ParseSubject(*m_feedItemInfo);
	}
	else if (!strcmp("link", name) && m_feedItemInfo &&
		(!m_feedItemInfo->GetUrl() || strlen(m_feedItemInfo->GetUrl()) == 0))
	{
		m_feedItemInfo->SetUrl(m_tagContent.c_str());
		ResetTagContent();
	}
	else if (!strcmp("category", name) && m_feedItemInfo)
	{
		m_feedItemInfo->SetCategory(m_tagContent.c_str());
		ResetTagContent();
	}
	else if (!strcmp("description", name) && m_feedItemInfo)
	{
		// cleanup CDATA
		CString description = m_tagContent.c_str();
		WebUtil::XmlStripTags(description);
		WebUtil::XmlDecode(description);
		WebUtil::XmlRemoveEntities(description);
		m_feedItemInfo->SetDescription(description);
		ResetTagContent();
	}
	else if (!strcmp("pubDate", name) && m_feedItemInfo)
	{
		time_t unixtime = WebUtil::ParseRfc822DateTime(m_tagContent.c_str());
		if (unixtime > 0)
		{
			m_feedItemInfo->SetTime(unixtime);
		}
		ResetTagContent();
	}
}

void FeedFile::Parse_Content(std::string content)
{
	m_tagContent.append(std::move(content));
}

void FeedFile::ResetTagContent()
{
	m_tagContent.clear();
}

void FeedFile::SAX_StartElement(FeedFile* file, const char *name, const char **atts)
{
	file->Parse_StartElement(name, atts);
}

void FeedFile::SAX_EndElement(FeedFile* file, const char *name)
{
	file->Parse_EndElement(name);
}

void FeedFile::SAX_characters(FeedFile* file, const char* xmlstr, int len)
{
	if (!xmlstr || len <= 0)
		return;
	
	std::string str(xmlstr, len);
	Util::Trim(str);
	file->Parse_Content(std::move(str));
}

xmlEntityPtr FeedFile::SAX_getEntity(FeedFile* file, const xmlChar* name)
{
	xmlEntityPtr e = xmlGetPredefinedEntity(name);

	if (!e)
	{
		warn("entity not found");
		file->m_ignoreNextError = true;
	}

	return e;
}

void FeedFile::SAX_error(FeedFile* file, const char *msg, ...)
{
	if (file->m_ignoreNextError)
	{
		file->m_ignoreNextError = false;
		return;
	}

	va_list argp;
	va_start(argp, msg);
	char errMsg[1024];
	vsnprintf(errMsg, sizeof(errMsg), msg, argp);
	errMsg[1024-1] = '\0';
	va_end(argp);

	// remove trailing CRLF
	for (char* pend = errMsg + strlen(errMsg) - 1; pend >= errMsg && (*pend == '\n' || *pend == '\r' || *pend == ' '); pend--) *pend = '\0';
	error("Error parsing rss feed %s: %s", file->m_infoName.c_str(), errMsg);
}
