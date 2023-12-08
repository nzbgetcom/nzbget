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
		json["ScanScript"] = script.GetScanScript();
		json["QueueScript"] = script.GetQueueScript();
		json["SchedulerScript"] = script.GetSchedulerScript();
		json["FeedScript"] = script.GetFeedScript();
		json["QueueEvents"] = script.GetQueueEvents();
		json["TaskTime"] = script.GetTaskTime();

		for (const auto& option : script.GetOptions())
		{
			Json::JsonObject optionJson;
			Json::JsonArray selectJson;

			optionJson["Type"] = option.type;
			optionJson["Name"] = option.name;
			optionJson["DisplayName"] = option.displayName;
			optionJson["Description"] = option.description;
			optionJson["Value"] = option.value;

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

	std::string ToXml(const Script& script)
	{
		xmlNodePtr scriptNode = xmlNewNode(NULL, BAD_CAST "value><struct");

		xmlNodePtr memberNode = xmlNewNode(NULL, BAD_CAST "member");
		xmlNodePtr valueNode = xmlNewNode(NULL, BAD_CAST "value");
		xmlNewChild(memberNode, NULL, BAD_CAST "name", BAD_CAST "Name");
		xmlNewChild(valueNode, NULL, BAD_CAST "string", BAD_CAST script.GetName());
		xmlAddChild(scriptNode, memberNode);
		xmlAddChild(scriptNode, valueNode);
		//xmlNewChild(scriptNode, NULL, BAD_CAST "member><name>Author</name><value><string", BAD_CAST script.GetAuthor());
		////xmlNewChild(scriptNode, NULL, BAD_CAST "Main", BAD_CAST script.GetN);
		// xmlNewChild(scriptNode, NULL, BAD_CAST "PostScript", BAD_CAST script.GetPostScript());
		// xmlNewChild(scriptNode, NULL, BAD_CAST "ScanScript", BAD_CAST script.GetScanScript());
		// xmlNewChild(scriptNode, NULL, BAD_CAST "QueueScript", BAD_CAST script.GetQueueScript());
		// xmlNewChild(scriptNode, NULL, BAD_CAST "SchedulerScript", BAD_CAST script.GetSchedulerScript());
		// xmlNewChild(scriptNode, NULL, BAD_CAST "FeedScript", BAD_CAST script.GetFeedScript());
		// 
		// xmlNewChild(scriptNode, NULL, BAD_CAST "DisplayName", BAD_CAST script.GetDisplayName());
		// xmlNewChild(scriptNode, NULL, BAD_CAST "Version", BAD_CAST script.GetVersion());
		// xmlNewChild(scriptNode, NULL, BAD_CAST "License", BAD_CAST script.GetLicense());
		// xmlNewChild(scriptNode, NULL, BAD_CAST "Description", BAD_CAST script.GetDescription());
		// xmlNewChild(scriptNode, NULL, BAD_CAST "QueueEvents", BAD_CAST script.GetQueueEvents());
		// xmlNewChild(scriptNode, NULL, BAD_CAST "TaskTime", BAD_CAST script.GetTaskTime());

		// for (const ManifestFile::Option& option : script.GetOptions()) {
		// 	xmlNodePtr optionNode = xmlNewChild(scriptNode, NULL, BAD_CAST "Options", NULL);
		// 	xmlNewChild(optionNode, NULL, BAD_CAST "Type", BAD_CAST option.type.c_str());
		// 	xmlNewChild(optionNode, NULL, BAD_CAST "Name", BAD_CAST option.name.c_str());
		// 	xmlNewChild(optionNode, NULL, BAD_CAST "DisplayName", BAD_CAST option.displayName.c_str());
		// 	xmlNewChild(optionNode, NULL, BAD_CAST "Description", BAD_CAST option.description.c_str());
		// 	xmlNewChild(optionNode, NULL, BAD_CAST "Value", BAD_CAST option.value.c_str());

		// 	for (const std::string& selectOption : option.select) {
		// 		xmlNewChild(optionNode, NULL, BAD_CAST "Select", BAD_CAST selectOption.c_str());
		// 	}
		// }

		// for (const ManifestFile::Command& command : script.GetCommands()) {
		// 	xmlNodePtr commandNode = xmlNewChild(scriptNode, NULL, BAD_CAST "Commands", NULL);
		// 	xmlNewChild(commandNode, NULL, BAD_CAST "Name", BAD_CAST command.name.c_str());
		// 	xmlNewChild(commandNode, NULL, BAD_CAST "DisplayName", BAD_CAST command.displayName.c_str());
		// 	xmlNewChild(commandNode, NULL, BAD_CAST "Description", BAD_CAST command.description.c_str());
		// 	xmlNewChild(commandNode, NULL, BAD_CAST "Action", BAD_CAST command.action.c_str());
		// }

		std::string result;

		xmlBufferPtr buffer = xmlBufferCreate();
		if (buffer == nullptr) {
			xmlBufferFree(buffer);
			xmlFreeNode(scriptNode);
			return result;
		}

		int size = xmlNodeDump(buffer, scriptNode->doc, scriptNode, 0, 1);
		if (size > 0) {
			result = std::string(reinterpret_cast<const char*>(buffer->content), size);
		}

		xmlBufferFree(buffer);
		xmlFreeNode(scriptNode);
		xmlCleanupParser();

		return result;
	}
}
