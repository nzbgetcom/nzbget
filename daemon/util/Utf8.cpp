
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
#include "Log.h"

namespace Utf8
{
	std::optional<std::wstring> Utf8ToWide(const std::string& str) noexcept
	{
		if (str.empty()) return L"";

		int requiredSize = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
		if (requiredSize <= 0) return std::nullopt;

		std::wstring wstr(requiredSize, '\0');

		requiredSize = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wstr.data(), requiredSize);
		if (requiredSize <= 0) return std::nullopt;

		return wstr;
	}

	std::optional<std::string> WideToUtf8(const std::wstring& wstr) noexcept
	{
		if (wstr.empty()) return "";

		int requiredSize = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (requiredSize <= 0) return std::nullopt;

		std::string str(requiredSize, '\0');

		requiredSize = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, str.data(), requiredSize, nullptr, nullptr);
		if (requiredSize <= 0) return std::nullopt;

		return str;
	}
}
