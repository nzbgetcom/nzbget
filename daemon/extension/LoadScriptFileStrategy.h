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

#include "ManifestFile.h"
#include "Script.h"

namespace LoadScriptFileStrategy
{
	static const char* BEGIN_SCRIPT_SIGNATURE = "### NZBGET ";
	static const char* POST_SCRIPT_SIGNATURE = "POST-PROCESSING";
	static const char* SCAN_SCRIPT_SIGNATURE = "SCAN";
	static const char* QUEUE_SCRIPT_SIGNATURE = "QUEUE";
	static const char* SCHEDULER_SCRIPT_SIGNATURE = "SCHEDULER";
	static const char* FEED_SCRIPT_SIGNATURE = "FEED";
	static const char* END_SCRIPT_SIGNATURE = " SCRIPT";
	static const char* QUEUE_EVENTS_SIGNATURE = "### QUEUE EVENTS:";
	static const char* TASK_TIME_SIGNATURE = "### TASK TIME:";
	static const char* DEFINITION_SIGNATURE = "###";

	class Strategy {
	public:
		virtual bool Load(Script& script) const = 0;
		virtual ~Strategy() = default;
	};

	class HeaderConfigBased : public Strategy {
	public:
		bool Load(Script& script) const override;
		~HeaderConfigBased() = default;
	};

	// class ManifestBased : public Strategy {
	// public:
	// 	ManifestBased() = delete;
	// 	explicit ManifestBased(ManifestFile::Manifest&& manifest_);
	// 	bool Load(Script& script) const override;
	// 	~ManifestBased() {}
	// private:
	// 	ManifestFile::Manifest manifest;
	// };
}

#endif
