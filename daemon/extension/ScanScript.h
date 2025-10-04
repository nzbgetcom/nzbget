/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2024-2025 Denis <denis@nzbget.com>
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


#ifndef SCANSCRIPT_H
#define SCANSCRIPT_H

#include "NzbScript.h"

class ScanScriptController : public NzbScriptController
{
public:
	ScanScriptController(
		std::string& nzbName,
		std::string& category,
		std::string& dupeKey)
		: m_nzbName{ nzbName }
		, m_category{ category }
		, m_dupeKey{ dupeKey } {}

	static void ExecuteScripts(
		const char* nzbFilename,
		NzbInfo* nzbInfo,
		const char* directory,
		std::string& nzbName,
		std::string& category,
		int* priority,
		NzbParameterList* parameters,
		bool* addTop,
		bool* addPaused,
		std::string& dupeKey,
		int* dupeScore,
		EDupeMode* dupeMode);
	static bool HasScripts();

protected:
	void ExecuteScript(std::shared_ptr<const Extension::Script> script) override;
	void AddMessage(Message::EKind kind, const char* text) override;

	void SetPriority(int* priority) { m_priority = priority; }
	void SetDupeScore(int* dupeScore) { m_dupeScore = dupeScore; }
	void SetDupeMode(EDupeMode* dupeMode) { m_dupeMode = dupeMode; }
	void SetAddTop(bool* addTop) { m_addTop = addTop; }
	void SetAddPaused(bool* addPaused) { m_addPaused = addPaused; }
	void SetParameters(NzbParameterList* parameters) { m_parameters = parameters; }

private:
	const char* m_nzbFilename;
	const char* m_url;
	const char* m_directory;
	std::string& m_nzbName;
	std::string& m_category;
	std::string& m_dupeKey;
	int* m_priority;
	int* m_dupeScore;
	bool* m_addTop;
	bool* m_addPaused;
	EDupeMode* m_dupeMode;
	NzbParameterList* m_parameters;
	int m_prefixLen = 0;

	void PrepareParams(const char* scriptName);
};

#endif
