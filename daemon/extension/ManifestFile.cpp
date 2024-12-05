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

#include "ManifestFile.h"
#include "Json.h"
#include "FileSystem.h"
#include "Log.h"

namespace ManifestFile
{
	bool Load(Manifest& manifest, const char* directory)
	{
		std::string path = std::string(directory) + PATH_SEPARATOR + MANIFEST_FILE;
		DiskFile file;
		if (!file.Open(path.c_str(), DiskFile::omRead)) return false;

		std::string jsonStr;
		char buffer[BUFFER_SIZE];
		while (file.ReadLine(buffer, BUFFER_SIZE))
		{
			jsonStr += buffer;
		}
		
		auto desRes = Json::Deserialize(jsonStr);
		if (!desRes.has_value())
		{
			error("Failed to parse %s. Syntax error.", path.c_str());
			return false;
		}

		Json::JsonValue jsonValue = std::move(desRes.value());
		Json::JsonObject json = jsonValue.as_object();

		if (!ValidateAndSet(json, manifest))
			return false;

		CheckKeyAndSet(json, "nzbgetMinVersion", manifest.nzbgetMinVersion);

		return true;
	}

	bool ValidateAndSet(const Json::JsonObject& json, Manifest& manifest)
	{
		if (!CheckKeyAndSet(json, "author", manifest.author))
			return false;

		if (!CheckKeyAndSet(json, "main", manifest.main))
			return false;

		if (!CheckKeyAndSet(json, "homepage", manifest.homepage))
			return false;

		if (!CheckKeyAndSet(json, "about", manifest.about))
			return false;

		if (!CheckKeyAndSet(json, "version", manifest.version))
			return false;

		if (!CheckKeyAndSet(json, "name", manifest.name))
			return false;

		if (!CheckKeyAndSet(json, "displayName", manifest.displayName))
			return false;

		if (!CheckKeyAndSet(json, "kind", manifest.kind))
			return false;

		if (!CheckKeyAndSet(json, "license", manifest.license))
			return false;

		if (!CheckKeyAndSet(json, "taskTime", manifest.taskTime))
			return false;

		if (!CheckKeyAndSet(json, "queueEvents", manifest.queueEvents))
			return false;

		if (!ValidateOptionsAndSet(json, manifest.options))
			return false;

		if (!ValidateCommandsAndSet(json, manifest.commands))
			return false;

		ValidateSectionsAndSet(json, manifest.options, manifest.commands);

		if (!ValidateTxtAndSet(json, manifest.description, "description"))
			return false;

		if (!ValidateTxtAndSet(json, manifest.requirements, "requirements"))
			return false;

		return true;
	}

	bool ValidateCommandsAndSet(const Json::JsonObject& json, std::vector<Command>& commands)
	{
		auto rawCommands = json.if_contains("commands");
		if (!rawCommands || !rawCommands->is_array())
			return false;

		for (auto& value : rawCommands->as_array())
		{
			Json::JsonObject cmdJson = value.as_object();
			Command command{};

			CheckKeyAndSet(cmdJson, "section", command.section.name, DEFAULT_SECTION_NAME);

			if (!CheckKeyAndSet(cmdJson, "name", command.name))
				continue;

			if (!CheckKeyAndSet(cmdJson, "displayName", command.displayName))
				continue;

			if (!ValidateTxtAndSet(cmdJson, command.description, "description"))
				continue;

			if (!CheckKeyAndSet(cmdJson, "action", command.action))
				continue;

			commands.emplace_back(std::move(command));
		}

		commands.shrink_to_fit();

		return true;
	}

	bool ValidateOptionsAndSet(const Json::JsonObject& json, std::vector<Option>& options)
	{
		auto rawOptions = json.if_contains("options");
		if (!rawOptions || !rawOptions->is_array())
			return false;

		for (auto& optionVal : rawOptions->as_array())
		{
			Json::JsonObject optionJson = optionVal.as_object();
			auto selectJson = optionJson.if_contains("select");
			if (!selectJson || !selectJson->is_array())
				continue;

			Option option{};

			CheckKeyAndSet(optionJson, "section", option.section.name, DEFAULT_SECTION_NAME);

			if (!CheckKeyAndSet(optionJson, "name", option.name))
				continue;

			if (!CheckKeyAndSet(optionJson, "displayName", option.displayName))
				continue;

			if (!CheckKeyAndSet(optionJson, "value", option.value))
				continue;

			if (!ValidateTxtAndSet(optionJson, option.description, "description"))
				continue;

			for (auto& selectVal : selectJson->as_array())
			{
				if (selectVal.is_string())
				{
					option.select.emplace_back(selectVal.get_string().c_str());
					continue;
				}
				if (selectVal.is_number())
				{
					option.select.emplace_back(selectVal.to_number<double>());
					continue;
				}
			}

			options.emplace_back(std::move(option));
		}

		options.shrink_to_fit();

		return true;
	}

	bool ValidateTxtAndSet(const Json::JsonObject& json, std::vector<std::string>& property, const char* propName)
	{
		auto rawProp = json.if_contains(propName);
		if (!rawProp || !rawProp->is_array())
			return false;

		for (auto& value : rawProp->as_array())
		{
			if (value.is_string())
			{
				property.emplace_back(value.get_string().c_str());
			}
		}

		property.shrink_to_fit();

		return true;
	}

	bool CheckKeyAndSet(const Json::JsonObject& json, const char* key, std::string& property)
	{
		const auto& rawProperty = json.if_contains(key);
		if (!rawProperty || !rawProperty->is_string())
			return false;

		property = rawProperty->get_string().c_str();
		return true;
	}

	bool CheckKeyAndSet(const Json::JsonObject& json, const char* key, std::string& property, std::string defValue)
	{
		const auto& rawProperty = json.if_contains(key);
		if (!rawProperty || !rawProperty->is_string())
		{
			property = std::move(defValue);
			return false;
		}

		property = rawProperty->get_string().c_str();
		return true;
	}

	bool CheckKeyAndSet(const Json::JsonObject& json, const char* key, SelectOption& property)
	{
		const auto& rawProperty = json.if_contains(key);
		if (!rawProperty)
			return false;

		if (rawProperty->is_string())
		{
			property = rawProperty->get_string().c_str();
			return true;
		}

		if (rawProperty->is_number())
		{
			property = rawProperty->to_number<double>();
			return true;
		}

		return false;
	}

	bool CheckKeyAndSet(const Json::JsonObject& json, const char* key, bool& property)
	{
		const auto& rawProperty = json.if_contains(key);
		if (rawProperty && rawProperty->is_bool())
		{
			property = rawProperty->as_bool();
			return true;
		}

		return false;
	}

	bool ValidateSectionsAndSet(const Json::JsonObject& json, std::vector<Option>& options, std::vector<Command>& commands)
	{
		auto rawSections = json.if_contains("sections");
		if (!rawSections || !rawSections->is_array())
			return false;

		for (auto& sectionVal : rawSections->as_array())
		{
			Json::JsonObject sectionJson = sectionVal.as_object();

			Section section;

			if (!CheckKeyAndSet(sectionJson, "name", section.name))
				continue;

			if (Util::StrCaseCmp(section.name, DEFAULT_SECTION_NAME))
				continue;

			if (!CheckKeyAndSet(sectionJson, "prefix", section.prefix))
				continue;

			if (!CheckKeyAndSet(sectionJson, "multi", section.multi))
				continue;

			for (auto& option : options)
			{
				if (option.section.name == section.name)
				{
					option.section = section;
				}
			}

			for (auto& command : commands)
			{
				if (command.section.name == section.name)
				{
					command.section = section;
				}
			}
		}

		return true;
	}
}
