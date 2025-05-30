/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2023-2025 Denis <denis@nzbget.com>
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
	const char* DEFAULT_SECTION_NAME = "options";
	const char* BEGIN_SCRIPT_SIGNATURE = "### NZBGET ";
	const char* BEGIN_SCRIPT_COMMANDS_AND_OTPIONS = "### OPTIONS";
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

	namespace V1
	{
		bool Load(Extension::Script& script, const char* location, const char* rootDir)
		{
			std::ifstream file(script.GetEntry());
			if (!file.is_open())
			{
				return false;
			}

			std::string queueEvents;
			std::string taskTime;
			std::string about;

			Extension::Kind kind;
			std::vector<std::string> description;
			std::vector<std::string> requirements;
			std::vector<ManifestFile::Option> options;
			std::vector<ManifestFile::Command> commands;

			bool inBeforeConfig = false;
			bool inConfig = false;
			bool inAbout = false;
			bool inDescription = false;

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

				// if TASK TIME, e.g. ### TASK TIME: *;*:00;*:30	###
				if (!strncmp(line.c_str(), TASK_TIME_SIGNATURE, TASK_TIME_SIGNATURE_LEN))
				{
					taskTime = line.substr(TASK_TIME_SIGNATURE_LEN + 1);
					RemoveTailAndTrim(taskTime, "###");
					continue;
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
					continue;
				}

				// if QUEUE EVENTS, e.g. ### QUEUE EVENTS: NZB_ADDED, NZB_DOWNLOADED	###
				if (!strncmp(line.c_str(), QUEUE_EVENTS_SIGNATURE, QUEUE_EVENTS_SIGNATURE_LEN))
				{
					queueEvents = line.substr(QUEUE_EVENTS_SIGNATURE_LEN + 1);
					RemoveTailAndTrim(queueEvents, "###");
					continue;
				}

				// if about, e.g. # Sorts movies and tv shows.
				if (inConfig && !strncmp(line.c_str(), "# ", 2) && !inDescription)
				{
					inAbout = true;
					about.append(line.substr(2)).push_back('\n');
					continue;
				}

				if (inConfig && !strncmp(line.c_str(), "#", 1) && inAbout)
				{
					inAbout = false;
					inDescription = true;
					continue;
				}

				// if requirements: e.g. NOTE: This script requires Python to be installed on your system.
				if (inConfig && !strncmp(line.c_str(), "# NOTE: ", 8) && inDescription)
				{
					requirements.push_back(line.substr(strlen("# NOTE: ")));
					continue;
				}

				// if description: e.g # This is a script for downloaded TV shows and movies...
				if (inConfig && !strncmp(line.c_str(), "# ", 2) && inDescription)
				{
					description.push_back(line.substr(2));
					continue;
				}

				// if "OPTIONS" and other sections, e.g.: ### OPTIONS or ### CATEGORIES
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

			BuildDisplayName(script);
			Util::TrimRight(about);

			requirements.shrink_to_fit();
			options.shrink_to_fit();
			commands.shrink_to_fit();

			script.SetLocation(location);
			script.SetRootDir(rootDir);
			script.SetRequirements(std::move(requirements));
			script.SetKind(std::move(kind));
			script.SetQueueEvents(std::move(queueEvents));
			script.SetAbout(std::move(about));
			script.SetDescription(std::move(description));
			script.SetTaskTime(std::move(taskTime));
			script.SetOptions(std::move(options));
			script.SetCommands(std::move(commands));

			return true;
		}

		void RemoveTailAndTrim(std::string& str, const char* tail)
		{
			size_t tailIdx = str.find(tail);
			if (tailIdx != std::string::npos)
			{
				str.erase(tailIdx);
			}
			Util::TrimRight(str);
		}

		void BuildDisplayName(Extension::Script& script)
		{
			BString<1024> shortName = script.GetName();
			if (char* ext = strrchr(shortName, '.')) *ext = '\0'; // strip file extension

			const char* displayName = FileSystem::BaseFileName(shortName);

			script.SetDisplayName(displayName);
		}

		void ParseOptionsAndCommands(
			std::ifstream& file,
			std::vector<ManifestFile::Option>& options,
			std::vector<ManifestFile::Command>& commands)
		{
			std::vector<ManifestFile::SelectOption> selectOpts;
			std::vector<std::string> description;
			std::string currSectionName = DEFAULT_SECTION_NAME;

			std::string line;
			while (std::getline(file, line))
			{
				if (strstr(line.c_str(), END_SCRIPT_SIGNATURE))
				{
					break;
				}

				if (line.empty())
				{
					continue;
				}

				if (!strncmp(line.c_str(), DEFINITION_SIGNATURE, DEFINITION_SIGNATURE_LEN))
				{
					currSectionName = line.substr(DEFINITION_SIGNATURE_LEN + 1);
					RemoveTailAndTrim(currSectionName, "###");
					continue;
				}

				size_t selectStartIdx = line.rfind("(");
				size_t selectEndIdx = line.rfind(")");
				bool hasSelectOptions = description.empty()
					&& !strncmp(line.c_str(), "# ", 2)
					&& selectStartIdx != std::string::npos
					&& selectEndIdx != std::string::npos
					&& selectEndIdx == line.length() - 2;

				// e.g. # Description (Always, OnFailure) or # Description (1-65535) or # Description (1, 2-5, 10).
				if (hasSelectOptions)
				{
					std::string selectOptionsStr = line.substr(selectStartIdx + 1, selectEndIdx - selectStartIdx - 1);
					auto result = ExtractElements(selectOptionsStr);
					auto& selectOptions = result.first;
					auto& delimiter = result.second;
					bool canBeNum = delimiter == "-";

					if (delimiter.empty()) // doesn't have
					{
						description.push_back(line.substr(2));
						continue;
					}

					if (canBeNum)
					{
						description.push_back(line.substr(2));
					}
					else
					{
						description.push_back(line.substr(2, selectStartIdx - 3) + ".");
					}

					selectOpts = GetSelectOptions(selectOptions, canBeNum);
					continue;
				}

				if (!strncmp(line.c_str(), "# ", 2))
				{
					description.push_back(line.substr(2));
					continue;
				}

				if (strncmp(line.c_str(), "# ", 2))
				{
					// if option, e.g. #ConnectionTest=Send
					size_t eqPos = line.find("=");
					// if command, e.g. #ConnectionTest@Send Test E-Mail
					size_t atPos = line.find("@");

					if (atPos != std::string::npos && eqPos == std::string::npos)
					{
						ManifestFile::Command command{};
						command.action = line.substr(atPos + 1);
						Util::Trim(command.action);
						ParseSectionAndSet<ManifestFile::Command>(command, currSectionName, line, atPos);
						command.description = std::move(description);
						commands.push_back(std::move(command));
						description.clear();
						selectOpts.clear();
						continue;
					}

					if (eqPos != std::string::npos)
					{
						ManifestFile::Option option{};
						ParseSectionAndSet<ManifestFile::Option>(option, currSectionName, line, eqPos);
						bool canBeNum = !selectOpts.empty() && std::get_if<double>(&selectOpts[0]);
						std::string value = line.substr(eqPos + 1);
						Util::Trim(value);
						option.value = GetSelectOpt(value, canBeNum);
						option.description = std::move(description);
						option.select = std::move(selectOpts);
						options.push_back(std::move(option));
						description.clear();
						selectOpts.clear();
						continue;
					}
				}
			}
		}

		std::vector<ManifestFile::SelectOption>
		GetSelectOptions(const std::vector<std::string>& opts, bool canBeNum)
		{
			std::vector<ManifestFile::SelectOption> selectOpts;

			for (const auto& option : opts)
			{
				selectOpts.push_back(GetSelectOpt(option, canBeNum));
			}
			return selectOpts;
		}

		ManifestFile::SelectOption GetSelectOpt(const std::string& val, bool canBeNum)
		{
			if (!canBeNum)
			{
				return ManifestFile::SelectOption(val);
			}

			auto result = Util::StrToNum<double>(val);
			if (result.has_value())
			{
				return ManifestFile::SelectOption(result.value());
			}

			return ManifestFile::SelectOption(val);
		}

		std::pair<std::vector<std::string>, std::string>
		ExtractElements(const std::string& str)
		{
			std::vector<std::string> elements;
			std::string word;

			for (size_t i = 0; i < str.size(); ++i)
			{
				if (i == (str.size() - 1))
				{
					word += str[i];
					elements.push_back(std::move(word));
					break;
				}

				if (str[i] == ',')
				{
					elements.push_back(std::move(word));
					word.clear();
					continue;
				}

				if (str[i] == ' ' && word.empty())
				{
					continue;
				}

				if (str[i] == ' ' && !word.empty())
				{
					elements.clear();
					elements.push_back(str);
					return std::make_pair(std::move(elements), "");
				}

				if (str[i] == '-' && elements.empty())
				{
					std::string restWord;
					for (size_t j = i + 1; j < str.size(); ++j)
					{
						if (j >= str.size() - 1)
						{
							restWord += str[j];
							elements.push_back(std::move(word));
							elements.push_back(std::move(restWord));
							return std::make_pair(std::move(elements), "-");
						}

						if (str[j] == ' ')
						{
							elements.push_back(str);
							return std::make_pair(std::move(elements), "");
						}

						if (str[j] == ',')
						{
							word += '-' + restWord;
							elements.push_back(std::move(word));
							word.clear();
							i = j;
							break;
						}

						restWord += str[j];
					}

					continue;
				}

				word += str[i];
			}

			if (elements.size() == 1)
			{
				return std::make_pair(std::move(elements), "");
			}

			return std::make_pair(std::move(elements), ",");
		}
	}

	bool V2::Load(Extension::Script& script, const char* location, const char* rootDir)
	{
		ManifestFile::Manifest manifest{};
		if (!ManifestFile::Load(manifest, location))
			return false;

		std::string entry = std::string(location) + PATH_SEPARATOR + manifest.main;
		script.SetEntry(std::move(entry));
		script.SetLocation(location);
		script.SetRootDir(rootDir);
		script.SetAuthor(std::move(manifest.author));
		script.SetHomepage(std::move(manifest.homepage));
		script.SetLicense(std::move(manifest.license));
		script.SetVersion(std::move(manifest.version));
		script.SetNzbgetMinVersion(std::move(manifest.nzbgetMinVersion));
		script.SetDisplayName(std::move(manifest.displayName));
		script.SetName(std::move(manifest.name));
		script.SetAbout(std::move(manifest.about));
		script.SetDescription(std::move(manifest.description));
		script.SetKind(GetScriptKind(manifest.kind));
		script.SetQueueEvents(std::move(manifest.queueEvents));
		script.SetTaskTime(std::move(manifest.taskTime));
		script.SetRequirements(std::move(manifest.requirements));
		script.SetCommands(std::move(manifest.commands));
		script.SetOptions(std::move(manifest.options));
		return true;
	}

	Extension::Kind GetScriptKind(const std::string& line)
	{
		Extension::Kind kind;
		kind.post = strstr(line.c_str(), POST_SCRIPT_SIGNATURE) != nullptr;
		kind.scan = strstr(line.c_str(), SCAN_SCRIPT_SIGNATURE) != nullptr;
		kind.queue = strstr(line.c_str(), QUEUE_SCRIPT_SIGNATURE) != nullptr;
		kind.scheduler = strstr(line.c_str(), SCHEDULER_SCRIPT_SIGNATURE) != nullptr;
		kind.feed = strstr(line.c_str(), FEED_SCRIPT_SIGNATURE) != nullptr;
		return kind;
	}
}
