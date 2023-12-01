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

#ifndef MANIFESTFILE_H
#define MANIFESTFILE_H

#include <string>
#include <vector>
#include "Json.h"

namespace ManifestFile
{
	extern const char* MANIFEST_FILE;

	struct Option
	{
		std::string name;
		std::string displayName;
		std::string description;
		std::string value;
		std::vector<std::string> select;
	};

	struct Command 
	{
		std::string name;
		std::string displayName;
		std::string description;
		std::string action;
	};

	struct Manifest
	{
		std::string author;
		std::string entry;
		std::string kind;
		std::string name;
		std::string displayName;
		std::string version;
		std::string license;
		std::string description;
		std::string queueEvents;
		std::string taskTime;
		std::vector<Option> options;
		std::vector<Command> commands;
	};

	bool Load(Manifest& manifest, const char* directory);
	bool ValidateRequiredAndSet(const Json::JsonObject& json, Manifest& manifest);
	bool ValidateCommandsAndSet(const Json::JsonObject& json, std::vector<Command>& commands);
	bool ValidateOptionsAndSet(const Json::JsonObject& json, std::vector<Option>& options);
	bool CheckKeyAndSet(const Json::JsonObject& json, const char* key, std::string& property);
};

#endif
