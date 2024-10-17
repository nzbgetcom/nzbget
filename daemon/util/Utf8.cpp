
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

#include <exception>
#include <windows.h>

#include "Utf8.h"
#include "Log.h"

namespace Utf8
{
	constexpr int MAX_ARGS = 128;

	std::optional<std::wstring> Utf8ToWide(const std::string& str) noexcept
	{
		if (str.empty()) return L"";

		int wideStrLen = MultiByteToWideChar(CP_UTF8, 0, str.data(), -1, nullptr, 0);
		if (wideStrLen < 1) return std::nullopt;
	
		std::wstring wstr(wideStrLen, 0);
		wideStrLen = MultiByteToWideChar(CP_UTF8, 0, str.data(), -1, wstr.data(), wideStrLen);
		if (wideStrLen < 1) return std::nullopt;

		return wstr;
	}

	std::optional<std::string> WideToUtf8(const std::wstring& wstr) noexcept
	{
		if (wstr.empty()) return "";

		int utf8StrLen = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), wstr.size(), nullptr, 0, nullptr, nullptr);
		if (utf8StrLen < 1) return std::nullopt;
	
		std::string str(utf8StrLen, 0);

		utf8StrLen = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), wstr.size(), str.data(), utf8StrLen, nullptr, nullptr);
		if (utf8StrLen < 1) return std::nullopt;
	
		return str;
	}

	WideToUtf8ArgsAdapter::WideToUtf8ArgsAdapter(int argc, wchar_t* wargv[]) noexcept(false)
		: m_argc(argc)
	{
		if (wargv == nullptr)
		{
			throw std::invalid_argument("Invalid argument: wargv cannot be nullptr.");
		}

		if (m_argc > MAX_ARGS)
		{
			warn("Too many arguments (%i/%i).\nOnly %i will be processed.", argc, MAX_ARGS, MAX_ARGS);
			m_argc = MAX_ARGS;
		}

		m_argv = new char* [m_argc + 1];
		for (int i = 0; i < m_argc; ++i)
		{
			if (wargv[i] == nullptr)
			{
				warn("Invalid argument: encountered nullptr in wargv.\nSkipping %i argument.", i);
				--m_argc;
				--i;
				continue;
			}

			std::string arg = WideToUtf8(wargv[i]).value();
			size_t size = arg.size() + 1;
			m_argv[i] = new char[size];
			std::strcpy(m_argv[i], arg.c_str());
		}
		m_argv[m_argc] = nullptr;
	}

	char** WideToUtf8ArgsAdapter::GetUtf8Args() noexcept
	{
		return m_argv;
	}

	WideToUtf8ArgsAdapter::~WideToUtf8ArgsAdapter()
	{
		if (m_argv)
		{
			for (int i = 0; i < m_argc; ++i)
			{
				delete m_argv[i];
			}
			delete[] m_argv;
		}
	}
}
