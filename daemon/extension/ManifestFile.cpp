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

#include "nzbget.h"
#include <fstream>
#include "ManifestFile.h"
#include "Json.h"
#include "FileSystem.h"

namespace ManifestFile
{
	bool Load(Manifest& manifest, const char* directory)
	{
		DirBrowser dir(directory);
		while (const char* filename = dir.Next()) 
		{
			if (strncmp(filename, MANIFEST_FILE, sizeof(MANIFEST_FILE)) == 0)
			{
				BString<1024> fullFilename("%s%c%s", directory, PATH_SEPARATOR, MANIFEST_FILE);
				std::fstream fs(fullFilename);
				Json::error_code ec;
				
				Json::JSON json = Json::Read(fs, ec);
				if (ec)
					return false;

				manifest.author = json.at("author").as_string().c_str();
				manifest.entry = json.at("entry").as_string().c_str();
				manifest.description = json.at("description").as_string().c_str();
				manifest.version = json.at("version").as_string().c_str();
				manifest.name = json.at("name").as_string().c_str();
				manifest.displayName = json.at("displayName").as_string().c_str();
				manifest.kind = json.at("kind").as_string().c_str();
				manifest.license = json.at("license").as_string().c_str();
				return true;
			}
		}
		return false;
	}
}
