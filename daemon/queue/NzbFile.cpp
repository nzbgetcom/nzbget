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

NzbFile::NzbFile(const char* fileName, const char* category) 
	: m_fileName{ fileName ? fileName : "" }
	, m_ignoreNextError(false)
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

ArticleInfo* NzbFile::AddArticle(FileInfo* fileInfo, std::unique_ptr<ArticleInfo> articleInfo)
{
	// kept in file order and put in part-number order when the file ends: a
	// list indexed by the part number took the memory a huge number asks for
	fileInfo->GetArticles()->push_back(std::move(articleInfo));
	return fileInfo->GetArticles()->back().get();
}

int NzbFile::DeclaredParts(const char* subject)
{
	// the part count posters put at the very end of the subject: "(1/N)"
	if (Util::EmptyStr(subject))
	{
		return 0;
	}
	const char* end = subject + strlen(subject);
	while (end > subject && isspace((unsigned char)end[-1])) end--;
	if (end == subject || end[-1] != ')')
	{
		return 0;
	}
	const char* p = end - 1;
	int64 total = 0;
	int64 scale = 1;
	while (p > subject && isdigit((unsigned char)p[-1]) && scale <= 1000000)
	{
		p--;
		total += (*p - '0') * scale;
		scale *= 10;
	}
	if (scale == 1 || p == subject || p[-1] != '/')
	{
		return 0;
	}
	p--;
	const char* digits = p;
	while (p > subject && isdigit((unsigned char)p[-1])) p--;
	if (p == digits || p == subject || p[-1] != '(')
	{
		return 0;
	}
	// a bound against nonsense, not a limit for real postings
	return total <= 1000000 ? (int)total : 0;
}

void NzbFile::AddFileInfo(std::unique_ptr<FileInfo> fileInfo)
{
	// calculate file size and delete empty articles

	// part-number order; of two segments with one number the later counts
	ArticleList* articles = fileInfo->GetArticles();
	std::stable_sort(articles->begin(), articles->end(),
		[](const std::unique_ptr<ArticleInfo>& a, const std::unique_ptr<ArticleInfo>& b)
		{
			return a->GetPartNumber() < b->GetPartNumber();
		});
	for (size_t i = 0; i + 1 < articles->size(); i++)
	{
		if ((*articles)[i]->GetPartNumber() == (*articles)[i + 1]->GetPartNumber())
		{
			(*articles)[i].reset();
		}
	}
	articles->erase(std::remove(articles->begin(), articles->end(), nullptr), articles->end());

	if (articles->empty())
	{
		return;
	}

	// every missing number counts with the size of the first segment present;
	// so do the parts past the last one listed that the subject declares
	// ("... yEnc (1/3709)" with segments 1..1510 only): without them a file
	// the nzb lists only partly looked complete until it was downloaded
	int totalArticles = std::max(articles->back()->GetPartNumber(),
		DeclaredParts(fileInfo->GetSubject()));
	int missedArticles = totalArticles - (int)articles->size();
	int64 oneSize = articles->front()->GetSize();
	int64 size = 0;
	for (std::unique_ptr<ArticleInfo>& article : *articles)
	{
		size += article->GetSize();
	}
	int64 missedSize = missedArticles * oneSize;
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
	for (FileList::iterator it = m_nzbInfo->GetFileList()->begin(); it != m_nzbInfo->GetFileList()->end(); ++it)
	{
		FileInfo* fileInfo1 = (*it).get();
		int dupe = 1;
		for (FileList::iterator it2 = it + 1; it2 != m_nzbInfo->GetFileList()->end(); ++it2)
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

	m_metaName = FileSystem::SanitizePathSegment(m_metaName);
	m_metaTitle = FileSystem::SanitizePathSegment(m_metaTitle);

	if (m_metaName.empty() && !m_metaTitle.empty())
	{
		m_metaName = m_metaTitle;
	}

	// Sanitize control characters (\r, \n, \t, etc.) to prevent
	// line desynchronization in the line-based DiskState file format.
	Util::SanitizeLine(m_category);

	if (!m_metaName.empty() && m_nzbInfo)
	{
		m_nzbInfo->SetMetaName(m_metaName);
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

	m_currentElement = name;
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
			m_article = AddArticle(m_fileInfo.get(), std::move(article));
		}
	}
	else if (!strcmp("meta", name))
	{
		m_hasPassword = false;
		m_hasCategory = false;
		m_hasName = false;
		m_hasTitle = false;

		if (!atts)
		{
			m_nzbInfo->AddMessage(Message::mkWarning, tagAttrMessage);
			return;
		}

		for (int i = 0; atts[i] && atts[i + 1]; i += 2)
		{
			if (!strcasecmp("type", atts[i]))
			{
				if (!strcasecmp("password", atts[i + 1])) m_hasPassword = true;
				else if (!strcasecmp("category", atts[i + 1])) m_hasCategory = true;
				else if (!strcasecmp("name", atts[i + 1])) m_hasName = true;
				else if (!strcasecmp("title", atts[i + 1])) m_hasTitle = true;
			}
		}
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
	else if (!strcmp("meta", name) && m_hasCategory)
	{
		m_category = m_tagContent;
	}
	else if (!strcmp("meta", name) && m_hasName)
	{
		m_metaName = m_tagContent;
	}
	else if (!strcmp("meta", name) && m_hasTitle)
	{
		m_metaTitle = m_tagContent;
	}

	m_currentElement.clear();
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
	if (len <= 0)
		return;

	std::string str(xmlstr, len);
	if (file->m_currentElement == "meta" && file->m_hasCategory)
	{
		// Do not break existing users' filters that rely on this normalization
		Util::Trim(str);
	}

	if (!str.empty())
	{
		file->Parse_Content(str.data(), str.length());
	}
}

void* NzbFile::SAX_getEntity(NzbFile* file, const char * name)
{
	xmlEntityPtr e = xmlGetPredefinedEntity(reinterpret_cast<const xmlChar*>(name));

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
