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


#ifndef UTF8_H
#define UTF8_H

#include <string>

namespace Utf8
{
	std::wstring Utf8ToWide(const std::string& str) noexcept;
	std::string WideToUtf8(const std::wstring& wstr) noexcept;

	class WideToUtf8ArgsAdapter final
	{
	public:
		WideToUtf8ArgsAdapter(int argc, wchar_t* wargv[]) noexcept(false);

		char** GetUtf8Args() noexcept;

		WideToUtf8ArgsAdapter() = delete;
		WideToUtf8ArgsAdapter(const WideToUtf8ArgsAdapter&) = delete;
		WideToUtf8ArgsAdapter(WideToUtf8ArgsAdapter&&) = delete;
		WideToUtf8ArgsAdapter& operator=(const WideToUtf8ArgsAdapter&) = delete;
		WideToUtf8ArgsAdapter& operator=(WideToUtf8ArgsAdapter&&) = delete;

		~WideToUtf8ArgsAdapter();

	private:
		char** m_argv;
		int m_argc;
	};
}

#endif
