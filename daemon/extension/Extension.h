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
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef SCRIPT_H
#define SCRIPT_H

#include "ManifestFile.h"

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
		void SetAuthor(const char* author);
		const char* GetAuthor() const;
		void SetVersion(const char* version);
		const char* GetVersion() const;
		void SetLicense(const char* license);
		const char* GetLicense() const;
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
		void SetOptions(std::vector<ManifestFile::Option>&& options);
		const std::vector<ManifestFile::Option>& GetOptions() const;
		void SetCommands(std::vector<ManifestFile::Command>&& commands);
		const std::vector<ManifestFile::Command>& GetCommands() const;

	private:
		Kind m_kind;
		std::string m_author;
		std::string m_version;
		std::string m_license;
		std::string m_name;
		std::string m_location;
		std::string m_displayName;
		std::string m_description;
		std::string m_queueEvents;
		std::string m_taskTime;
		std::vector<ManifestFile::Option> m_options;
		std::vector<ManifestFile::Command> m_commands;
	};

	std::string ToJson(const Script& script);
}

#endif
