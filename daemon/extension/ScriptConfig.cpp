/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2013-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"
#include "Util.h"
#include "FileSystem.h"
#include "Options.h"
#include "Log.h"
#include "ScriptConfig.h"
#include "LoadScriptFileStrategy.h"

void ScriptConfig::InitOptions()
{
	LoadScripts(m_scripts);
	InitConfigTemplates();
	CreateTasks();
}

bool ScriptConfig::LoadConfig(Options::OptEntries* optEntries)
{
	// read config file
	DiskFile infile;

	if (!infile.Open(g_Options->GetConfigFilename(), DiskFile::omRead))
	{
		return false;
	}

	int fileLen = (int)FileSystem::FileSize(g_Options->GetConfigFilename());
	CString buf;
	buf.Reserve(fileLen);

	while (infile.ReadLine(buf, fileLen + 1))
	{
		// remove trailing '\n' and '\r' and spaces
		Util::TrimRight(buf);

		// skip comments and empty lines
		if (buf[0] == 0 || buf[0] == '#' || strspn(buf, " ") == strlen(buf))
		{
			continue;
		}

		CString optname;
		CString optvalue;
		if (Options::SplitOptionString(buf, optname, optvalue))
		{
			optEntries->emplace_back(optname, optvalue);
		}
	}

	infile.Close();

	Options::ConvertOldOptions(optEntries);

	return true;
}

bool ScriptConfig::SaveConfig(Options::OptEntries* optEntries)
{
	// save to config file
	DiskFile infile;

	if (!infile.Open(g_Options->GetConfigFilename(), DiskFile::omReadWrite))
	{
		return false;
	}

	std::vector<CString> config;
	std::set<Options::OptEntry*> writtenOptions;

	// read config file into memory array
	int fileLen = (int)FileSystem::FileSize(g_Options->GetConfigFilename()) + 1;
	CString content;
	content.Reserve(fileLen);
	while (infile.ReadLine(content, fileLen + 1))
	{
		config.push_back(*content);
	}
	content.Clear();

	// write config file back to disk, replace old values of existing options with new values
	infile.Seek(0);
	for (CString& buf : config)
	{
		const char* eq = strchr(buf, '=');
		if (eq && buf[0] != '#')
		{
			// remove trailing '\n' and '\r' and spaces
			buf.TrimRight();

			CString optname;
			CString optvalue;
			if (g_Options->SplitOptionString(buf, optname, optvalue))
			{
				Options::OptEntry* optEntry = optEntries->FindOption(optname);
				if (optEntry)
				{
					infile.Print("%s=%s\n", optEntry->GetName(), optEntry->GetValue());
					writtenOptions.insert(optEntry);
				}
			}
		}
		else
		{
			infile.Print("%s", *buf);
		}
	}

	// write new options
	for (Options::OptEntry& optEntry : *optEntries)
	{
		std::set<Options::OptEntry*>::iterator fit = writtenOptions.find(&optEntry);
		if (fit == writtenOptions.end())
		{
			infile.Print("%s=%s\n", optEntry.GetName(), optEntry.GetValue());
		}
	}

	// close and truncate the file
	int pos = (int)infile.Position();
	infile.Close();

	FileSystem::TruncateFile(g_Options->GetConfigFilename(), pos);

	return true;
}

bool ScriptConfig::LoadConfigTemplates(ConfigTemplates* configTemplates)
{
	CharBuffer buffer;
	if (!FileSystem::LoadFileIntoBuffer(g_Options->GetConfigTemplate(), buffer, true))
	{
		return false;
	}
	configTemplates->emplace_back(Script("", ""), buffer);

	if (!g_Options->GetScriptDir())
	{
		return true;
	}

	Scripts scriptList;
	LoadScripts(scriptList);

	for (Script& script : scriptList)
	{
		DiskFile infile;
		if (!infile.Open(script.GetLocation(), DiskFile::omRead))
		{
			configTemplates->emplace_back(std::move(script), "");
			continue;
		}

		StringBuilder templ;
		char buf[1024];
		bool inConfig = false;
		bool inHeader = false;

		while (infile.ReadLine(buf, sizeof(buf) - 1))
		{
			if (!strncmp(buf, LoadScriptFileStrategy::BEGIN_SCRIPT_SIGNATURE, LoadScriptFileStrategy::BEGIN_SINGNATURE_LEN) &&
				strstr(buf, LoadScriptFileStrategy::END_SCRIPT_SIGNATURE) &&
				(strstr(buf, LoadScriptFileStrategy::POST_SCRIPT_SIGNATURE) ||
				 strstr(buf, LoadScriptFileStrategy::SCAN_SCRIPT_SIGNATURE) ||
				 strstr(buf, LoadScriptFileStrategy::QUEUE_SCRIPT_SIGNATURE) ||
				 strstr(buf, LoadScriptFileStrategy::SCHEDULER_SCRIPT_SIGNATURE) ||
				 strstr(buf, LoadScriptFileStrategy::FEED_SCRIPT_SIGNATURE)))
			{
				if (inConfig)
				{
					break;
				}
				inConfig = true;
				inHeader = true;
				continue;
			}

			inHeader &= !strncmp(buf, LoadScriptFileStrategy::DEFINITION_SIGNATURE, LoadScriptFileStrategy::DEFINITION_SIGNATURE_LEN);

			if (inConfig && !inHeader)
			{
				templ.Append(buf);
			}
		}

		infile.Close();

		configTemplates->emplace_back(std::move(script), templ);
	}

	return true;
}

void ScriptConfig::InitConfigTemplates()
{
	if (!LoadConfigTemplates(&m_configTemplates))
	{
		error("Could not read configuration templates");
	}
}

void ScriptConfig::LoadScripts(Scripts& scripts)
{
	if (Util::EmptyStr(g_Options->GetScriptDir()))
	{
		return;
	}

	Scripts tmpScripts;

	Tokenizer tokDir(g_Options->GetScriptDir(), ",;");
	while (const char* scriptDir = tokDir.Next())
	{
		LoadScriptDir(tmpScripts, scriptDir, false);
	}

	tmpScripts.sort(
		[](Script& script1, Script& script2)
		{
			return strcmp(script1.GetName(), script2.GetName()) < 0;
		});

	// first add all scripts from ScriptOrder
	Tokenizer tokOrder(g_Options->GetScriptOrder(), ",;");
	while (const char* scriptName = tokOrder.Next())
	{
		Scripts::iterator pos = std::find_if(tmpScripts.begin(), tmpScripts.end(),
			[scriptName](Script& script)
			{
				return !strcmp(script.GetName(), scriptName);
			});

		if (pos != tmpScripts.end())
		{
			scripts.splice(scripts.end(), tmpScripts, pos);
		}
	}

	// then add all other scripts from scripts directory
	scripts.splice(scripts.end(), std::move(tmpScripts));

	BuildScriptDisplayNames(scripts);
}

void ScriptConfig::LoadScriptDir(Scripts& scripts, const char* directory, bool isSubDir)
{
	DirBrowser dir(directory);

	while (const char* filename = dir.Next())
	{
		if (filename[0] != '.' && filename[0] != '_')
		{
			BString<1024> fullFilename("%s%c%s", directory, PATH_SEPARATOR, filename);

			if (!FileSystem::DirectoryExists(fullFilename))
			{
				BString<1024> scriptName = BuildScriptName(directory, filename, isSubDir);
				if (ScriptExists(scripts, scriptName))
				{
					continue;
				}

				const auto strategy = LoadScriptFileStrategy::Factory::Create(directory);
				Script script(scriptName, fullFilename);
				if (LoadScriptFile(script, *strategy))
				{
					scripts.push_back(std::move(script));
				}

			}
			else if (!isSubDir)
			{
				LoadScriptDir(scripts, fullFilename, true);
			}
		}
	}
}

bool ScriptConfig::LoadScriptFile(Script& script, const LoadScriptFileStrategy::Strategy &strategy)
{
	return strategy.Load(script);
}

BString<1024> ScriptConfig::BuildScriptName(const char* directory, const char* filename, bool isSubDir) const
{
	if (isSubDir)
	{
		BString<1024> directory2 = directory;
		int len = strlen(directory2);
		if (directory2[len-1] == PATH_SEPARATOR || directory2[len-1] == ALT_PATH_SEPARATOR)
		{
			// trim last path-separator
			directory2[len-1] = '\0';
		}

		return BString<1024>("%s%c%s", FileSystem::BaseFileName(directory2), PATH_SEPARATOR, filename);
	}
	else
	{
		return filename;
	}
}

bool ScriptConfig::ScriptExists(const Scripts& scripts, const char* scriptName) const
{
	return std::find_if(scripts.begin(), scripts.end(),
		[scriptName](const Script& script)
		{
			return !strcmp(script.GetName(), scriptName);
		}) != scripts.end();
}

void ScriptConfig::BuildScriptDisplayNames(Scripts& scripts)
{
	// trying to use short name without path and extension.
	// if there are other scripts with the same short name - using a longer name instead (with ot without extension)

	for (Script& script : scripts)
	{
		BString<1024> shortName = script.GetName();
		if (char* ext = strrchr(shortName, '.')) *ext = '\0'; // strip file extension

		const char* displayName = FileSystem::BaseFileName(shortName);

		for (Script& script2 : scripts)
		{
			BString<1024> shortName2 = script2.GetName();
			if (char* ext = strrchr(shortName2, '.')) *ext = '\0'; // strip file extension

			const char* displayName2 = FileSystem::BaseFileName(shortName2);

			if (!strcmp(displayName, displayName2) && script.GetName() != script2.GetName())
			{
				if (!strcmp(shortName, shortName2))
				{
					displayName = script.GetName();
				}
				else
				{
					displayName = shortName;
				}
				break;
			}
		}

		script.SetDisplayName(displayName);
	}
}

void ScriptConfig::CreateTasks()
{
	for (Script& script : m_scripts)
	{
		if (script.GetSchedulerScript() && !Util::EmptyStr(script.GetTaskTime()))
		{
			Tokenizer tok(g_Options->GetExtensions(), ",;");
			while (const char* scriptName = tok.Next())
			{
				if (FileSystem::SameFilename(scriptName, script.GetName()))
				{
					g_Options->CreateSchedulerTask(0, script.GetTaskTime(), 
						nullptr, Options::scScript, script.GetName());
					break;
				}
			}
		}
	}
}
