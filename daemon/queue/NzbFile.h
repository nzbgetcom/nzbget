/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2024 Denis <denis@nzbget.com>
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


#ifndef NZBFILE_H
#define NZBFILE_H

#include "NString.h"
#include "DownloadInfo.h"

class NzbFile
{
public:
	NzbFile(const char* fileName, const char* category);
	bool Parse();
	const char* GetFileName() const { return m_fileName.c_str(); }
	std::unique_ptr<NzbInfo> DetachNzbInfo() { return std::move(m_nzbInfo); }
	const std::string& GetPassword() const { return m_password; }

	void LogDebugInfo();

private:
	std::unique_ptr<NzbInfo> m_nzbInfo;
	std::string m_fileName;
	std::string m_password;

	void AddArticle(FileInfo* fileInfo, std::unique_ptr<ArticleInfo> articleInfo);
	void AddFileInfo(std::unique_ptr<FileInfo> fileInfo);
	void ParseSubject(FileInfo* fileInfo, bool TryQuotes);
	void BuildFilenames();
	void ProcessFiles();
	void CalcHashes();
	bool HasDuplicateFilenames();
	void ReadPasswordFromFilename();
	

	std::unique_ptr<FileInfo> m_fileInfo;
	ArticleInfo* m_article = nullptr;
	StringBuilder m_tagContent;
	bool m_ignoreNextError;
	bool m_hasPassword = false;

	static void SAX_StartElement(NzbFile* file, const char *name, const char **atts);
	static void SAX_EndElement(NzbFile* file, const char *name);
	static void SAX_characters(NzbFile* file, const char *  xmlstr, int len);
	static void* SAX_getEntity(NzbFile* file, const char *  name);
	static void SAX_error(NzbFile* file, const char *msg, ...);
	void Parse_StartElement(const char *name, const char **atts);
	void Parse_EndElement(const char *name);
	void Parse_Content(const char *buf, int len);
};

#endif
