/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2012-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef URLCOORDINATOR_H
#define URLCOORDINATOR_H

#include "NString.h"
#include "Log.h"
#include "Thread.h"
#include "WebDownloader.h"
#include "DownloadInfo.h"
#include "Observer.h"

class UrlDownloader;

class UrlCoordinator : public Thread, public Observer, public Debuggable
{
public:
	UrlCoordinator();
	~UrlCoordinator() override;
	void Run() override;
	void Stop() override;
	void Update(Subject* caller, void* aspect) override;

	// Editing the queue
	void AddUrlToQueue(std::unique_ptr<NzbInfo> nzbInfo, bool addFirst);
	bool HasMoreJobs() const { return m_hasMoreJobs; }
	bool DeleteQueueEntry(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, bool avoidHistory);

protected:
	void LogDebugInfo() override;

private:
	using ActiveDownloads = std::list<UrlDownloader*>;

	class DownloadQueueObserver final : public Observer
	{
	public:
		UrlCoordinator* m_owner;
		void Update(Subject* caller, void* aspect) override { m_owner->DownloadQueueUpdate(caller, aspect); }
	};

	ActiveDownloads m_activeDownloads;
	std::atomic<bool> m_hasMoreJobs{true};
	std::mutex m_waitMutex;
	std::condition_variable m_waitCond;
	DownloadQueueObserver m_downloadQueueObserver;

	NzbInfo* GetNextUrl(DownloadQueue* downloadQueue);
	void StartUrlDownload(NzbInfo* nzbInfo);
	void UrlCompleted(UrlDownloader* urlDownloader);
	void ResetHangingDownloads();
	void WaitJobs();
	void DownloadQueueUpdate(Subject* caller, void* aspect);
};

extern UrlCoordinator* g_UrlCoordinator;

class UrlDownloader : public WebDownloader
{
public:
	void SetNzbInfo(NzbInfo* nzbInfo) { m_nzbInfo = nzbInfo; }
	NzbInfo* GetNzbInfo() { return m_nzbInfo; }
	const char* GetCategory() const { return m_category; }

protected:
	void ProcessHeader(const char* line) override;

private:
	NzbInfo* m_nzbInfo;
	CString m_category;
};

#endif
