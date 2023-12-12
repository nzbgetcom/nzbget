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
	void Script::SetAuthor(std::string&& author)
	{
		m_author = std::move(author);
	}

	const char* Script::GetAuthor() const
	{
		return m_author.c_str();
	}

	void Script::SetVersion(std::string&& version)
	{
		m_version = std::move(version);
	}

	const char* Script::GetVersion() const
	{
		return m_version.c_str();
	}

	void Script::SetLicense(std::string&& license)
	{
		m_license = std::move(license);
	}

	const char* Script::GetLicense() const
	{
		return m_license.c_str();
	}

	void Script::SetName(std::string&& name)
	{
		m_name = std::move(name);
	}

	const char* Script::GetName() const
	{
		return m_name.c_str();
	}

	void Script::SetLocation(std::string&& location)
	{
		m_location = std::move(location);
	}

	const char* Script::GetLocation() const
	{
		return m_location.c_str();
	}

	void Script::SetDisplayName(std::string&& displayName)
	{
		m_displayName = std::move(displayName);
	}

	const char* Script::GetDisplayName() const
	{
		return m_displayName.c_str();
	}

	void Script::SetCaption(std::string&& caption)
	{
		m_caption = std::move(caption);
	}

	const char* Script::GetCaption() const
	{
		return m_caption.c_str();
	}

	void Script::SetDescription(std::string&& description)
	{
		m_description = std::move(description);
	};

	const char* Script::GetDescription() const
	{
		return m_description.c_str();
	}

	void Script::SetKind(Kind&& kind)
	{
		m_kind = std::move(kind);
	};

	bool Script::GetPostScript() const
	{
		return m_kind.post;
	}

	bool Script::GetScanScript() const
	{
		return m_kind.scan;
	}

	bool Script::GetQueueScript() const
	{
		return m_kind.queue;
	}
	bool Script::GetSchedulerScript() const
	{
		return m_kind.scheduler;
	}
	bool Script::GetFeedScript() const
	{
		return m_kind.feed;
	}
	void Script::SetQueueEvents(std::string&& queueEvents)
	{
		m_queueEvents = std::move(queueEvents);
	}

	const char* Script::GetQueueEvents() const
	{
		return m_queueEvents.c_str();
	}


	void Script::SetTaskTime(std::string&& taskTime)
	{
		m_taskTime = std::move(taskTime);
	}

	const char* Script::GetTaskTime() const
	{
		return m_taskTime.c_str();
	}

	void Script::SetOptions(std::vector<ManifestFile::Option>&& options)
	{
		m_options = std::move(options);
	}

	const std::vector<ManifestFile::Option>& Script::GetOptions() const
	{
		return m_options;
	}

	void Script::SetCommands(std::vector<ManifestFile::Command>&& commands)
	{
		m_commands = std::move(commands);
	}

	const std::vector<ManifestFile::Command>& Script::GetCommands() const
	{
		return m_commands;
	}

	std::string ToJsonStr(const Script& script)
	{
		Json::JsonObject json;
		Json::JsonArray optionsJson;
		Json::JsonArray commandsJson;

		json["Location"] = script.GetLocation();
		json["Name"] = script.GetName();
		json["DisplayName"] = script.GetDisplayName();
		json["Caption"] = script.GetCaption();
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

	std::string ToXmlStr(const Script& script)
	{
		xmlNodePtr rootNode = xmlNewNode(NULL, BAD_CAST "value");
		xmlNodePtr structNode = xmlNewNode(NULL, BAD_CAST "struct");

		AddNewNode(structNode, "Name", "string", script.GetName());
		AddNewNode(structNode, "DisplayName", "string", script.GetDisplayName());
		AddNewNode(structNode, "Caption", "string", script.GetCaption());
		AddNewNode(structNode, "Description", "string", script.GetDescription());
		AddNewNode(structNode, "Author", "string", script.GetAuthor());
		AddNewNode(structNode, "License", "string", script.GetLicense());
		AddNewNode(structNode, "Version", "string", script.GetVersion());

		AddNewNode(structNode, "PostScript", "boolean", BooToStr(script.GetPostScript()));
		AddNewNode(structNode, "ScanScript", "boolean", BooToStr(script.GetScanScript()));
		AddNewNode(structNode, "QueueScript", "boolean", BooToStr(script.GetQueueScript()));
		AddNewNode(structNode, "SchedulerScript", "boolean", BooToStr(script.GetSchedulerScript()));
		AddNewNode(structNode, "FeedScript", "boolean", BooToStr(script.GetFeedScript()));

		AddNewNode(structNode, "QueueEvents", "string", script.GetQueueEvents());
		AddNewNode(structNode, "TaskTime", "string", script.GetTaskTime());

		xmlNodePtr commandsNode = xmlNewNode(NULL, BAD_CAST "Commands");
		for (const ManifestFile::Command& command : script.GetCommands()) {
			AddNewNode(commandsNode, "Name", "string", command.name.c_str());
			AddNewNode(commandsNode, "DisplayName", "string", command.displayName.c_str());
			AddNewNode(commandsNode, "Description", "string", command.description.c_str());
			AddNewNode(commandsNode, "Action", "string", command.action.c_str());
		}

		xmlNodePtr optionsNode = xmlNewNode(NULL, BAD_CAST "Options");
		for (const ManifestFile::Option& option : script.GetOptions()) {
			AddNewNode(optionsNode, "Type", "string", option.type.c_str());
			AddNewNode(optionsNode, "Name", "string", option.name.c_str());
			AddNewNode(optionsNode, "DisplayName", "string", option.displayName.c_str());
			AddNewNode(optionsNode, "Value", "string", option.value.c_str());

			xmlNodePtr selectNode = xmlNewNode(NULL, BAD_CAST "Select");
			for (const std::string& selectOption : option.select) {
				AddNewNode(selectNode, "Value", "string", selectOption.c_str());
			}

			xmlAddChild(optionsNode, selectNode);
		}

		xmlAddChild(structNode, commandsNode);
		xmlAddChild(structNode, optionsNode);
		xmlAddChild(rootNode, structNode);

		std::string result;

		xmlBufferPtr buffer = xmlBufferCreate();
		if (buffer == nullptr) {
			XmlCleanup(buffer, rootNode);
			return result;
		}

		int size = xmlNodeDump(buffer, rootNode->doc, rootNode, 0, 0);
		if (size > 0) {
			result = std::string(reinterpret_cast<const char*>(buffer->content), size);
		}

		XmlCleanup(buffer, rootNode);

		return result;
	}

	void AddNewNode(xmlNodePtr rootNode, const char* name, const char* type, const char* value)
	{
		xmlNodePtr memberNode = xmlNewNode(NULL, BAD_CAST "member");
		xmlNodePtr valueNode = xmlNewNode(NULL, BAD_CAST "value");
		xmlNewChild(memberNode, NULL, BAD_CAST "name", BAD_CAST name);
		xmlNewChild(valueNode, NULL, BAD_CAST type, BAD_CAST value);
		xmlAddChild(memberNode, valueNode);
		xmlAddChild(rootNode, memberNode);
	}

	const char* BooToStr(bool value)
	{
		return value ? "true" : "false";
	}

	void XmlCleanup(xmlBufferPtr buffer, xmlNodePtr rootNode)
	{
		xmlBufferFree(buffer);
		xmlFreeNode(rootNode);
		xmlCleanupParser();
	}
}
