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

#include "nzbget.h"

#include "Extension.h"

namespace Extension
{
	Script::Script(const char* name, const char* location)
		: m_name(name)
		, m_location(location)
		, m_displayName(name) {};
	void Script::SetAuthor(const char* author) { m_author = author; }
	const char* Script::GetAuthor() const { return m_author.c_str(); }
	void Script::SetVersion(const char* version) { m_version = version; }
	const char* Script::GetVersion() const { return m_version.c_str(); }
	void Script::SetLicense(const char* license) { m_license = license; }
	const char* Script::GetLicense() const { return m_license.c_str(); }
	void Script::SetName(const char* name) { m_name = name; }
	const char* Script::GetName() const { return m_name.c_str(); }
	void Script::SetLocation(const char* location) { m_location = location; }
	const char* Script::GetLocation() const { return m_location.c_str(); }
	void Script::SetDisplayName(const char* displayName) { m_displayName = displayName; }
	const char* Script::GetDisplayName() const { return m_displayName.c_str(); }
	void Script::SetDescription(const char* description) { m_description = description; };
	const char* Script::GetDescription() const { return m_description.c_str(); }
	void Script::SetKind(Kind&& kind) { m_kind = std::move(kind); };
	bool Script::GetPostScript() const { return m_kind.post; }
	bool Script::GetScanScript() const { return m_kind.scan; }
	bool Script::GetQueueScript() const { return m_kind.queue; }
	bool Script::GetSchedulerScript() const { return m_kind.scheduler; }
	bool Script::GetFeedScript() const { return m_kind.feed; }
	void Script::SetQueueEvents(const char* queueEvents) { m_queueEvents = queueEvents; }
	const char* Script::GetQueueEvents() const { return m_queueEvents.c_str(); }
	void Script::SetTaskTime(const char* taskTime) { m_taskTime = taskTime; }
	const char* Script::GetTaskTime() const { return m_taskTime.c_str(); }
	void Script::SetOptions(std::vector<ManifestFile::Option>&& options) { m_options = std::move(options); }
	const std::vector<ManifestFile::Option>& Script::GetOptions() const { return m_options; }
	void Script::SetCommands(std::vector<ManifestFile::Command>&& commands) { m_commands = std::move(commands); }
	const std::vector<ManifestFile::Command>& Script::GetCommands() const { return m_commands; }

	std::string ToJson(const Script& script)
	{
		Json::JsonObject json;
		Json::JsonArray optionsJson;
		Json::JsonArray commandsJson;

		json["Location"] = script.GetLocation();
		json["Name"] = script.GetName();
		json["DisplayName"] = script.GetDisplayName();
		json["Description"] = script.GetDescription();
		json["Author"] = script.GetAuthor();
		json["License"] = script.GetLicense();
		json["Version"] = script.GetVersion();
		json["PostScript"] = script.GetPostScript();
		json["QueueScript"] = script.GetQueueScript();
		json["SchedulerScript"] = script.GetSchedulerScript();
		json["FeedScript"] = script.GetFeedScript();
		json["QueueEvents"] = script.GetQueueEvents();
		json["TaskTime"] = script.GetTaskTime();

		for (const auto& option : script.GetOptions())
		{
			Json::JsonObject optionJson;
			Json::JsonArray selectJson;

			optionJson["Name"] = option.name;
			optionJson["DisplayName"] = option.displayName;
			optionJson["Description"] = option.description;

			for (const auto& select : option.select)
			{
				selectJson.push_back(Json::JsonValue(select));
			}
			optionJson["Select"] = std::move(selectJson);
			optionsJson.push_back(std::move(optionJson));
		}

		for (const auto& command : script.GetCommands())
		{
			Json::JsonObject commandJson;

			commandJson["Name"] = command.name;
			commandJson["DisplayName"] = command.displayName;
			commandJson["Description"] = command.description;
			commandJson["Action"] = command.action;

			commandsJson.push_back(std::move(commandJson));
		}

		json["Options"] = std::move(optionsJson);
		json["Commands"] = std::move(commandsJson);

		return Json::Serialize(json);
	}
}
