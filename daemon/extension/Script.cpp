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
#include "Script.h"
#include "NString.h"

Script::Script(const char *name, const char *location)
	: m_name(name), m_location(location), m_displayName(name){};

const char *Script::GetName() const { return m_name; }
const char *Script::GetLocation() const { return m_location; }
void Script::SetDisplayName(const char *displayName) { m_displayName = displayName; }
const char *Script::GetDisplayName() const { return m_displayName; }
bool Script::GetPostScript() const { return m_postScript; }
void Script::SetPostScript(bool postScript) { m_postScript = postScript; }
bool Script::GetScanScript() const { return m_scanScript; }
void Script::SetScanScript(bool scanScript) { m_scanScript = scanScript; }
bool Script::GetQueueScript() const { return m_queueScript; }
void Script::SetQueueScript(bool queueScript) { m_queueScript = queueScript; }
bool Script::GetSchedulerScript() const { return m_schedulerScript; }
void Script::SetSchedulerScript(bool schedulerScript) { m_schedulerScript = schedulerScript; }
bool Script::GetFeedScript() const { return m_feedScript; }
void Script::SetFeedScript(bool feedScript) { m_feedScript = feedScript; }
void Script::SetQueueEvents(const char *queueEvents) { m_queueEvents = queueEvents; }
const char *Script::GetQueueEvents() const { return m_queueEvents; }
void Script::SetTaskTime(const char *taskTime) { m_taskTime = taskTime; }
const char *Script::GetTaskTime() const { return m_taskTime; }
