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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LOADSCRIPTFILESTRATEGY_H
#define LOADSCRIPTFILESTRATEGY_H

#include <vector>
#include "Extension.h"
#include "Util.h"

namespace ExtensionLoader
{
	extern const char* BEGIN_SCRIPT_SIGNATURE;
	extern const char* BEGIN_SCRIPT_COMMANDS_AND_OTPIONS;
	extern const char* POST_SCRIPT_SIGNATURE;
	extern const char* SCAN_SCRIPT_SIGNATURE;
	extern const char* QUEUE_SCRIPT_SIGNATURE;
	extern const char* SCHEDULER_SCRIPT_SIGNATURE;
	extern const char* FEED_SCRIPT_SIGNATURE;
	extern const char* END_SCRIPT_SIGNATURE;
	extern const char* QUEUE_EVENTS_SIGNATURE;
	extern const char* TASK_TIME_SIGNATURE;
	extern const char* DEFINITION_SIGNATURE;

	extern const int BEGIN_SCRIPT_COMMANDS_AND_OTPIONS_LEN;
	extern const int BEGIN_SINGNATURE_LEN;
	extern const int QUEUE_EVENTS_SIGNATURE_LEN;
	extern const int TASK_TIME_SIGNATURE_LEN;
	extern const int DEFINITION_SIGNATURE_LEN;

	namespace V1
	{
		bool Load(Extension::Script& script);
		void ParseOptionsAndCommands(
			std::ifstream& file,
			std::vector<ManifestFile::Option>& options,
			std::vector<ManifestFile::Command>& commands
		);
		void RemoveTailAndTrim(std::string& str, const char* tail);
	}

	namespace V2
	{
		bool Load(Extension::Script& script, const char* directory);
	}

	Extension::Kind GetScriptKind(const char* line);
}

#endif
