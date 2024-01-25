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

#ifndef EXTENSION_H
#define EXTENSION_H

#include <atomic>
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
		Script() noexcept = default;
		~Script() noexcept = default;

		Script& operator=(const Script&) = delete;
		Script(const Script&) = delete;

		Script(Script&& other) noexcept = default;
		Script& operator=(Script&& other) noexcept = default;

		void SetEntry(std::string entry);
		const char* GetEntry() const;
		void SetLocation(std::string location);
		const char* GetLocation() const;
		void SetRootDir(std::string dir);
		const char* GetRootDir() const;
		void SetAuthor(std::string author);
		const char* GetAuthor() const;
		void SetVersion(std::string version);
		const char* GetVersion() const;
		void SetLicense(std::string license);
		const char* GetLicense() const;
		void SetHomepage(std::string homepage);
		const char* GetHomepage() const;
		void SetName(std::string name);
		const char* GetName() const;
		void SetDisplayName(std::string displayName);
		const char* GetDisplayName() const;
		void SetAbout(std::string about);
		const char* GetAbout() const;
		void SetDescription(std::vector<std::string> description);
		const std::vector<std::string>& GetDescription() const;
		void SetKind(Kind kind);
		bool GetPostScript() const;
		bool GetScanScript() const;
		bool GetQueueScript() const;
		bool GetSchedulerScript() const;
		bool GetFeedScript() const;
		void SetQueueEvents(std::string queueEvents);
		const char* GetQueueEvents() const;
		void SetTaskTime(std::string taskTime);
		const char* GetTaskTime() const;
		void SetRequirements(std::vector<std::string> requirements);
		const std::vector<std::string>& GetRequirements() const;
		void SetOptions(std::vector<ManifestFile::Option> options);
		const std::vector<ManifestFile::Option>& GetOptions() const;
		void SetCommands(std::vector<ManifestFile::Command> commands);
		const std::vector<ManifestFile::Command>& GetCommands() const;

	private:
		Kind m_kind;
		std::string m_entry;
		std::string m_location;
		std::string m_rootDir;
		std::string m_author;
		std::string m_version;
		std::string m_homepage;
		std::string m_license;
		std::string m_name;
		std::string m_displayName;
		std::string m_about;
		std::string m_queueEvents;
		std::string m_taskTime;
		std::vector<std::string> m_description;
		std::vector<std::string> m_requirements;
		std::vector<ManifestFile::Option> m_options;
		std::vector<ManifestFile::Command> m_commands;
	};

	std::string ToJsonStr(const Script& script);
	std::string ToXmlStr(const Script& script);

	namespace
	{
		void AddNewNode(xmlNodePtr rootNode, const char* name, const char* type, const char* value);
		const char* BoolToStr(bool value);
	}
}

#endif
