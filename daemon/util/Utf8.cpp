
/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
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

		if (requiredSize <= STACK_BUFFER_SIZE)
		{
			wchar_t buffer[STACK_BUFFER_SIZE];
			int result = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buffer, STACK_BUFFER_SIZE);
			if (result <= 0) return std::nullopt;
			return std::wstring(buffer);
		}

		auto buffer = std::make_unique<wchar_t[]>(requiredSize);
		int result = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buffer.get(), requiredSize);
		if (result <= 0) return std::nullopt;
		return std::wstring(buffer.get());
	}

	std::optional<std::string> WideToUtf8(const std::wstring& wstr)
	{
		if (wstr.empty()) return "";

		int requiredSize = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (requiredSize <= 0) return std::nullopt;

		if (requiredSize <= STACK_BUFFER_SIZE)
		{
			char buffer[STACK_BUFFER_SIZE];
			int result = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buffer, STACK_BUFFER_SIZE, nullptr, nullptr);
			if (result <= 0) return std::nullopt;
			return std::string(buffer);
		}

		auto buffer = std::make_unique<char[]>(requiredSize);
		int result = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buffer.get(), requiredSize, nullptr, nullptr);
		if (result <= 0) return std::nullopt;
		return std::string(buffer.get());
	}
}
