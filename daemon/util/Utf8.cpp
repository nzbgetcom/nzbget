
/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2024 Denis <denis@nzbget.com>
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

#include <windows.h>
#include "Utf8.h"

namespace Utf8
{
	std::optional<std::wstring> Utf8ToWide(const std::string& str)
	{
		if (str.empty()) return L"";

		int requiredSize = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
		if (requiredSize <= 0) return std::nullopt;

		wchar_t* buffer = new (std::nothrow) wchar_t[requiredSize];
		if (!buffer) return std::nullopt;

		requiredSize = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buffer, requiredSize);
		if (requiredSize <= 0)
		{
			delete[] buffer;
			return std::nullopt;
		}

		return std::wstring(buffer);
	}

	std::optional<std::string> WideToUtf8(const std::wstring& wstr)
	{
		if (wstr.empty()) return "";

		int requiredSize = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (requiredSize <= 0) return std::nullopt;

		char* buffer = new (std::nothrow) char[requiredSize];
		if (!buffer) return std::nullopt;

		requiredSize = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buffer, requiredSize, nullptr, nullptr);
		if (requiredSize <= 0)
		{
			delete[] buffer;
			return std::nullopt;
		}

		return std::string(buffer);
	}
}
