/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2007-2017 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#ifndef QUEUESCRIPT_H
#define QUEUESCRIPT_H

#include "DownloadInfo.h"
#include "Extension.h"

class QueueScriptCoordinator
{
public:
	enum EEvent
	{
		qeFileDownloaded, // lowest priority
		qeUrlCompleted,
		qeNzbMarked,
		qeNzbAdded,
		qeNzbNamed,
		qeNzbDownloaded,
		qeNzbDeleted // highest priority
	};

	void Stop() { m_stopped = true; }
	void InitOptions();
	void EnqueueScript(NzbInfo* nzbInfo, EEvent event);
	void CheckQueue();
	bool HasJob(int nzbId, bool* active);
	int GetQueueSize();
	static NzbInfo* FindNzbInfo(DownloadQueue* downloadQueue, int nzbId);

private:
	class QueueItem
	{
	public:
		QueueItem(int nzbId, std::shared_ptr<Extension::Script> script, EEvent event) :
			m_nzbId(nzbId), m_script(std::move(script)), m_event(event) {}
		int GetNzbId() { return m_nzbId; }
		const std::shared_ptr<Extension::Script>& GetScript() const { return m_script; }
		EEvent GetEvent() { return m_event; }
	private:
		int m_nzbId;
		std::shared_ptr<Extension::Script> m_script;
		EEvent m_event;
	};

	typedef std::deque<std::unique_ptr<QueueItem>> Queue;

	Queue m_queue;
	Mutex m_queueMutex;
	std::unique_ptr<QueueItem> m_curItem;
	bool m_hasQueueScripts = false;
	bool m_stopped = false;

	bool UsableScript(std::shared_ptr<Extension::Script> script, NzbInfo* nzbInfo, EEvent event);
};

extern QueueScriptCoordinator* g_QueueScriptCoordinator;

#endif
