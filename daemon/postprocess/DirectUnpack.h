/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef DIRECTUNPACK_H
#define DIRECTUNPACK_H

#include <atomic>
#include "Log.h"
#include "Thread.h"
#include "DownloadInfo.h"
#include "ScriptController.h"

class DirectUnpack final : public Thread, public ScriptController
{
public:
	virtual void Run();
	static void StartJob(NzbInfo* nzbInfo);
	void Stop(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	void FileDownloaded(DownloadQueue* downloadQueue, FileInfo* fileInfo);
	void NzbDownloaded(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	void NzbDeleted(DownloadQueue* downloadQueue, NzbInfo* nzbInfo);
	static bool IsArchiveFilename(const char* filename);

protected:
	virtual bool ReadLine(char* buf, int bufSize, FILE* stream);
	virtual void AddMessage(Message::EKind kind, const char* text);

private:
	class ParamList : public std::vector<CString>
	{
	public:
		bool Exists(const char* param) { return std::find(begin(), end(), param) != end(); }
	};

	/**
	 *  @brief Checks if Direct unpack can proceed.
	 *
	 *  @return True if direct unpack is permitted (either the file is not an
 	 *          archive or the archive contains no failed articles), false otherwise.
	 */
	bool CanProceed(FileInfo* fileInfo) const;

	typedef std::deque<CString> ArchiveList;

	int m_nzbId;
	CString m_name;
	CString m_infoName;
	CString m_infoNameUp;
	CString m_destDir;
	CString m_finalDir;
	CString m_unpackDir;
	CString m_unpackExtendedDir;
	CString m_password;
	CString m_waitingFile;
	CString m_progressLabel;
	std::atomic<bool> m_nzbCompleted{false};
	Mutex m_volumeMutex;
	ArchiveList m_archives;
	time_t m_extraStartTime = 0;
	ArchiveList m_extractedArchives;
	bool m_allOkMessageReceived = false;
	bool m_unpackOk = false;
	bool m_finalDirCreated = false;
	bool m_processed = false;
	bool m_unpacking = false;
	void CreateUnpackDir();
	void FindArchiveFiles();
	void ExecuteUnrar(const char* archiveName);
	bool PrepareCmdParams(const char* command, ParamList* params, const char* infoName);
	void WaitNextVolume(const char* filename);
	void Cleanup();
	bool IsMainArchive(const char* filename);
	void SetProgressLabel(NzbInfo* nzbInfo, const char* progressLabel);
	void AddExtraTime(NzbInfo* nzbInfo);
};

#endif
