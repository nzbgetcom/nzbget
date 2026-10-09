/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Denis <denis@nzbget.com>
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


#ifndef DUPEUTIL_H
#define DUPEUTIL_H

#include <fstream>
#include <string>
#include "FileSystem.h"

/* Small helpers the DupeSearch files share. Needs nzbget.h first (fs). */
namespace DupeUtil
{
	/* writes a temporary file and renames it over path, so a crash can't leave
	 * half of the file; false if it couldn't be written */
	inline bool WriteAtomic(const std::string& path, const std::string& data)
	{
		std::string temp = path + ".new";
		{
			std::ofstream file(fs::u8path(temp), std::ios::binary | std::ios::trunc);
			file.write(data.data(), data.size());
			// checked after the close: a small file is still in the stream's
			// buffer before it, and a failed write (disk full) went unseen and
			// the good file was replaced by an empty one
			file.close();
			if (file.fail())
			{
				FileSystem::DeleteFile(temp.c_str());
				return false;
			}
		}
		return FileSystem::MoveFile(temp.c_str(), path.c_str());
	}

	inline std::string Lower(std::string text)
	{
		for (char& ch : text)
		{
			ch = (char)tolower((unsigned char)ch);
		}
		return text;
	}

	/* the text without any of chars at its start and end */
	inline std::string Trim(const std::string& text, const char* chars = " \t\r\n")
	{
		size_t begin = text.find_first_not_of(chars);
		if (begin == std::string::npos)
		{
			return "";
		}
		return text.substr(begin, text.find_last_not_of(chars) - begin + 1);
	}

	inline std::string ReadAll(const std::string& path)
	{
		std::ifstream file(fs::u8path(path), std::ios::binary);
		return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	}
}

#endif
