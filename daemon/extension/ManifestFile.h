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

#ifndef MANIFEST_FILE_H
#define MANIFEST_FILE_H

#include <string>
#include <vector>
#include <variant>
#include "Json.h"
#include "Util.h"

namespace ManifestFile
{
	using SelectOption = std::variant<double, std::string>;

	extern const char* MANIFEST_FILE;
	extern const char* DEFAULT_SECTION_NAME;

	struct Section
	{
		bool multi;
		std::string name;
		std::string prefix;
	};

	struct Option
	{
		Section section;
		std::string name;
		std::string displayName;
		std::vector<std::string> description;
		SelectOption value;
		std::vector<SelectOption> select;
	};

	struct Command
	{
		Section section;
		std::string name;
		std::string displayName;
		std::string action;
		std::vector<std::string> description;
	};

	struct Manifest
	{
		std::string author;
		std::string main;
		std::string kind;
		std::string name;
		std::string homepage;
		std::string displayName;
		std::string version;
		std::string nzbgetMinVersion;
		std::string license;
		std::string about;
		std::string queueEvents;
		std::string taskTime;
		std::vector<std::string> description;
		std::vector<std::string> requirements;
		std::vector<Option> options;
		std::vector<Command> commands;
	};

	bool Load(Manifest& manifest, const char* directory);

	bool ValidateAndSet(const Json::JsonObject& json, Manifest& manifest);
	bool ValidateCommandsAndSet(const Json::JsonObject& json, std::vector<Command>& commands);
	bool ValidateOptionsAndSet(const Json::JsonObject& json, std::vector<Option>& options);
	bool ValidateSectionsAndSet(const Json::JsonObject& json, std::vector<Option>& options, std::vector<Command>& commands);
	bool ValidateTxtAndSet(const Json::JsonObject& json, std::vector<std::string>& property, const char* propName);
	bool CheckKeyAndSet(const Json::JsonObject& json, const char* key, std::string& property);
	bool CheckKeyAndSet(const Json::JsonObject& json, const char* key, std::string& property, std::string defValue);
	bool CheckKeyAndSet(const Json::JsonObject& json, const char* key, SelectOption& property);
	bool CheckKeyAndSet(const Json::JsonObject& json, const char* key, bool& property);
};

#endif
