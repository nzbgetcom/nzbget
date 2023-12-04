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

#include "Util.h"
#include "FileSystem.h"
#include "Options.h"
#include "NString.h"
#include "ExtensionLoader.h"
#include "ExtensionManager.h"

namespace ExtensionManager
{
	const Extensions& Manager::GetExtensions() const
	{
		return m_extensions;
	}

	void Manager::LoadExtensions(const char* directory)
	{
		Extension extension;

		if (ExtensionLoader::V2::Load(extension, directory)) {

			if (!ScriptExists(m_extensions, script.GetName()))
			{
				m_extensions.push_back(std::move(extension));
			}
			return;
		}

		DirBrowser dir(directory);
		while (const char* filename = dir.Next())
		{
			if (filename[0] == '.' || filename[0] == '_')
				continue;

			BString<1024> fullFilename("%s%c%s", directory, PATH_SEPARATOR, filename);

			if (!FileSystem::DirectoryExists(fullFilename))
			{
				BString<1024> scriptName = BuildScriptName(directory, filename, isSubDir);
				if (ScriptExists(scripts, filename))
				{
					continue;
				}

				extension.SetName(scriptName);
				extension.SetLocation(fullFilename);
				if (ExtensionLoader::V1::Load(extension))
				{
					m_extensions.push_back(std::move(extension));
				}
			}
			else if (!isSubDir)
			{
				LoadScriptDir(scripts, fullFilename, true);
			}
		}
	}

	void Manager::CreateTasks()
	{
		for (Extension& extension : m_exensions)
		{
			if (!extension.GetSchedulerScript() || Util::EmptyStr(extension.GetTaskTime()))
			{

			}
			Tokenizer tok(g_Options->GetExtensions(), ",;");
			while (const char* scriptName = tok.Next())
			{
				if (FileSystem::SameFilename(scriptName, extension.GetName()))
				{
					g_Options->CreateSchedulerTask(0, extension.GetTaskTime(),
						nullptr, Options::scScript, extension.GetName());
					break;
				}
			}
		}
	}
}
