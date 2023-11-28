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

namespace Extension
{
	struct Kind
	{
		bool post = false;
		bool scan = false;
		bool queue = false;
		bool scheduler = false;
		bool feed = false;
	};

	class Script
	{
	public:
		Script() = default;
		Script(Script&&) noexcept = default;
		Script(const char* name, const char* location);
		void SetName(const char* name);
		const char* GetName() const;
		void SetLocation(const char* location);
		const char* GetLocation() const;
		void SetDisplayName(const char* displayName);
		const char* GetDisplayName() const;
		void SetDescription(const char* displayName);
		const char* GetDescription() const;
		void SetKind(Kind&& kind);
		bool GetPostScript() const;
		bool GetScanScript() const;
		bool GetQueueScript() const;
		bool GetSchedulerScript() const;
		bool GetFeedScript() const;
		void SetQueueEvents(const char* queueEvents);
		const char* GetQueueEvents() const;
		void SetTaskTime(const char* taskTime);
		const char* GetTaskTime() const;

	private:
		Kind kind;
		CString m_name;
		CString m_location;
		CString m_displayName;
		CString m_description;
		CString m_queueEvents;
		CString m_taskTime;
	};
}

#endif
