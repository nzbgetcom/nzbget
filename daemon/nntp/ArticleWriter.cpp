/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2014-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include <sstream>
#include <iomanip>
#include "ArticleWriter.h"
#include "DiskState.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"

CachedSegmentData::~CachedSegmentData()
{
	g_ArticleCache->Free(this);
}

CachedSegmentData& CachedSegmentData::operator=(CachedSegmentData&& other)
{
	g_ArticleCache->Free(this);
	m_data = other.m_data;
	m_size = other.m_size;
	other.m_data = nullptr;
	other.m_size = 0;
	return *this;
}


void ArticleWriter::SetWriteBuffer(DiskFile& outFile, int recSize)
{
	if (g_Options->GetWriteBuffer() > 0)
	{
		outFile.SetWriteBuffer(recSize > 0 && recSize < g_Options->GetWriteBuffer() * 1024 ?
			recSize : g_Options->GetWriteBuffer() * 1024);
	}
}

void ArticleWriter::Prepare()
{
	BuildOutputFilename();
	m_resultFilename = m_articleInfo->GetResultFilename()
		? m_articleInfo->GetResultFilename()
		: "";
}

bool ArticleWriter::Start(Decoder::EFormat format, const char* filename, int64 fileSize,
	int64 articleOffset, int articleSize)
{
	m_outFile.Close();
	m_format = format;
	m_articleOffset = articleOffset;
	m_articleSize = articleSize ? articleSize : m_articleInfo->GetSize();
	m_articlePtr = 0;

	// prepare file for writing
	if (m_format == Decoder::efYenc)
	{
		if (g_Options->GetDupeCheck() &&
			m_fileInfo->GetNzbInfo()->GetDupeMode() != dmForce &&
			!m_fileInfo->GetNzbInfo()->GetManyDupeFiles())
		{
			bool outputInitialized;
			{
				Guard guard = m_fileInfo->GuardOutputFile();
				outputInitialized = m_fileInfo->GetOutputInitialized();
				if (!g_Options->GetDirectWrite())
				{
					m_fileInfo->SetOutputInitialized(true);
				}
			}

			if (!outputInitialized && filename &&
				FileSystem::FileExists(BString<1024>("%s%c%s", m_fileInfo->GetNzbInfo()->GetDestDir(), PATH_SEPARATOR, filename)))
			{
				m_duplicate = true;
				return false;
			}
		}

		if (g_Options->GetDirectWrite())
		{
			Guard guard = m_fileInfo->GuardOutputFile();
			if (!m_fileInfo->GetOutputInitialized())
			{
				if (!CreateOutputFile(fileSize))
				{
					return false;
				}
				m_fileInfo->SetOutputInitialized(true);
			}
		}
	}

	// allocate cache buffer
	if (g_Options->GetArticleCache() > 0 && !g_Options->GetRawArticle() &&
		(!g_Options->GetDirectWrite() || m_format == Decoder::efYenc))
	{
		m_articleData = g_ArticleCache->Alloc(m_articleSize);

		while (!m_articleData.GetData() && g_ArticleCache->GetFlushing())
		{
			Util::Sleep(5);
			m_articleData = g_ArticleCache->Alloc(m_articleSize);
		}

		if (!m_articleData.GetData())
		{
			detail("Article cache is full, using disk for %s", m_infoName.c_str());
		}
	}

	if (!m_articleData.GetData())
	{
		bool directWrite = (g_Options->GetDirectWrite() || m_fileInfo->GetForceDirectWrite()) && m_format == Decoder::efYenc;
		const char* outFilename = directWrite ? m_outputFilename.c_str() : m_tempFilename.c_str();
		if (!m_outFile.Open(outFilename, directWrite ? DiskFile::omReadWrite : DiskFile::omWrite))
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not %s file %s: %s", directWrite ? "open" : "create", outFilename,
				*FileSystem::GetLastErrorMessage());
			return false;
		}
		SetWriteBuffer(m_outFile, m_articleInfo->GetSize());

		if (directWrite)
		{
			m_outFile.Seek(m_articleOffset);
		}
	}

	return true;
}

bool ArticleWriter::GetSkipDiskWrite()
{
	if (m_fileInfo && m_fileInfo->GetNzbInfo() && m_fileInfo->GetNzbInfo()->GetSkipDiskWrite())
	{
		return true;
	}

	return g_Options->GetSkipWrite();
}

bool ArticleWriter::Write(char* buffer, int len)
{
	if (!g_Options->GetRawArticle())
	{
		m_articlePtr += len;
	}

	if (m_articlePtr > m_articleSize)
	{
		// An attempt to write beyond article border is detected.
		// That's an error condition (damaged article).
		// We return 'false' since this isn't a fatal disk error and
		// article size mismatch will be detected in decoder check anyway.
		return true;
	}

	if (!g_Options->GetRawArticle() && m_articleData.GetData())
	{
		memcpy(m_articleData.GetData() + m_articlePtr - len, buffer, len);
		return true;
	}

	if (GetSkipDiskWrite())
	{
		return true;
	}

	return m_outFile.Write(buffer, len) > 0;
}

void ArticleWriter::Finish(bool success)
{
	m_outFile.Close();

	if (!success)
	{
		FileSystem::DeleteFile(m_tempFilename.c_str());
		FileSystem::DeleteFile(m_resultFilename.c_str());
		return;
	}

	bool directWrite = (g_Options->GetDirectWrite() || m_fileInfo->GetForceDirectWrite()) && m_format == Decoder::efYenc;

	if (!g_Options->GetRawArticle())
	{
		if (!directWrite && !m_articleData.GetData())
		{
			if (!FileSystem::MoveFile(m_tempFilename.c_str(), m_resultFilename.c_str()))
			{
				m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
					"Could not rename file %s to %s: %s", m_tempFilename.c_str(), m_resultFilename.c_str(),
					*FileSystem::GetLastErrorMessage());
			}
			FileSystem::DeleteFile(m_tempFilename.c_str());
		}

		if (m_articleData.GetData())
		{
			if (m_articleSize != m_articlePtr)
			{
				g_ArticleCache->Realloc(&m_articleData, m_articlePtr);
			}
			Guard contentGuard = g_ArticleCache->GuardContent();
			m_articleInfo->AttachSegment(std::make_unique<CachedSegmentData>(std::move(m_articleData)), m_articleOffset, m_articlePtr);
			m_fileInfo->SetCachedArticles(m_fileInfo->GetCachedArticles() + 1);
		}
		else
		{
			m_articleInfo->SetSegmentOffset(m_articleOffset);
			m_articleInfo->SetSegmentSize(m_articlePtr);
		}
	}
	else
	{
		// rawmode
		if (!FileSystem::MoveFile(m_tempFilename.c_str(), m_resultFilename.c_str()))
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not move file %s to %s: %s", m_tempFilename.c_str(), m_resultFilename.c_str(),
				*FileSystem::GetLastErrorMessage());
		}
	}
}

/* creates output file and subdirectores */
bool ArticleWriter::CreateOutputFile(int64 size)
{
	if (FileSystem::FileExists(m_outputFilename.c_str()))
	{
		if (FileSystem::FileSize(m_outputFilename.c_str()) == size)
		{
			// keep existing old file from previous program session
			return true;
		}
		// delete existing old file from previous program session
		FileSystem::DeleteFile(m_outputFilename.c_str());
	}

	// ensure the directory exist
	BString<1024> destDir;
	destDir.Set(m_outputFilename.c_str(), (int)(FileSystem::BaseFileName(m_outputFilename.c_str()) - m_outputFilename.c_str()));
	CString errmsg;

	if (!FileSystem::ForceDirectories(destDir, errmsg))
	{
		m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
			"Could not create directory %s: %s", *destDir, *errmsg);
		return false;
	}

	if (!FileSystem::AllocateFile(m_outputFilename.c_str(), size, g_Options->GetArticleCache() == 0, errmsg))
	{
		m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
			"Could not create file %s: %s", m_outputFilename.c_str(), *errmsg);
		return false;
	}

	return true;
}

void ArticleWriter::BuildOutputFilename()
{
	if (!m_fileInfo || !m_articleInfo)
		return;

	const char* tmpDir = g_Options->GetTempDir();
	const char* destDir = m_fileInfo->GetNzbInfo()->GetDestDir();
	if (Util::EmptyStr(tmpDir) || Util::EmptyStr(destDir))
		return;

	std::stringstream ss;
	ss << tmpDir;
	ss << PATH_SEPARATOR;
	ss << m_fileInfo->GetId();
	ss << ".";
	ss << std::setfill('0') << std::setw(3) << m_articleInfo->GetPartNumber();

	std::string filename = ss.str();

	m_articleInfo->SetResultFilename(filename.c_str());
	m_tempFilename = filename + ".tmp";
	
	if (g_Options->GetDirectWrite() || m_fileInfo->GetForceDirectWrite())
	{
		Guard guard = m_fileInfo->GuardOutputFile();
		const std::string& outputFilename = m_fileInfo->GetOutputFilename();

		if (!outputFilename.empty())
		{
			filename = outputFilename;
		}
		else
		{
			ss.str("");
			ss.clear();

			ss << destDir;
			ss << PATH_SEPARATOR;
			ss << m_fileInfo->GetId();
			ss << ".out.tmp";

			filename = ss.str();
			m_fileInfo->SetOutputFilename(filename);
		}

		m_outputFilename = std::move(filename);
	}
 }

void ArticleWriter::CompleteFileParts()
{
	debug("Completing file parts");
	debug("ArticleFilename: %s", m_fileInfo->GetFilename());

	bool directWrite = (g_Options->GetDirectWrite() || m_fileInfo->GetForceDirectWrite()) && m_fileInfo->GetOutputInitialized();

	std::string nzbDestDir;
	std::string nzbName;
	std::string destDir;
	std::string filename;

	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();
		nzbName = m_fileInfo->GetNzbInfo()->GetName();
		nzbDestDir = m_fileInfo->GetNzbInfo()->GetDestDir();
		m_outputFilename = m_fileInfo->GetOutputFilename();
		if (m_outputFilename.empty())
		{
			destDir = nzbDestDir;
			filename = m_fileInfo->GetFilename();
		}
		else
		{
			auto [destDir_, filename_] = FileSystem::SplitPathAndFilename(m_outputFilename);
			destDir.swap(destDir_);
			filename.swap(filename_);
		}
	}

	std::string infoFilename = nzbName + PATH_SEPARATOR + m_fileInfo->GetFilename();

	bool cached = m_fileInfo->GetCachedArticles() > 0;

	if (g_Options->GetRawArticle())
	{
		detail("Moving articles for %s", infoFilename.c_str());
	}
	else if (directWrite && cached)
	{
		detail("Writing articles for %s", infoFilename.c_str());
	}
	else if (directWrite)
	{
		detail("Checking articles for %s", infoFilename.c_str());
	}
	else
	{
		detail("Joining articles for %s", infoFilename.c_str());
	}

	// Ensure the DstDir is created
	CString errmsg;
	if (!FileSystem::ForceDirectories(destDir.c_str(), errmsg))
	{
		m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
			"Could not create directory %s: %s", destDir.c_str(), *errmsg);
		return;
	}

	std::string ofn;
	if (m_fileInfo->GetForceDirectWrite())
	{
		ofn = destDir + PATH_SEPARATOR + infoFilename;
	}
	else
	{
		ofn = FileSystem::MakeUniqueFilename(destDir.c_str(), filename.c_str()).Str();
	}

	DiskFile outfile;
	std::string tmpdestfile = ofn + ".tmp";

	if (!g_Options->GetRawArticle() && !directWrite)
	{
		FileSystem::DeleteFile(tmpdestfile.c_str());
		if (!outfile.Open(tmpdestfile.c_str(), DiskFile::omWrite))
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not create file %s: %s",
				tmpdestfile.c_str(),
				*FileSystem::GetLastErrorMessage()
			);
			return;
		}
	}
	else if (directWrite && cached)
	{
		if (!outfile.Open(m_outputFilename.c_str(), DiskFile::omReadWrite))
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not open file %s: %s",
				m_outputFilename.c_str(),
				*FileSystem::GetLastErrorMessage()
			);
			return;
		}
		tmpdestfile = m_outputFilename;
	}
	else if (g_Options->GetRawArticle())
	{
		FileSystem::DeleteFile(tmpdestfile.c_str());
		if (!FileSystem::CreateDirectory(ofn.c_str()))
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not create directory %s: %s",
				ofn.c_str(),
				*FileSystem::GetLastErrorMessage()
			);
			return;
		}
	}

	if (outfile.Active())
	{
		SetWriteBuffer(outfile, 0);
	}

	uint32 crc = 0;

	{
		std::unique_ptr<ArticleCache::FlushGuard> flushGuard;
		if (cached)
		{
			flushGuard = std::make_unique<ArticleCache::FlushGuard>(g_ArticleCache->GuardFlush());
		}

		CharBuffer buffer;
		bool firstArticle = true;

		if (!g_Options->GetRawArticle() && !directWrite)
		{
			buffer.Reserve(1024 * 64);
		}

		for (ArticleInfo* pa : m_fileInfo->GetArticles())
		{
			if (pa->GetStatus() != ArticleInfo::aiFinished)
			{
				continue;
			}

			if (!g_Options->GetRawArticle() && !directWrite && pa->GetSegmentOffset() > -1 &&
				pa->GetSegmentOffset() > outfile.Position() && outfile.Position() > -1)
			{
				memset(buffer, 0, buffer.Size());
				if (!GetSkipDiskWrite())
				{
					while (pa->GetSegmentOffset() > outfile.Position() && outfile.Position() > -1 &&
						outfile.Write(buffer, std::min((int)(pa->GetSegmentOffset() - outfile.Position()), buffer.Size())));
				}
			}

			if (pa->GetSegmentContent())
			{
				if (!GetSkipDiskWrite())
				{
					outfile.Seek(pa->GetSegmentOffset());
					outfile.Write(pa->GetSegmentContent(), pa->GetSegmentSize());
				}
				pa->DiscardSegment();
			}
			else if (!g_Options->GetRawArticle() && !directWrite && !GetSkipDiskWrite())
			{
				DiskFile infile;
				if (pa->GetResultFilename() && infile.Open(pa->GetResultFilename(), DiskFile::omRead))
				{
					int cnt = buffer.Size();
					while (cnt == buffer.Size())
					{
						cnt = (int)infile.Read(buffer, buffer.Size());
						outfile.Write(buffer, cnt);
					}
					infile.Close();
				}
				else
				{
					m_fileInfo->SetFailedArticles(m_fileInfo->GetFailedArticles() + 1);
					m_fileInfo->SetSuccessArticles(m_fileInfo->GetSuccessArticles() - 1);
					m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
						"Could not find file %s for %s [%i/%i]",
						pa->GetResultFilename(), infoFilename.c_str(), pa->GetPartNumber(),
						(int)m_fileInfo->GetArticles()->size());
				}
			}
			else if (g_Options->GetRawArticle())
			{
				BString<1024> dstFileName("%s%c%03i", ofn.c_str(), PATH_SEPARATOR, pa->GetPartNumber());
				if (!FileSystem::MoveFile(pa->GetResultFilename(), dstFileName))
				{
					m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
						"Could not move file %s to %s: %s", pa->GetResultFilename(),
						*dstFileName, *FileSystem::GetLastErrorMessage());
				}
			}

			if (m_format == Decoder::efYenc)
			{
				crc = firstArticle ? pa->GetCrc() : Crc32::Combine(crc, pa->GetCrc(), pa->GetSegmentSize());
				firstArticle = false;
			}
		}

		buffer.Clear();
	}

	if (outfile.Active())
	{
		outfile.Close();
		if (!directWrite && !FileSystem::MoveFile(tmpdestfile.c_str(), ofn.c_str()))
		{
			m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
				"Could not move file %s to %s: %s",
				tmpdestfile.c_str(),
				ofn.c_str(),
				*FileSystem::GetLastErrorMessage()
			);
		}
	}

	if (!directWrite)
	{
		for (ArticleInfo* pa : m_fileInfo->GetArticles())
		{
			if (pa->GetResultFilename())
			{
				FileSystem::DeleteFile(pa->GetResultFilename());
			}
		}
	}

	if (m_fileInfo->GetTotalArticles() == m_fileInfo->GetSuccessArticles())
	{
		m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkInfo, "Successfully downloaded %s", infoFilename.c_str());
	}
	else if (m_fileInfo->GetMissedArticles() + m_fileInfo->GetFailedArticles() > 0)
	{
		m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkWarning,
			"%i of %i article downloads failed for \"%s\"",
			m_fileInfo->GetMissedArticles() + m_fileInfo->GetFailedArticles(),
			m_fileInfo->GetTotalArticles(),
			infoFilename.c_str()
		);
	}
	else
	{
		m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkInfo, "Partially downloaded %s", infoFilename.c_str());
	}

	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();
		m_fileInfo->SetCrc(crc);

		if (m_fileInfo->IsHardLinked() && Util::EmptyStr(g_Options->GetInterDir()))
		{
			// No need to rename, remove hardlink of .out.tmp-file
			if (FileSystem::DeleteFile(m_outputFilename.c_str()))
			{
				m_fileInfo->SetOutputFilename(nullptr);
				return;
			}
			else
			{
				m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError, "Cannot remove hardlink");
			}
		}

		RenameOutputFile(filename, destDir);
	}
}

void ArticleWriter::RenameOutputFile(const std::string& filename, const std::string& destDir)
{
	if (filename == m_fileInfo->GetFilename())
		return;

	std::string ofn = FileSystem::MakeUniqueFilename(destDir.c_str(), m_fileInfo->GetFilename()).Str();
	if (!FileSystem::MoveFile(m_outputFilename.c_str(), ofn.c_str()))
	{
		m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
			"Could not rename file %s to %s: %s",
			m_outputFilename.c_str(), ofn.c_str(), *FileSystem::GetLastErrorMessage()
		);
	}
	m_fileInfo->SetOutputFilename(std::move(ofn));
}

void ArticleWriter::FlushCache()
{
	detail("Flushing cache for %s", m_infoName.c_str());

	bool directWrite = g_Options->GetDirectWrite() && m_fileInfo->GetOutputInitialized();
	DiskFile outfile;
	bool needBufFile = false;
	int flushedArticles = 0;
	int64 flushedSize = 0;

	{
		ArticleCache::FlushGuard flushGuard = g_ArticleCache->GuardFlush();

		std::vector<ArticleInfo*> cachedArticles;

		{
			Guard contentGuard = g_ArticleCache->GuardContent();

			if (m_fileInfo->GetFlushLocked())
			{
				return;
			}

			m_fileInfo->SetFlushLocked(true);

			cachedArticles.reserve(m_fileInfo->GetArticles()->size());
			for (ArticleInfo* pa : m_fileInfo->GetArticles())
			{
				if (pa->GetSegmentContent())
				{
					cachedArticles.push_back(pa);
				}
			}
		}

		for (ArticleInfo* pa : cachedArticles)
		{
			if (m_fileInfo->GetDeleted() && !m_fileInfo->GetNzbInfo()->GetParking())
			{
				// the file was deleted during flushing: stop flushing immediately
				break;
			}

			if (directWrite && !outfile.Active())
			{
				const std::string& outputFilename = m_fileInfo->GetOutputFilename();
				if (!outfile.Open(outputFilename.c_str(), DiskFile::omReadWrite))
				{
					m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
						"Could not open file %s: %s", outputFilename.c_str(),
						*FileSystem::GetLastErrorMessage());
					// prevent multiple error messages
					pa->DiscardSegment();
					flushedArticles++;
					break;
				}
				needBufFile = true;
			}

			BString<1024> destFile;

			if (!directWrite)
			{
				destFile.Format("%s.tmp", pa->GetResultFilename());
				if (!outfile.Open(destFile, DiskFile::omWrite))
				{
					m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
						"Could not create file %s: %s", *destFile,
						*FileSystem::GetLastErrorMessage());
					// prevent multiple error messages
					pa->DiscardSegment();
					flushedArticles++;
					break;
				}
				needBufFile = true;
			}

			if (outfile.Active() && needBufFile)
			{
				SetWriteBuffer(outfile, 0);
				needBufFile = false;
			}

			if (directWrite)
			{
				outfile.Seek(pa->GetSegmentOffset());
			}

			if (!GetSkipDiskWrite())
			{
				outfile.Write(pa->GetSegmentContent(), pa->GetSegmentSize());
			}

			flushedSize += pa->GetSegmentSize();
			flushedArticles++;

			pa->DiscardSegment();

			if (!directWrite)
			{
				outfile.Close();

				if (!FileSystem::MoveFile(destFile, pa->GetResultFilename()))
				{
					m_fileInfo->GetNzbInfo()->PrintMessage(Message::mkError,
						"Could not rename file %s to %s: %s", *destFile, pa->GetResultFilename(),
						*FileSystem::GetLastErrorMessage());
				}
			}
		}

		outfile.Close();

		{
			Guard contentGuard = g_ArticleCache->GuardContent();
			m_fileInfo->SetCachedArticles(m_fileInfo->GetCachedArticles() - flushedArticles);
			m_fileInfo->SetFlushLocked(false);
		}
	}

	detail("Saved %i articles (%.2f MB) from cache into disk for %s", flushedArticles,
		(float)(flushedSize / 1024.0 / 1024.0), m_infoName.c_str());
}

bool ArticleWriter::MoveCompletedFiles(NzbInfo* nzbInfo, const char* oldDestDir)
{
	if (nzbInfo->GetCompletedFiles()->empty())
	{
		return true;
	}

	// Ensure the DstDir is created
	CString errmsg;
	if (!FileSystem::ForceDirectories(nzbInfo->GetDestDir(), errmsg))
	{
		nzbInfo->PrintMessage(Message::mkError, "Could not create directory %s: %s", nzbInfo->GetDestDir(), *errmsg);
		return false;
	}

	// move already downloaded files to new destination
	for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
	{
		BString<1024> oldFileName("%s%c%s", oldDestDir, PATH_SEPARATOR, completedFile.GetFilename());
		BString<1024> newFileName("%s%c%s", nzbInfo->GetDestDir(), PATH_SEPARATOR, completedFile.GetFilename());

		// check if file was not moved already
		if (strcmp(oldFileName, newFileName))
		{
			// prevent overwriting of existing files
			newFileName = FileSystem::MakeUniqueFilename(nzbInfo->GetDestDir(), completedFile.GetFilename());

			detail("Moving file %s to %s", *oldFileName, *newFileName);
			if (!FileSystem::MoveFile(oldFileName, newFileName))
			{
				nzbInfo->PrintMessage(Message::mkError, "Could not move file %s to %s: %s",
					*oldFileName, *newFileName, *FileSystem::GetLastErrorMessage());
			}
		}
	}

	// delete old directory (if empty)
	if (FileSystem::DirEmpty(oldDestDir))
	{
		// check if there are pending writes into directory
		bool pendingWrites = false;
		for (FileInfo* fileInfo : nzbInfo->GetFileList())
		{
			if (!pendingWrites)
			{
				break;
			}

			const std::string& outputFilename = fileInfo->GetOutputFilename();
			if (fileInfo->GetActiveDownloads() > 0)
			{
				Guard guard = fileInfo->GuardOutputFile();
				pendingWrites = fileInfo->GetOutputInitialized() && !outputFilename.empty();
			}
			else
			{
				pendingWrites = fileInfo->GetOutputInitialized() && !outputFilename.empty();
			}
		}

		if (!pendingWrites)
		{
			FileSystem::RemoveDirectory(oldDestDir);
		}
	}

	return true;
}


CachedSegmentData ArticleCache::Alloc(int size)
{
	Guard guard(m_allocMutex);

	void* p = nullptr;

	if (m_allocated + size <= (size_t)g_Options->GetArticleCache() * 1024 * 1024)
	{
		p = malloc(size);
		if (p)
		{
			if (!m_allocated && g_Options->GetServerMode() && g_Options->GetContinuePartial())
			{
				g_DiskState->WriteCacheFlag();
			}
			if (!m_allocated)
			{
				// Resume Run(), the notification arrives later, after releasing m_allocMutex
				m_allocCond.NotifyAll();
			}
			m_allocated += size;
		}
	}

	return CachedSegmentData((char*)p, p ? size : 0);
}

bool ArticleCache::Realloc(CachedSegmentData* segment, int newSize)
{
	Guard guard(m_allocMutex);

	void* p = realloc(segment->m_data, newSize);
	if (p)
	{
		m_allocated += newSize - segment->m_size;
		segment->m_size = newSize;
		segment->m_data = (char*)p;
	}

	return p;
}

void ArticleCache::Free(CachedSegmentData* segment)
{
	if (segment->m_size)
	{
		free(segment->m_data);

		Guard guard(m_allocMutex);
		m_allocated -= segment->m_size;
		if (!m_allocated && g_Options->GetServerMode() && g_Options->GetContinuePartial())
		{
			g_DiskState->DeleteCacheFlag();
		}
	}
}

void ArticleCache::Run()
{
	// automatically flush the cache if it is filled to 90% (only in DirectWrite mode)
	size_t fillThreshold = (size_t)g_Options->GetArticleCache() * 1024 * 1024 / 100 * 90;

	int resetCounter = 0;
	bool justFlushed = false;
	while (!IsStopped() || m_allocated > 0)
	{
		if ((justFlushed || resetCounter >= 1000 || IsStopped() ||
			(g_Options->GetDirectWrite() && m_allocated >= fillThreshold)) &&
			m_allocated > 0)
		{
			justFlushed = CheckFlush(m_allocated >= fillThreshold);
			resetCounter = 0;
		}
		else if (!m_allocated)
		{
			Guard guard(m_allocMutex);
			m_allocCond.Wait(m_allocMutex, [&] { return IsStopped() || m_allocated > 0; });
			resetCounter = 0;
		}
		else
		{
			Util::Sleep(5);
			resetCounter += 5;
		}
	}
}

void ArticleCache::Stop()
{
	Thread::Stop();

	// Resume Run() to exit it
	Guard guard(m_allocMutex);
	m_allocCond.NotifyAll();
}

bool ArticleCache::CheckFlush(bool flushEverything)
{
	debug("Checking cache, Allocated: %i, FlushEverything: %i", (int)m_allocated, (int)flushEverything);

	BString<1024> infoName;

	{
		GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
		for (NzbInfo* nzbInfo : downloadQueue->GetQueue())
		{
			if (m_fileInfo)
			{
				break;
			}

			for (FileInfo* fileInfo : nzbInfo->GetFileList())
			{
				if (fileInfo->GetCachedArticles() > 0 && (fileInfo->GetActiveDownloads() == 0 || flushEverything))
				{
					m_fileInfo = fileInfo;
					infoName.Format("%s%c%s", m_fileInfo->GetNzbInfo()->GetName(), PATH_SEPARATOR, m_fileInfo->GetFilename());
					break;
				}
			}
		}
	}

	if (m_fileInfo)
	{
		ArticleWriter articleWriter;
		articleWriter.SetFileInfo(m_fileInfo);
		articleWriter.SetInfoName(infoName);
		articleWriter.FlushCache();
		m_fileInfo = nullptr;
		return true;
	}

	debug("Checking cache... nothing to flush");

	return false;
}

ArticleCache::FlushGuard::FlushGuard(Mutex& mutex) : m_guard(&mutex)
{
	g_ArticleCache->m_flushing = true;
}

ArticleCache::FlushGuard::~FlushGuard()
{
	if (m_guard)
	{
		g_ArticleCache->m_flushing = false;
	}
}
