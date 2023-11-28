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

#ifndef SCRIPTCONFIG_H
#define SCRIPTCONFIG_H

#include "NString.h"
#include "Container.h"
#include "Options.h"
#include "Extension.h"
#include "LoadScriptFileStrategy.h"

class ScriptConfig
{
public:
	using Script = Extension::Script;
	using Scripts = std::list<Script>;

	class ConfigTemplate
	{
	public:
		ConfigTemplate(Script&& script, const char* templ)
			: m_script(std::move(script))
			, m_template(templ) {}
		Script* GetScript() { return &m_script; }
		const char* GetTemplate() { return m_template; }

	private:
		Script m_script;
		CString m_template;
	};

	using ConfigTemplates = std::deque<ConfigTemplate>;

	void InitOptions();
	Scripts& GetScripts() { return m_scripts; }
	bool LoadConfig(Options::OptEntries* optEntries);
	bool SaveConfig(Options::OptEntries* optEntries);
	bool LoadConfigTemplates(ConfigTemplates* configTemplates);
	ConfigTemplates* GetConfigTemplates() { return &m_configTemplates; }

private:
	Scripts m_scripts;
	ConfigTemplates m_configTemplates;

	void InitConfigTemplates();
	void CreateTasks();
	void LoadScriptDir(Scripts& scripts, const char* directory, bool isSubDir);
	void BuildScriptDisplayName(Script& script);
	void LoadScripts(Scripts& scripts);
	BString<1024> BuildScriptName(const char* directory, const char* filename, bool isSubDir) const;
	bool ScriptExists(const Scripts& scripts, const char* scriptName) const;
};

extern ScriptConfig* g_ScriptConfig;

#endif
