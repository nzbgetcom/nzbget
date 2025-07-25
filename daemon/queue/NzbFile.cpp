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


#include "nzbget.h"
#include "NzbFile.h"
#include "Log.h"
#include "DownloadInfo.h"
#include "Options.h"
#include "DiskState.h"
#include "Util.h"
#include "FileSystem.h"
#include "Deobfuscation.h"

NzbFile::NzbFile(const char* fileName, const char* category) :
	m_fileName(fileName)
{
	debug("Creating NZBFile");

	m_nzbInfo = std::make_unique<NzbInfo>();
	m_nzbInfo->SetFilename(fileName);
	m_nzbInfo->SetCategory(category);
	m_nzbInfo->BuildDestDirName();
}

void NzbFile::LogDebugInfo()
{
	info(" NZBFile %s", m_fileName.c_str());
}

void NzbFile::AddArticle(FileInfo* fileInfo, std::unique_ptr<ArticleInfo> articleInfo)
{
	int index = articleInfo->GetPartNumber() - 1;

	// make Article-List big enough
	if (index >= (int)fileInfo->GetArticles()->size())
	{
		fileInfo->GetArticles()->resize(index + 1);
	}

	(*fileInfo->GetArticles())[index] = std::move(articleInfo);
}

void NzbFile::AddFileInfo(std::unique_ptr<FileInfo> fileInfo)
{
	// calculate file size and delete empty articles

	int64 size = 0;
	int64 missedSize = 0;
	int64 oneSize = 0;
	int uncountedArticles = 0;
	int missedArticles = 0;
	int totalArticles = (int)fileInfo->GetArticles()->size();
	int i = 0;
	for (ArticleList::iterator it = fileInfo->GetArticles()->begin(); it != fileInfo->GetArticles()->end(); )
	{
		ArticleInfo* article = (*it).get();
		if (!article)
		{
			fileInfo->GetArticles()->erase(it);
			it = fileInfo->GetArticles()->begin() + i;
			missedArticles++;
			if (oneSize > 0)
			{
				missedSize += oneSize;
			}
			else
			{
				uncountedArticles++;
			}
		}
		else
		{
			size += article->GetSize();
			if (oneSize == 0)
			{
				oneSize = article->GetSize();
			}
			it++;
			i++;
		}
	}

	if (fileInfo->GetArticles()->empty())
	{
		return;
	}

	missedSize += uncountedArticles * oneSize;
	size += missedSize;
	fileInfo->SetNzbInfo(m_nzbInfo.get());
	fileInfo->SetSize(size);
	fileInfo->SetRemainingSize(size - missedSize);
	fileInfo->SetMissedSize(missedSize);
	fileInfo->SetTotalArticles(totalArticles);
	fileInfo->SetMissedArticles(missedArticles);
	m_nzbInfo->GetFileList()->Add(std::move(fileInfo));
}

void NzbFile::ParseSubject(FileInfo* fileInfo, bool TryQuotes)
{
	if (!fileInfo) return;

	if (!fileInfo->GetSubject())
	{
		// Malformed file element without subject. We generate subject using internal element id.
		fileInfo->SetSubject(CString::FormatStr("%d", fileInfo->GetId()));
		return;
	}

	detail("Extracting a filename from Subject %s", fileInfo->GetSubject());

	std::string filename = Deobfuscation::Deobfuscate(fileInfo->GetSubject());

	detail("Extracted Filename: %s", filename.c_str());

	fileInfo->SetFilename(std::move(filename));
}

bool NzbFile::HasDuplicateFilenames()
{
	for (FileList::iterator it = m_nzbInfo->GetFileList()->begin(); it != m_nzbInfo->GetFileList()->end(); it++)
	{
		FileInfo* fileInfo1 = (*it).get();
		int dupe = 1;
		for (FileList::iterator it2 = it + 1; it2 != m_nzbInfo->GetFileList()->end(); it2++)
		{
			FileInfo* fileInfo2 = (*it2).get();
			if (!strcmp(fileInfo1->GetFilename(), fileInfo2->GetFilename()) &&
				strcmp(fileInfo1->GetSubject(), fileInfo2->GetSubject()))
			{
				dupe++;
			}
		}

		// If more than two files have the same parsed filename but different subjects,
		// this means, that the parsing was not correct.
		// in this case we take subjects as filenames to prevent
		// false "duplicate files"-alarm.
		// It's Ok for just two files to have the same filename, this is
		// an often case by posting-errors to repost bad files
		if (dupe > 2 || (dupe == 2 && m_nzbInfo->GetFileList()->size() == 2))
		{
			return true;
		}
	}

	return false;
}

/**
 * Generate filenames from subjects and check if the parsing of subject was correct
 */
void NzbFile::BuildFilenames()
{
	for (FileInfo* fileInfo : m_nzbInfo->GetFileList())
	{
		ParseSubject(fileInfo, true);
	}

	if (HasDuplicateFilenames())
	{
		for (FileInfo* fileInfo : m_nzbInfo->GetFileList())
		{
			ParseSubject(fileInfo, false);
		}
	}

	if (HasDuplicateFilenames())
	{
		m_nzbInfo->SetManyDupeFiles(true);
		for (FileInfo* fileInfo : m_nzbInfo->GetFileList())
		{
			fileInfo->SetFilename(fileInfo->GetSubject());
		}
	}
}

void NzbFile::CalcHashes()
{
	RawFileList sortedFiles;

	for (FileInfo* fileInfo : m_nzbInfo->GetFileList())
	{
		sortedFiles.push_back(fileInfo);
	}

	std::sort(sortedFiles.begin(), sortedFiles.end(),
		[](FileInfo* first, FileInfo* second)
		{
			return strcmp(first->GetFilename(), second->GetFilename()) > 0;
		});

	uint32 fullContentHash = 0;
	uint32 filteredContentHash = 0;
	int useForFilteredCount = 0;

	for (FileInfo* fileInfo : sortedFiles)
	{
		// check file extension
		bool skip = !fileInfo->GetParFile() &&
			Util::MatchFileExt(fileInfo->GetFilename(), g_Options->GetParIgnoreExt(), ",;");

		for (ArticleInfo* article: fileInfo->GetArticles())
		{
			int len = strlen(article->GetMessageId());
			fullContentHash = Util::HashBJ96(article->GetMessageId(), len, fullContentHash);
			if (!skip)
			{
				filteredContentHash = Util::HashBJ96(article->GetMessageId(), len, filteredContentHash);
				useForFilteredCount++;
			}
		}
	}

	// if filtered hash is based on less than a half of files - do not use filtered hash at all
	if (useForFilteredCount < (int)sortedFiles.size() / 2)
	{
		filteredContentHash = 0;
	}

	m_nzbInfo->SetFullContentHash(fullContentHash);
	m_nzbInfo->SetFilteredContentHash(filteredContentHash);
}

void NzbFile::ProcessFiles()
{
	BuildFilenames();

	for (FileInfo* fileInfo : m_nzbInfo->GetFileList())
	{
		fileInfo->MakeValidFilename();

		BString<1024> loFileName = fileInfo->GetFilename();
		for (char* p = loFileName; *p; p++) *p = tolower(*p); // convert string to lowercase
		bool parFile = strstr(loFileName, ".par2");

		m_nzbInfo->SetFileCount(m_nzbInfo->GetFileCount() + 1);
		m_nzbInfo->SetTotalArticles(m_nzbInfo->GetTotalArticles() + fileInfo->GetTotalArticles());
		m_nzbInfo->SetFailedArticles(m_nzbInfo->GetFailedArticles() + fileInfo->GetMissedArticles());
		m_nzbInfo->SetCurrentFailedArticles(m_nzbInfo->GetCurrentFailedArticles() + fileInfo->GetMissedArticles());
		m_nzbInfo->SetSize(m_nzbInfo->GetSize() + fileInfo->GetSize());
		m_nzbInfo->SetRemainingSize(m_nzbInfo->GetRemainingSize() + fileInfo->GetRemainingSize());
		m_nzbInfo->SetFailedSize(m_nzbInfo->GetFailedSize() + fileInfo->GetMissedSize());
		m_nzbInfo->SetCurrentFailedSize(m_nzbInfo->GetFailedSize());

		fileInfo->SetParFile(parFile);
		if (parFile)
		{
			m_nzbInfo->SetParSize(m_nzbInfo->GetParSize() + fileInfo->GetSize());
			m_nzbInfo->SetParFailedSize(m_nzbInfo->GetParFailedSize() + fileInfo->GetMissedSize());
			m_nzbInfo->SetParCurrentFailedSize(m_nzbInfo->GetParFailedSize());
			m_nzbInfo->SetRemainingParCount(m_nzbInfo->GetRemainingParCount() + 1);
		}
	}

	m_nzbInfo->UpdateMinMaxTime();

	CalcHashes();

	if (g_Options->GetServerMode())
	{
		for (FileInfo* fileInfo : m_nzbInfo->GetFileList())
		{
			g_DiskState->SaveFile(fileInfo);
			fileInfo->GetArticles()->clear();
		}
	}

	if (m_password.empty())
	{
		ReadPasswordFromFilename();
	}
}
/*
* Attempt to Read the Password from the Filename encoded in {{ Bracets }}
*/
void NzbFile::ReadPasswordFromFilename()
{
	size_t start = m_fileName.find("{{");
	if (start == std::string::npos) return;
	
	start += 2;

	size_t end = m_fileName.find("}}", start);
	if (end == std::string::npos) return;

	if (start < end)
	    m_password = m_fileName.substr(start, end - start);
}

bool NzbFile::Parse()
{
	xmlSAXHandler SAX_handler = {0};
	SAX_handler.startElement = reinterpret_cast<startElementSAXFunc>(SAX_StartElement);
	SAX_handler.endElement = reinterpret_cast<endElementSAXFunc>(SAX_EndElement);
	SAX_handler.characters = reinterpret_cast<charactersSAXFunc>(SAX_characters);
	SAX_handler.error = reinterpret_cast<errorSAXFunc>(SAX_error);
	SAX_handler.getEntity = reinterpret_cast<getEntitySAXFunc>(SAX_getEntity);

	m_ignoreNextError = false;

	int ret = xmlSAXUserParseFile(&SAX_handler, this, m_fileName.c_str());

	if (ret != 0)
	{
		m_nzbInfo->AddMessage(Message::mkError, BString<1024>(
			"Error parsing nzb-file %s", FileSystem::BaseFileName(m_fileName.c_str())));
		return false;
	}

	if (m_nzbInfo->GetFileList()->empty())
	{
		m_nzbInfo->AddMessage(Message::mkError, BString<1024>(
			"Error parsing nzb-file %s: file has no content", FileSystem::BaseFileName(m_fileName.c_str())));
		return false;
	}

	ProcessFiles();

	return true;
}

void NzbFile::Parse_StartElement(const char *name, const char **atts)
{
	BString<1024> tagAttrMessage("Malformed nzb-file, tag <%s> must have attributes", name);

	m_tagContent.Clear();

	if (!strcmp("file", name))
	{
		m_fileInfo = std::make_unique<FileInfo>();
		m_fileInfo->SetFilename(m_fileName.c_str());

		if (!atts)
		{
			m_nzbInfo->AddMessage(Message::mkWarning, tagAttrMessage);
			return;
		}

		for (int i = 0; atts[i]; i += 2)
		{
			const char* attrname = atts[i];
			const char* attrvalue = atts[i + 1];
			if (!strcmp("subject", attrname))
			{
				m_fileInfo->SetSubject(attrvalue);
			}
			if (!strcmp("date", attrname))
			{
				m_fileInfo->SetTime(atoi(attrvalue));
			}
		}
	}
	else if (!strcmp("segment", name))
	{
		if (!m_fileInfo)
		{
			m_nzbInfo->AddMessage(Message::mkWarning, "Malformed nzb-file, tag <segment> without tag <file>");
			return;
		}

		if (!atts)
		{
			m_nzbInfo->AddMessage(Message::mkWarning, tagAttrMessage);
			return;
		}

		int64 lsize = -1;
		int partNumber = -1;

		for (int i = 0; atts[i]; i += 2)
		{
			const char* attrname = atts[i];
			const char* attrvalue = atts[i + 1];
			if (!strcmp("bytes", attrname))
			{
				lsize = atol(attrvalue);
			}
			if (!strcmp("number", attrname))
			{
				partNumber = atol(attrvalue);
			}
		}

		if (partNumber > 0)
		{
			// new segment, add it!
			std::unique_ptr<ArticleInfo> article = std::make_unique<ArticleInfo>();
			article->SetPartNumber(partNumber);
			article->SetSize(lsize);
			m_article = article.get();
			AddArticle(m_fileInfo.get(), std::move(article));
		}
	}
	else if (!strcmp("meta", name))
	{
		if (!atts)
		{
			m_nzbInfo->AddMessage(Message::mkWarning, tagAttrMessage);
			return;
		}
		m_hasPassword = atts[0] && atts[1] && !strcmp("type", atts[0]) && !strcmp("password", atts[1]);
	}
}

void NzbFile::Parse_EndElement(const char *name)
{
	if (!strcmp("file", name))
	{
		// Close the file element, add the new file to file-list
		AddFileInfo(std::move(m_fileInfo));
		m_article = nullptr;
	}
	else if (!strcmp("group", name))
	{
		if (!m_fileInfo)
		{
			// error: bad nzb-file
			return;
		}

		m_fileInfo->GetGroups()->push_back(*m_tagContent);
		m_tagContent.Clear();
	}
	else if (!strcmp("segment", name))
	{
		if (!m_fileInfo || !m_article)
		{
			// error: bad nzb-file
			return;
		}

		// Get the #text part
		BString<1024> id("<%s>", *m_tagContent);
		m_article->SetMessageId(id);
		m_article = nullptr;
	}
	else if (!strcmp("meta", name) && m_hasPassword)
	{
		m_password = m_tagContent;
	}
}

void NzbFile::Parse_Content(const char *buf, int len)
{
	m_tagContent.Append(buf, len);
}

void NzbFile::SAX_StartElement(NzbFile* file, const char *name, const char **atts)
{
	file->Parse_StartElement(name, atts);
}

void NzbFile::SAX_EndElement(NzbFile* file, const char *name)
{
	file->Parse_EndElement(name);
}

void NzbFile::SAX_characters(NzbFile* file, const char * xmlstr, int len)
{
	char* str = (char*)xmlstr;

	// trim starting blanks
	int off = 0;
	for (int i = 0; i < len; i++)
	{
		char ch = str[i];
		if (ch == ' ' || ch == 10 || ch == 13 || ch == 9)
		{
			off++;
		}
		else
		{
			break;
		}
	}

	int newlen = len - off;

	// trim ending blanks
	for (int i = len - 1; i >= off; i--)
	{
		char ch = str[i];
		if (ch == ' ' || ch == 10 || ch == 13 || ch == 9)
		{
			newlen--;
		}
		else
		{
			break;
		}
	}

	if (newlen > 0)
	{
		// interpret tag content
		file->Parse_Content(str + off, newlen);
	}
}

void* NzbFile::SAX_getEntity(NzbFile* file, const char * name)
{
	xmlEntityPtr e = xmlGetPredefinedEntity((xmlChar* )name);

	if (!e)
	{
		file->m_nzbInfo->AddMessage(Message::mkWarning, "entity not found");
		file->m_ignoreNextError = true;
	}

	return e;
}

void NzbFile::SAX_error(NzbFile* file, const char *msg, ...)
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

	file->m_nzbInfo->AddMessage(Message::mkError, BString<1024>("Error parsing nzb-file: %s", errMsg));
}
