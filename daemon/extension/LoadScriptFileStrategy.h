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
#include "FileSystem.h"
#include <memory>

namespace LoadScriptFileStrategy
{
	extern const char* BEGIN_SCRIPT_SIGNATURE;
	extern const char* POST_SCRIPT_SIGNATURE;
	extern const char* SCAN_SCRIPT_SIGNATURE;
	extern const char* QUEUE_SCRIPT_SIGNATURE;
	extern const char* SCHEDULER_SCRIPT_SIGNATURE;
	extern const char* FEED_SCRIPT_SIGNATURE;
	extern const char* END_SCRIPT_SIGNATURE;
	extern const char* QUEUE_EVENTS_SIGNATURE;
	extern const char* TASK_TIME_SIGNATURE;
	extern const char* DEFINITION_SIGNATURE;

	extern const int BEGIN_SINGNATURE_LEN;
	extern const int QUEUE_EVENTS_SIGNATURE_LEN;
	extern const int TASK_TIME_SIGNATURE_LEN;
	extern const int DEFINITION_SIGNATURE_LEN;

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

	class ManifestBased : public Strategy {
	public:
		ManifestBased() = delete;
		explicit ManifestBased(ManifestFile::Manifest& manifest_);
		bool Load(Script& script) const override;
		~ManifestBased() {}
	private:
		ManifestFile::Manifest manifest;
	};

	class Factory
	{
	public:
		static std::unique_ptr<const Strategy> Create(const char* dir);
	};
}

#endif
