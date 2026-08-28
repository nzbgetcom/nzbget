/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2013-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include "Util.h"
#include "FileSystem.h"
#include "Options.h"
#include "Log.h"
#include "ScriptConfig.h"

void ScriptConfig::InitOptions()
{
	InitConfigTemplates();
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
	struct CaseInsensitiveLess
	{
		bool operator()(std::string_view a, std::string_view b) const
		{
			return std::lexicographical_compare(
				a.begin(), a.end(), b.begin(), b.end(),
				[](unsigned char ca, unsigned char cb)
				{
					return std::tolower(ca) < std::tolower(cb);
				}
			);
		}
	};

	// save to config file
	DiskFile infile;

	if (!infile.Open(g_Options->GetConfigFilename(), DiskFile::omReadWrite))
	{
		return false;
	}

	std::map<std::string_view, std::string_view, CaseInsensitiveLess> values;
	for (const auto& entry : *optEntries)
	{
		if (entry.GetName() && entry.GetValue())
		{
			values[entry.GetName()] = entry.GetValue();
		}
	}

	std::set<std::string_view, CaseInsensitiveLess> writtenOptions;

	// read config file into memory array
	int fileLen = (int)FileSystem::FileSize(g_Options->GetConfigFilename()) + 1;
	CString content;
	content.Reserve(fileLen);
	std::vector<CString> config;
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
				auto it = values.find(*optname);
				if (it != values.end() && writtenOptions.insert(it->first).second)
				{
					infile.Print("%s=%.*s\n", *optname, static_cast<int>(it->second.size()), it->second.data());
				}
			}
		}
		else
		{
			infile.Print("%s", *buf);
		}
	}

	// write new options
	for (const auto& entry : *optEntries)
	{
		if (entry.GetName() && entry.GetValue())
		{
			const char* name = entry.GetName();
			if (writtenOptions.insert(name).second)
			{
				auto it = values.find(name);
				std::string_view val = (it != values.end()) ? it->second : entry.GetValue();
				infile.Print("%s=%.*s\n", name, static_cast<int>(val.size()), val.data());
			}
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
	configTemplates->emplace_back(ConfigTemplate(buffer));

	return true;
}

void ScriptConfig::InitConfigTemplates()
{
	if (!LoadConfigTemplates(&m_configTemplates))
	{
		error("Could not read configuration templates");
	}
}
