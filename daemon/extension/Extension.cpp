/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2023-2024 Denis <denis@nzbget.com>
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

#include "Xml.h"
#include "Extension.h"

namespace Extension
{
	void Script::SetEntry(std::string entry)
	{
		m_entry = std::move(entry);
	}

	const char* Script::GetEntry() const
	{
		return m_entry.c_str();
	}

	void Script::SetLocation(std::string location)
	{
		m_location = std::move(location);
	}

	void Script::SetRootDir(std::string dir)
	{
		m_rootDir = std::move(dir);
	}

	const char* Script::GetRootDir() const
	{
		return m_rootDir.c_str();
	}

	const char* Script::GetLocation() const
	{
		return m_location.c_str();
	}

	void Script::SetAuthor(std::string author)
	{
		m_author = std::move(author);
	}

	const char* Script::GetAuthor() const
	{
		return m_author.c_str();
	}

	void Script::SetHomepage(std::string homepage)
	{
		m_homepage = std::move(homepage);
	}

	const char* Script::GetHomepage() const
	{
		return m_homepage.c_str();
	}

	void Script::SetVersion(std::string version)
	{
		m_version = std::move(version);
	}

	void Script::SetNzbgetMinVersion(std::string version)
	{
		m_nzbgetMinVersion = std::move(version);
	}

	const char* Script::GetVersion() const
	{
		return m_version.c_str();
	}

	const char* Script::GetNzbgetMinVersion() const
	{
		return m_nzbgetMinVersion.c_str();
	}

	void Script::SetLicense(std::string license)
	{
		m_license = std::move(license);
	}

	const char* Script::GetLicense() const
	{
		return m_license.c_str();
	}

	void Script::SetName(std::string name)
	{
		m_name = std::move(name);
	}

	const char* Script::GetName() const
	{
		return m_name.c_str();
	}

	void Script::SetDisplayName(std::string displayName)
	{
		m_displayName = std::move(displayName);
	}

	const char* Script::GetDisplayName() const
	{
		return m_displayName.c_str();
	}

	void Script::SetAbout(std::string about)
	{
		m_about = std::move(about);
	}

	const char* Script::GetAbout() const
	{
		return m_about.c_str();
	}

	void Script::SetDescription(std::vector<std::string> description)
	{
		m_description = std::move(description);
	}

	const std::vector<std::string>& Script::GetDescription() const
	{
		return m_description;
	}

	void Script::SetKind(Kind kind)
	{
		m_kind = std::move(kind);
	}

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
	void Script::SetQueueEvents(std::string queueEvents)
	{
		m_queueEvents = std::move(queueEvents);
	}

	const char* Script::GetQueueEvents() const
	{
		return m_queueEvents.c_str();
	}


	void Script::SetTaskTime(std::string taskTime)
	{
		m_taskTime = std::move(taskTime);
	}

	const char* Script::GetTaskTime() const
	{
		return m_taskTime.c_str();
	}

	void Script::SetOptions(std::vector<ManifestFile::Option> options)
	{
		m_options = std::move(options);
	}

	void Script::SetRequirements(std::vector<std::string> requirements)
	{
		m_requirements = std::move(requirements);
	}

	const std::vector<std::string>& Script::GetRequirements() const
	{
		return m_requirements;
	}

	const std::vector<ManifestFile::Option>& Script::GetOptions() const
	{
		return m_options;
	}

	void Script::SetCommands(std::vector<ManifestFile::Command> commands)
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
		Json::JsonArray descriptionJson;
		Json::JsonArray requirementsJson;
		Json::JsonArray optionsJson;
		Json::JsonArray commandsJson;

		json["Entry"] = script.GetEntry();
		json["Location"] = script.GetLocation();
		json["RootDir"] = script.GetRootDir();
		json["Name"] = script.GetName();
		json["DisplayName"] = script.GetDisplayName();
		json["About"] = script.GetAbout();
		json["Author"] = script.GetAuthor();
		json["Homepage"] = script.GetHomepage();
		json["License"] = script.GetLicense();
		json["Version"] = script.GetVersion();
		json["NZBGetMinVersion"] = script.GetNzbgetMinVersion();
		json["PostScript"] = script.GetPostScript();
		json["ScanScript"] = script.GetScanScript();
		json["QueueScript"] = script.GetQueueScript();
		json["SchedulerScript"] = script.GetSchedulerScript();
		json["FeedScript"] = script.GetFeedScript();
		json["QueueEvents"] = script.GetQueueEvents();
		json["TaskTime"] = script.GetTaskTime();

		for (const auto& line : script.GetDescription())
		{
			descriptionJson.push_back(Json::JsonValue(line));
		}

		for (const auto& line : script.GetRequirements())
		{
			requirementsJson.push_back(Json::JsonValue(line));
		}

		for (const auto& option : script.GetOptions())
		{
			Json::JsonObject optionJson;
			Json::JsonArray descriptionJson;
			Json::JsonArray selectJson;

			optionJson["Name"] = option.name;
			optionJson["DisplayName"] = option.displayName;
			optionJson["Section"] = option.section.name;
			optionJson["Multi"] = option.section.multi;
			optionJson["Prefix"] = option.section.prefix;

			if (const std::string* val = std::get_if<std::string>(&option.value))
			{
				optionJson["Value"] = *val;
			}
			else if (const double* val = std::get_if<double>(&option.value))
			{
				optionJson["Value"] = *val;
			}

			for (const auto& line : option.description)
			{
				descriptionJson.push_back(Json::JsonValue(line));
			}

			for (const auto& value : option.select)
			{
				if (const std::string* val = std::get_if<std::string>(&value))
				{
					selectJson.push_back(Json::JsonValue(*val));
				}
				else if (const double* val = std::get_if<double>(&value))
				{
					selectJson.push_back(Json::JsonValue(*val));
				}
			}

			optionJson["Description"] = std::move(descriptionJson);
			optionJson["Select"] = std::move(selectJson);
			optionsJson.push_back(std::move(optionJson));
		}

		for (const auto& command : script.GetCommands())
		{
			Json::JsonObject commandJson;
			Json::JsonArray descriptionJson;

			commandJson["Name"] = command.name;
			commandJson["DisplayName"] = command.displayName;
			commandJson["Action"] = command.action;
			commandJson["Section"] = command.section.name;
			commandJson["Multi"] = command.section.multi;
			commandJson["Prefix"] = command.section.prefix;

			for (const auto& line : command.description)
			{
				descriptionJson.push_back(Json::JsonValue(line));
			}

			commandJson["Description"] = std::move(descriptionJson);
			commandsJson.push_back(std::move(commandJson));
		}

		json["Description"] = std::move(descriptionJson);
		json["Requirements"] = std::move(requirementsJson);
		json["Options"] = std::move(optionsJson);
		json["Commands"] = std::move(commandsJson);

		return Json::Serialize(json);
	}

	std::string ToXmlStr(const Script& script)
	{
		xmlNodePtr rootNode = xmlNewNode(nullptr, BAD_CAST "value");
		xmlNodePtr structNode = xmlNewNode(nullptr, BAD_CAST "struct");

		Xml::AddNewNode(structNode, "Entry", "string", script.GetEntry());
		Xml::AddNewNode(structNode, "Location", "string", script.GetLocation());
		Xml::AddNewNode(structNode, "RootDir", "string", script.GetRootDir());
		Xml::AddNewNode(structNode, "Name", "string", script.GetName());
		Xml::AddNewNode(structNode, "DisplayName", "string", script.GetDisplayName());
		Xml::AddNewNode(structNode, "About", "string", script.GetAbout());
		Xml::AddNewNode(structNode, "Author", "string", script.GetAuthor());
		Xml::AddNewNode(structNode, "Homepage", "string", script.GetHomepage());
		Xml::AddNewNode(structNode, "License", "string", script.GetLicense());
		Xml::AddNewNode(structNode, "Version", "string", script.GetVersion());
		Xml::AddNewNode(structNode, "NZBGetMinVersion", "string", script.GetNzbgetMinVersion());

		Xml::AddNewNode(structNode, "PostScript", "boolean", Xml::BoolToStr(script.GetPostScript()));
		Xml::AddNewNode(structNode, "ScanScript", "boolean", Xml::BoolToStr(script.GetScanScript()));
		Xml::AddNewNode(structNode, "QueueScript", "boolean", Xml::BoolToStr(script.GetQueueScript()));
		Xml::AddNewNode(structNode, "SchedulerScript", "boolean", Xml::BoolToStr(script.GetSchedulerScript()));
		Xml::AddNewNode(structNode, "FeedScript", "boolean", Xml::BoolToStr(script.GetFeedScript()));

		Xml::AddNewNode(structNode, "QueueEvents", "string", script.GetQueueEvents());
		Xml::AddNewNode(structNode, "TaskTime", "string", script.GetTaskTime());

		xmlNodePtr descriptionNode = xmlNewNode(nullptr, BAD_CAST "Description");
		for (const std::string& line : script.GetDescription())
		{
			Xml::AddNewNode(descriptionNode, "Value", "string", line.c_str());
		}

		xmlNodePtr requirementsNode = xmlNewNode(nullptr, BAD_CAST "Requirements");
		for (const std::string& line : script.GetRequirements())
		{
			Xml::AddNewNode(requirementsNode, "Value", "string", line.c_str());
		}

		xmlNodePtr commandsNode = xmlNewNode(nullptr, BAD_CAST "Commands");
		for (const ManifestFile::Command& command : script.GetCommands())
		{
			Xml::AddNewNode(commandsNode, "Name", "string", command.name.c_str());
			Xml::AddNewNode(commandsNode, "DisplayName", "string", command.displayName.c_str());
			Xml::AddNewNode(commandsNode, "Action", "string", command.action.c_str());
			Xml::AddNewNode(commandsNode, "Multi", "boolean", Xml::BoolToStr(command.section.multi));
			Xml::AddNewNode(commandsNode, "Section", "string", command.section.name.c_str());
			Xml::AddNewNode(commandsNode, "Prefix", "string", command.section.prefix.c_str());

			xmlNodePtr descriptionNode = xmlNewNode(nullptr, BAD_CAST "Description");
			for (const std::string& line : command.description)
			{
				Xml::AddNewNode(descriptionNode, "Value", "string", line.c_str());
			}
			xmlAddChild(commandsNode, descriptionNode);
		}

		xmlNodePtr optionsNode = xmlNewNode(nullptr, BAD_CAST "Options");
		for (const ManifestFile::Option& option : script.GetOptions())
		{
			Xml::AddNewNode(optionsNode, "Name", "string", option.name.c_str());
			Xml::AddNewNode(optionsNode, "DisplayName", "string", option.displayName.c_str());
			Xml::AddNewNode(optionsNode, "Multi", "boolean", Xml::BoolToStr(option.section.multi));
			Xml::AddNewNode(optionsNode, "Section", "string", option.section.name.c_str());
			Xml::AddNewNode(optionsNode, "Prefix", "string", option.section.prefix.c_str());

			if (const std::string* val = std::get_if<std::string>(&option.value))
			{
				Xml::AddNewNode(optionsNode, "Value", "string", val->c_str());
			}
			else if (const double* val = std::get_if<double>(&option.value))
			{
				std::string valStr = std::to_string(*val);
				Xml::AddNewNode(optionsNode, "Value", "double", valStr.c_str());
			}

			xmlNodePtr selectNode = xmlNewNode(nullptr, BAD_CAST "Select");
			for (const auto& selectOption : option.select)
			{
				if (const std::string* val = std::get_if<std::string>(&selectOption))
				{
					Xml::AddNewNode(selectNode, "Value", "string", val->c_str());
				}
				else if (const double* val = std::get_if<double>(&selectOption))
				{
					std::string valStr = std::to_string(*val);
					Xml::AddNewNode(selectNode, "Value", "double", valStr.c_str());
				}
			}

			xmlNodePtr descriptionNode = xmlNewNode(nullptr, BAD_CAST "Description");
			for (const std::string& line : option.description)
			{
				Xml::AddNewNode(descriptionNode, "Value", "string", line.c_str());
			}

			xmlAddChild(optionsNode, descriptionNode);
			xmlAddChild(optionsNode, selectNode);
		}

		xmlAddChild(structNode, descriptionNode);
		xmlAddChild(structNode, requirementsNode);
		xmlAddChild(structNode, commandsNode);
		xmlAddChild(structNode, optionsNode);
		xmlAddChild(rootNode, structNode);

		std::string result = Xml::Serialize(rootNode);

		xmlFreeNode(rootNode);

		return result;
	}
}
