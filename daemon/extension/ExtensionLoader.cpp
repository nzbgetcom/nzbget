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
#include <sstream>
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

				// if about, e.g. # Sort movies and tv shows.
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
		namespace
		{
			void RemoveTailAndTrim(std::string& str, const char* tail)
			{
				size_t tailIdx = str.find(tail);
				if (tailIdx != std::string::npos)
				{
					str = str.substr(0, tailIdx);
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
				std::vector<std::string> description;
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
					bool hasSelectOptions = selectStartIdx != std::string::npos
						&& selectEndIdx != std::string::npos
						&& selectEndIdx != std::string::npos
						&& !strncmp(line.c_str(), "# ", 2);

					// e.g. # When to send the message (Always, OnFailure) or # SMTP server port (1-65535).
					if (hasSelectOptions)
					{
						std::string comma = ", ";
						std::string dash = "-";
						bool foundComma = line.substr(selectStartIdx, selectEndIdx).find(comma) != std::string::npos;
						bool foundDash = line.substr(selectStartIdx, selectEndIdx).find(dash) != std::string::npos;
						if (!foundComma && !foundDash || !description.empty())
						{
							description.push_back(line.substr(2));
							continue;
						}

						std::string selectOptionsStr = line.substr(selectStartIdx + 1, selectEndIdx - selectStartIdx - 1);

						if (foundComma && !CheckCommaAfterEachWord(selectOptionsStr))
						{
							description.push_back(line.substr(2));
							continue;
						}
						if (foundDash)
						{
							description.push_back(line.substr(2));
						}
						else
						{
							description.push_back(line.substr(2, selectStartIdx - 3) + ".");
						}

						ManifestFile::Option option;
						std::vector<ManifestFile::SelectOption> selectOpts;
						std::string delimiter = foundDash ? dash : comma;
						ParseSelectOptions(selectOptionsStr, delimiter, selectOpts, foundDash);
						option.select = std::move(selectOpts);

						while (std::getline(file, line))
						{
							if (line == "#")
							{
								continue;
							}
							if (!strncmp(line.c_str(), "# ", 2))
							{
								line = line.substr(2);
								Util::TrimRight(line);
								if (!line.empty())
								{
									description.push_back(std::move(line));
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
									option.value = std::move(GetSelectOpt(value, foundDash));
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
						// if command, e.g. #ConnectionTest@Send Test E-Mail
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
								// if option, e.g. #Username=myaccount
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
						description.push_back(line.substr(2));
					}
				}
			}

			void ParseSelectOptions(
				std::string& line,
				const std::string& delimiter,
				std::vector<ManifestFile::SelectOption>& selectOpts,
				bool isDash)
			{
				size_t pos = 0;
				while ((pos = line.find(delimiter)) != std::string::npos)
				{
					std::string selectOpt = line.substr(0, pos);
					selectOpts.push_back(std::move(GetSelectOpt(selectOpt, isDash)));
					line.erase(0, pos + delimiter.length());
				}
				selectOpts.push_back(std::move(GetSelectOpt(line, isDash)));
			}

			ManifestFile::SelectOption GetSelectOpt(const std::string& val, bool isNumber)
			{
				if (isNumber)
					return ManifestFile::SelectOption(std::stod(val));

				return ManifestFile::SelectOption(val);
			}

			bool CheckCommaAfterEachWord(const std::string& sentence)
			{
				std::stringstream ss(sentence);
				std::string word;

				while (ss >> word) {
					if (!word.empty() && word.back() != ',' && !ss.eof())
					{
						return false;
					}
				}

				return true;
			}
		}
	}

	bool V2::Load(Extension::Script& script, const char* location, const char* rootDir)
	{
		ManifestFile::Manifest manifest;
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

	namespace
	{
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
}
