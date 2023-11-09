/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2023 Denis <denis@nzbget.com>
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

#ifndef SCRIPT_H
#define SCRIPT_H

#include "NString.h"

class Script
{
public:
	Script(const char *name, const char *location);;
	Script(Script &&) = default;
	const char *GetName() const;
	const char *GetLocation() const;
	void SetDisplayName(const char *displayName);
	const char *GetDisplayName() const;
	bool GetPostScript() const;
	void SetPostScript(bool postScript);
	bool GetScanScript() const;
	void SetScanScript(bool scanScript);
	bool GetQueueScript() const;
	void SetQueueScript(bool queueScript);
	bool GetSchedulerScript() const;
	void SetSchedulerScript(bool schedulerScript);
	bool GetFeedScript() const;
	void SetFeedScript(bool feedScript);
	void SetQueueEvents(const char *queueEvents);
	const char *GetQueueEvents() const;
	void SetTaskTime(const char *taskTime);
	const char *GetTaskTime() const;

private:
	CString m_name;
	CString m_location;
	CString m_displayName;
	bool m_postScript = false;
	bool m_scanScript = false;
	bool m_queueScript = false;
	bool m_schedulerScript = false;
	bool m_feedScript = false;
	CString m_queueEvents;
	CString m_taskTime;
};

#endif
