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

namespace Extension
{
	Script::Script(const char* name_, const char* location_)
		: name(name_)
		, location(location_)
		, displayName(name_) {};
	void Script::SetAuthor(const char* author_) { author = author_; }
	const char* Script::GetAuthor() const { return author.c_str(); }
	void Script::SetVersion(const char* version_) { version = version_; }
	const char* Script::GetVersion() const { return version.c_str(); }
	void Script::SetLicense(const char* license_) { license = license_; }
	const char* Script::GetLicense() const { return license.c_str(); }
	void Script::SetName(const char* name_) { name = name_; }
	const char* Script::GetName() const { return name.c_str(); }
	void Script::SetLocation(const char* location_) { location = location_; }
	const char* Script::GetLocation() const { return location.c_str(); }
	void Script::SetDisplayName(const char* displayName_) { displayName = displayName_; }
	const char* Script::GetDisplayName() const { return displayName.c_str(); }
	void Script::SetDescription(const char* description_) { description = description_; };
	const char* Script::GetDescription() const { return description.c_str(); }
	void Script::SetKind(Kind&& kind_) { kind = std::move(kind_); };
	bool Script::GetPostScript() const { return kind.post; }
	bool Script::GetScanScript() const { return kind.scan; }
	bool Script::GetQueueScript() const { return kind.queue; }
	bool Script::GetSchedulerScript() const { return kind.scheduler; }
	bool Script::GetFeedScript() const { return kind.feed; }
	void Script::SetQueueEvents(const char* queueEvents) { kind.queue = queueEvents; }
	const char* Script::GetQueueEvents() const { return queueEvents.c_str(); }
	void Script::SetTaskTime(const char* taskTime_) { taskTime = taskTime_; }
	const char* Script::GetTaskTime() const { return taskTime.c_str(); }
	void Script::SetOptions(std::vector<ManifestFile::Option>&& options_) { options = std::move(options_); }
	const std::vector<ManifestFile::Option>& Script::GetOptions() const { return options; }
	void Script::SetCommands(std::vector<ManifestFile::Command>&& commands_) { commands = std::move(commands_); }
	const std::vector<ManifestFile::Command>& Script::GetCommands() const { return commands; }
}
