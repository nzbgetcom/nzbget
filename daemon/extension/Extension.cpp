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

#include "nzbget.h"
#include "Extension.h"
#include "NString.h"

namespace Extension
{
	Script::Script(const char* name, const char* location)
		: m_name(name)
		, m_location(location)
		, m_displayName(name) {};

	void Script::SetName(const char* name) { m_name = name; };
	const char* Script::GetName() const { return m_name; }
	void Script::SetLocation(const char* location) { m_location = location; }
	const char* Script::GetLocation() const { return m_location; }
	void Script::SetDisplayName(const char* displayName) { m_displayName = displayName; }
	const char* Script::GetDisplayName() const { return m_displayName; }
	void Script::SetDescription(const char* description) { m_description = description; };
	const char* Script::GetDescription() const { return m_description; };
	void Script::SetKind(Kind&& kind_) { kind = std::move(kind_); };
	bool Script::GetPostScript() const { return kind.post; }
	bool Script::GetScanScript() const { return kind.scan; }
	bool Script::GetQueueScript() const { return kind.queue; }
	bool Script::GetSchedulerScript() const { return kind.scheduler; }
	bool Script::GetFeedScript() const { return kind.feed; }
	void Script::SetQueueEvents(const char* queueEvents) { kind.queue = queueEvents; }
	const char* Script::GetQueueEvents() const { return m_queueEvents; }
	void Script::SetTaskTime(const char* taskTime) { m_taskTime = taskTime; }
	const char* Script::GetTaskTime() const { return m_taskTime; }
}
