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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef FEEDFILE_H
#define FEEDFILE_H

#include <string>
#include <string_view>
#include "FeedInfo.h"

class FeedFile
{
public:
	FeedFile(const char* fileName, const char* infoName);
	bool Parse();
	std::unique_ptr<FeedItemList> DetachFeedItems() { return std::move(m_feedItems); }

	void LogDebugInfo();

private:
	std::unique_ptr<FeedItemList> m_feedItems;
	std::string m_fileName;
	std::string m_infoName;

	void ParseSubject(FeedItemInfo& feedItemInfo);

	FeedItemInfo* m_feedItemInfo;
	std::string m_tagContent;
	bool m_ignoreNextError;

	static void SAX_StartElement(FeedFile* file, const char *name, const char **atts);
	static void SAX_EndElement(FeedFile* file, const char *name);
	static void SAX_textHandler(FeedFile* file, const char *  xmlstr, int len);
	static xmlEntityPtr SAX_getEntity(FeedFile* file, const xmlChar*  name);
	static void SAX_error(FeedFile* file, const char *msg, ...);
	void Parse_StartElement(const char *name, const char **atts);
	void Parse_EndElement(const char *name);
	void Parse_Content(std::string content);

	/**
	 * Extracts the file size in bytes from a description string.
	 * Some RSS feeds embed the file size directly within the description text.
	 * Example description string: "Total Size : 299.6 MB | MultiUp"
	 */
	int64 ExtractSizeFromDescription(std::string_view description);
	void ResetTagContent();
};

#endif
