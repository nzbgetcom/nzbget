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

#ifndef MANIFESTFILE_H
#define MANIFESTFILE_H

#include <string>

namespace ManifestFile
{
	extern const char* MANIFEST_FILE;

	struct Manifest
	{
		std::string author;
		std::string entry;
		std::string kind;
		std::string name;
		std::string displayName;
		std::string version;
		std::string license;
		std::string description;
	};

	bool Load(Manifest& manifest, const char* directory);
};

#endif