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

#include <fstream>
#include "ExtensionLoader.h"
#include "ManifestFile.h"
#include "ScriptConfig.h"
#include "FileSystem.h"

namespace ExtensionLoader
{
	const char* BEGIN_SCRIPT_SIGNATURE = "### NZBGET ";
	const char* BEGIN_SCRIPT_COMMANDS_AND_OTPIONS = "### OPTIONS ";
	const char* POST_SCRIPT_SIGNATURE = "POST-PROCESSING";
	const char* SCAN_SCRIPT_SIGNATURE = "SCAN";
	const char* QUEUE_SCRIPT_SIGNATURE = "QUEUE";
	const char* SCHEDULER_SCRIPT_SIGNATURE = "SCHEDULER";
	const char* FEED_SCRIPT_SIGNATURE = "FEED";
	const char* END_SCRIPT_SIGNATURE = " SCRIPT";
	const char* QUEUE_EVENTS_SIGNATURE = "### QUEUE EVENTS:";
	const char* TASK_TIME_SIGNATURE = "### TASK TIME:";
	const char* DEFINITION_SIGNATURE = "###";

	const int BEGIN_SCRIPT_COMMANDS_AND_OTPIONS_LEN = strlen(BEGIN_SCRIPT_COMMANDS_AND_OTPIONS);
	const int BEGIN_SINGNATURE_LEN = strlen(BEGIN_SCRIPT_SIGNATURE);
	const int QUEUE_EVENTS_SIGNATURE_LEN = strlen(QUEUE_EVENTS_SIGNATURE);
	const int TASK_TIME_SIGNATURE_LEN = strlen(TASK_TIME_SIGNATURE);
	const int DEFINITION_SIGNATURE_LEN = strlen(DEFINITION_SIGNATURE);

	bool V1::Load(Extension::Script& script)
	{
		std::ifstream file(script.GetLocation());
		if (!file.is_open())
		{
			return false;
		}

		bool feedScript = false;
		std::string queueEvents;
		std::string taskTime;
		std::string description;

		Extension::Kind kind;
		std::vector<ManifestFile::Option> options;
		std::vector<ManifestFile::Command> commands;

		bool inBeforeConfig = false;
		bool inConfig = false;

		// Declarations "QUEUE EVENT:" and "TASK TIME:" can be placed:
		// - in script definition body (between opening and closing script signatures);
		// - immediately before script definition (before opening script signature);
		// - immediately after script definition (after closing script signature).
		// The last two pissibilities are provided to increase compatibility of scripts with older
		// nzbget versions which do not expect the extra declarations in the script defintion body.

		std::string line;
		while (std::getline(file, line))
		{
			if (line.empty())
			{
				continue;
			}
			if (!inBeforeConfig && !strncmp(line.c_str(), DEFINITION_SIGNATURE, DEFINITION_SIGNATURE_LEN))
			{
				inBeforeConfig = true;
			}
			if (!inBeforeConfig && !inConfig)
			{
				continue;
			}
			if (!strncmp(line.c_str(), TASK_TIME_SIGNATURE, TASK_TIME_SIGNATURE_LEN))
			{
				taskTime = line.substr(TASK_TIME_SIGNATURE_LEN + 1);
				RemoveTailAndTrim(taskTime, "###");
			}
			if (!strncmp(line.c_str(), BEGIN_SCRIPT_SIGNATURE, BEGIN_SINGNATURE_LEN) && strstr(line.c_str(), END_SCRIPT_SIGNATURE))
			{
				if (inConfig)
				{
					break;
				}
				inBeforeConfig = false;
				inConfig = true;
				kind = GetScriptKind(line.c_str());
			}
			if (!strncmp(line.c_str(), QUEUE_EVENTS_SIGNATURE, QUEUE_EVENTS_SIGNATURE_LEN))
			{
				queueEvents = line.substr(QUEUE_EVENTS_SIGNATURE_LEN + 1);
				RemoveTailAndTrim(queueEvents, "###");
			}
			if (inConfig && !strncmp(line.c_str(), "# ", 2))
			{
				description.append(line.substr(2)).push_back('\n');
			}
			if (!strncmp(line.c_str(), BEGIN_SCRIPT_COMMANDS_AND_OTPIONS, BEGIN_SCRIPT_COMMANDS_AND_OTPIONS_LEN))
			{
				ParseOptionsAndCommands(file, options, commands);
				break;
			}
		}

		if (!(kind.post || kind.scan || kind.queue || kind.scheduler || kind.feed))
		{
			return false;
		}

		script.SetKind(std::move(kind));
		script.SetQueueEvents(queueEvents.c_str());
		script.SetDescription(description.c_str());
		script.SetTaskTime(taskTime.c_str());
		BuildDisplayName(script);
		script.SetOptions(std::move(options));
		script.SetCommands(std::move(commands));

		return true;
	}

	void V1::RemoveTailAndTrim(std::string& str, const char* tail)
	{
		size_t tailIdx = str.find(tail);
		if (tailIdx != std::string::npos)
		{
			str = str.substr(0, tailIdx);
		}
		Util::TrimRight(str);
	}

	void V1::BuildDisplayName(Extension::Script& script)
	{
		BString<1024> shortName = script.GetName();
		if (char* ext = strrchr(shortName, '.')) *ext = '\0'; // strip file extension

		const char* displayName = FileSystem::BaseFileName(shortName);

		script.SetDisplayName(displayName);
	}

	void V1::ParseOptionsAndCommands(
		std::ifstream& file,
		std::vector<ManifestFile::Option>& options,
		std::vector<ManifestFile::Command>& commands)
	{
		std::string description;
		std::string line;
		while (getline(file, line))
		{
			if (strstr(line.c_str(), END_SCRIPT_SIGNATURE))
			{
				break;
			}
			if (line.empty())
			{
				continue;
			}

			size_t selectStartIdx = line.find("(");
			size_t selectEndIdx = line.find(")");
			if (selectStartIdx != std::string::npos && selectEndIdx != std::string::npos)
			{
				std::string delimiter = ", ";
				bool foundComma = line.substr(selectStartIdx, selectEndIdx).find(delimiter) != std::string::npos;
				if (!foundComma || !description.empty())
				{
					description += line.substr(2) + '\n';
					continue;
				}
				ManifestFile::Option option;
				std::vector<std::string> select;
				description += line.substr(2, selectStartIdx - 3) + ".\n";

				std::string selectStr = line.substr(selectStartIdx + 1, selectEndIdx - selectStartIdx - 1);
				size_t pos = 0;
	
				while ((pos = selectStr.find(delimiter)) != std::string::npos) {
					std::string option = selectStr.substr(0, pos);
					select.push_back(option);
					selectStr.erase(0, pos + delimiter.length());
				}
				select.push_back(selectStr);
				option.select = std::move(select);

				while (std::getline(file, line))
				{
					if (line == "#")
					{
						continue;;
					}
					if (!strncmp(line.c_str(), "# ", 2))
					{
						line = line.substr(2);
						Util::TrimRight(line);
						if (!line.empty())
						{
							description += line + '\n';
						}
						continue;
					}

					if (strncmp(line.c_str(), "# ", 2))
					{
						size_t delimPos = line.find("=");
						if (delimPos != std::string::npos)
						{
							std::string name = line.substr(1, delimPos - 1);
							std::string value = line.substr(delimPos + 1);
							option.value = std::move(value);
							option.name = std::move(name);
							option.displayName = option.name;
						}
						option.description = std::move(description);
						options.push_back(std::move(option));
						break;
					}
				}
				continue;
			}
			if (strncmp(line.c_str(), "# ", 2))
			{	
				size_t eqPos = line.find("=");
				size_t atPos = line.find("@");
				if (atPos != std::string::npos && eqPos == std::string::npos)
				{
					ManifestFile::Command command;
					std::string name = line.substr(1, atPos - 1);
					std::string action = line.substr(atPos + 1);
					command.action = std::move(action);
					command.name = std::move(name);
					command.description = std::move(description);
					command.displayName = command.name;
					commands.push_back(std::move(command));
				}
				else
				{
					if (eqPos != std::string::npos)
					{
						ManifestFile::Option option;
						std::string name = line.substr(1, eqPos - 1);
						std::string value = line.substr(eqPos + 1);
						option.value = std::move(value);
						option.name = std::move(name);
						option.description = std::move(description);
						option.displayName = option.name;
						options.push_back(std::move(option));
					}
				}
			}
			else
			{
				description += line.substr(2) + '\n';
			}
		}
	}

	bool V2::Load(Extension::Script& script, const char* directory)
	{
		ManifestFile::Manifest manifest;
		if (!ManifestFile::Load(manifest, directory))
			return false;

		BString<1024> location("%s%c%s", directory, PATH_SEPARATOR, manifest.main.c_str());
		script.SetLocation(location);
		script.SetAuthor(manifest.author.c_str());
		script.SetLicense(manifest.license.c_str());
		script.SetVersion(manifest.version.c_str());
		script.SetDisplayName(manifest.displayName.c_str());
		script.SetName(manifest.name.c_str());
		script.SetDescription(manifest.description.c_str());
		script.SetKind(GetScriptKind(manifest.kind.c_str()));
		script.SetQueueEvents(manifest.queueEvents.c_str());
		script.SetTaskTime(manifest.taskTime.c_str());
		script.SetCommands(std::move(manifest.commands));
		script.SetOptions(std::move(manifest.options));
		return true;
	}

	Extension::Kind GetScriptKind(const char* line)
	{
		Extension::Kind kind;
		kind.post = strstr(line, POST_SCRIPT_SIGNATURE) != nullptr;
		kind.scan = strstr(line, SCAN_SCRIPT_SIGNATURE) != nullptr;
		kind.queue = strstr(line, QUEUE_SCRIPT_SIGNATURE) != nullptr;
		kind.scheduler = strstr(line, SCHEDULER_SCRIPT_SIGNATURE) != nullptr;
		kind.feed = strstr(line, FEED_SCRIPT_SIGNATURE) != nullptr;
		return kind;
	}
}
