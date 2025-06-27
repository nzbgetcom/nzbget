/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2025 Denis <denis@nzbget.net>
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
#include "NzbScript.h"
#include "Options.h"
#include "Log.h"
#include "FileSystem.h"

/**
 * If szStripPrefix is not nullptr, only pp-parameters, whose names start with the prefix
 * are processed. The prefix is then stripped from the names.
 * If szStripPrefix is nullptr, all pp-parameters are processed; without stripping.
 */
void NzbScriptController::PrepareEnvParameters(NzbParameterList* parameters, const char* stripPrefix)
{
	int prefixLen = stripPrefix ? strlen(stripPrefix) : 0;

	for (NzbParameter& parameter : parameters)
	{
		const char* value = parameter.GetValue();

		if (stripPrefix && !strncmp(parameter.GetName(), stripPrefix, prefixLen) && (int)strlen(parameter.GetName()) > prefixLen)
		{
			SetEnvVarSpecial("NZBPR", parameter.GetName() + prefixLen, value);
		}
		else if (!stripPrefix)
		{
			SetEnvVarSpecial("NZBPR", parameter.GetName(), value);
		}
	}
}

void NzbScriptController::PrepareEnvScript(NzbParameterList* parameters, const char* scriptName)
{
	if (parameters)
	{
		PrepareEnvParameters(parameters, nullptr);
	}

	BString<1024> paramPrefix("%s:", scriptName);

	if (parameters)
	{
		PrepareEnvParameters(parameters, paramPrefix);
	}

	PrepareEnvOptions(paramPrefix);
}

void NzbScriptController::ExecuteScriptList(const char* scriptList)
{
	if (Util::EmptyStr(scriptList))
	{
		return;
	}

	Tokenizer tok(scriptList, ",;");
	while (const char* scriptName = tok.Next())
	{
		auto found = g_ExtensionManager->FindIf([&scriptName](auto script)
			{
				return strcmp(scriptName, script->GetName()) == 0;
			}
		);

		if (found)
		{
			ExecuteScript(std::move(found.value()));
		}
	}
}
