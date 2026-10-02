/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2014-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2024-2026 Denis <denis@nzbget.com>
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


#ifndef ARTICLEWRITER_H
#define ARTICLEWRITER_H

#include <atomic>
#include <optional>
#include <string>
#include "NString.h"
#include "DownloadInfo.h"
#include "Decoder.h"
#include "FileSystem.h"

class CachedSegmentData final : public SegmentData
{
public:
	CachedSegmentData() {}
	CachedSegmentData(char* data, int size) : m_data(data), m_size(size) {}
	CachedSegmentData(const CachedSegmentData&) = delete;
	CachedSegmentData(CachedSegmentData&& other) :
		m_data(other.m_data), m_size(other.m_size) { other.m_data = nullptr; other.m_size = 0; }
	CachedSegmentData& operator=(CachedSegmentData&& other);
	~CachedSegmentData() override;
	char* GetData() override { return m_data; }

private:
	char* m_data = nullptr;
	int m_size = 0;

	friend class ArticleCache;
};

class ArticleWriter
{
public:
	struct OutputPaths 
	{
	    std::string finalPath;
	    std::string tempPath;
	};
	void SetInfoName(const char* infoName) { m_infoName = infoName ? infoName : ""; }
	void SetInfoName(std::string infoName) { m_infoName = std::move(infoName); }
	void SetFileInfo(FileInfo* fileInfo) { m_fileInfo = fileInfo; }
	void SetArticleInfo(ArticleInfo* articleInfo) { m_articleInfo = articleInfo; }
	void Prepare();
	bool Start(Decoder::EFormat format, const char* filename, int64 fileSize, int64 articleOffset, int articleSize);
	bool Write(char* buffer, int len);
	void Finish(bool success);
	bool GetDuplicate() { return m_duplicate; }
	void LogStartMessage(std::string_view infoFilename, bool directWrite, bool cached);
	std::optional<OutputPaths> SetupOutputFile(DiskFile &outfile,
											std::string_view destDir,
											std::string_view filename,
											std::string_view infoFilename,
											bool directWrite,
											bool cached);
	uint32 ProcessArticles(DiskFile& outfile,
						std::string_view infoFilename,
						std::string_view finalOutputPath,
						bool directWrite,
						bool cached);
	bool CommitDiskFile(DiskFile& outfile,
						std::string_view tempDestPath,
						std::string_view finalOutputPath,
						bool directWrite);
	bool CleanupOldData(bool directWrite,
						std::string_view nzbDestDir,
						std::string_view finalOutputPath);
	void ReportCompletionStatus(std::string_view infoFilename);
	void HandlePostProcessing(uint32 crc, 
							std::string_view currentPath, 
							std::string_view originalFilename, 
							std::string_view nzbDestDir);
	bool CompleteFileParts();
	static bool MoveCompletedFiles(NzbInfo* nzbInfo, const char* oldDestDir);
	void FlushCache();

private:
	bool IsDupeFallbackArticle() const;
	bool GetSkipDiskWrite();

	FileInfo* m_fileInfo;
	ArticleInfo* m_articleInfo;
	DiskFile m_outFile;
	std::string m_tempFilename;
	std::string m_outputFilename;
	std::string m_resultFilename;
	std::string m_infoName;
	Decoder::EFormat m_format = Decoder::efUnknown;
	CachedSegmentData m_articleData;
	int64 m_articleOffset;
	int m_articleSize;
	int m_articlePtr;
	bool m_duplicate = false;
	bool m_commitError = false;

	bool CreateOutputFile(int64 size);
	void BuildOutputFilename();
	void SetWriteBuffer(DiskFile& outFile, int recSize);
};

class ArticleCache final : public Thread
{
public:
	class FlushGuard
	{
	public:
		FlushGuard(FlushGuard&& other) = default;
		~FlushGuard();
	private:
		Guard m_guard;
		FlushGuard(Mutex& mutex);
		friend class ArticleCache;
	};

	void Run() override;
	void Stop() override;
	CachedSegmentData Alloc(int size);
	bool Realloc(CachedSegmentData* segment, int newSize);
	void Free(CachedSegmentData* segment);
	FlushGuard GuardFlush() { return FlushGuard(m_flushMutex); }
	Guard GuardContent() { return Guard(m_contentMutex); }
	bool GetFlushing() { return m_flushing; }
	size_t GetAllocated() { return m_allocated; }
	bool FileBusy(FileInfo* fileInfo) { return fileInfo == m_fileInfo; }

private:
	std::atomic<size_t> m_allocated{0};
	FileInfo* m_fileInfo = nullptr;
	std::mutex m_allocMutex;
	std::condition_variable m_allocCond;
	Mutex m_flushMutex;
	Mutex m_contentMutex;
	bool m_flushing = false;

	bool CheckFlush(bool flushEverything);
};

extern ArticleCache* g_ArticleCache;

#endif
