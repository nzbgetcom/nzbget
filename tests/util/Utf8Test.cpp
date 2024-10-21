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

#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>
#include <exception>

#include "Utf8.h"

using namespace std;
using namespace Utf8;

BOOST_AUTO_TEST_CASE(Utf8Test)
{
	{
		string emptyString = "";
		wstring expectedWide = L"";
		wstring actualWide = Utf8ToWide(emptyString);

		string expectedUtf8 = "";
		string actualUtf8 = WideToUtf8(expectedWide);

		BOOST_CHECK(wcscmp(actualWide.c_str(), expectedWide.c_str()) == 0);
		BOOST_CHECK(strcmp(actualUtf8.c_str(), expectedUtf8.c_str()) == 0);
	}

	{
		string asciiString = "Hello, World!";
		wstring expectedWide = L"Hello, World!";
		wstring actualWide = Utf8ToWide(asciiString);

		string expectedUtf8 = "Hello, World!";
		string actualUtf8 = WideToUtf8(expectedWide);

		BOOST_CHECK(wcscmp(actualWide.c_str(), expectedWide.c_str()) == 0);
		BOOST_CHECK(strcmp(actualUtf8.c_str(), expectedUtf8.c_str()) == 0);
	}

	{
		string nonAsciiString = "Привет, мир!";
		wstring expectedWide = L"Привет, мир!";
		wstring actualWide = Utf8ToWide(nonAsciiString);

		string expectedUtf8 = "Привет, мир!";
		string actualUtf8 = WideToUtf8(expectedWide);

		BOOST_CHECK(wcscmp(actualWide.c_str(), expectedWide.c_str()) == 0);
		BOOST_CHECK(strcmp(actualUtf8.c_str(), expectedUtf8.c_str()) == 0);
	}

	{
		string specialCharsString = "This string has: !@#$%^&*()_+=-`~[]{}:;'<>,.?/";
		wstring expectedWide = L"This string has: !@#$%^&*()_+=-`~[]{}:;'<>,.?/";
		wstring actualWide = Utf8ToWide(specialCharsString);

		string expectedUtf8 = "This string has: !@#$%^&*()_+=-`~[]{}:;'<>,.?/";
		string actualUtf8 = WideToUtf8(expectedWide);

		BOOST_CHECK(wcscmp(actualWide.c_str(), expectedWide.c_str()) == 0);
		BOOST_CHECK(strcmp(actualUtf8.c_str(), expectedUtf8.c_str()) == 0);
	}

	{
		string multiLangString = "Привет! こんにちは世界! 안녕하세요!";
		wstring expectedWide = L"Привет! こんにちは世界! 안녕하세요!";
		wstring actualWide = Utf8ToWide(multiLangString);

		string expectedUtf8 = "Привет! こんにちは世界! 안녕하세요!";
		string actualUtf8 = WideToUtf8(expectedWide);

		BOOST_CHECK(wcscmp(actualWide.c_str(), expectedWide.c_str()) == 0);
		BOOST_CHECK(strcmp(actualUtf8.c_str(), expectedUtf8.c_str()) == 0);
	}

	{
		BOOST_CHECK_EXCEPTION(
			WideToUtf8ArgsAdapter(0, nullptr),
			std::invalid_argument,
			[&](const std::exception& e) {
				return e.what() == std::string("Invalid argument: wargv cannot be nullptr.");
			}
		);
	}

	{
		wchar_t* wargv[1] = { nullptr };
		WideToUtf8ArgsAdapter adapter(0, wargv);
		const char* const* utf8Args = adapter.GetUtf8Args();

		BOOST_CHECK(utf8Args[0] == nullptr);
	}

	{
		wchar_t* wargv[3] = { L"Привет", L"мир", L"!" };
		WideToUtf8ArgsAdapter adapter(3, wargv);
		const char* const* utf8Args = adapter.GetUtf8Args();

		for (int i = 0; i < 3; ++i) {
			std::string arg = WideToUtf8(wargv[i]);
			BOOST_CHECK(strcmp(arg.c_str(), utf8Args[i]) == 0);
		}
	}

	{
		wchar_t* wargv[3] = { L"arg1", nullptr, L"arg3" };
		WideToUtf8ArgsAdapter adapter(3, wargv);
		const char* const* utf8Args = adapter.GetUtf8Args();
		int argc = 3;
		for (int i = 0; i < argc; ++i)
		{
			if (wargv[i] == nullptr)
			{
				--i;
				--argc;
				continue;
			}

			std::string arg = WideToUtf8(wargv[i]);
			BOOST_CHECK(strcmp(arg.c_str(), utf8Args[i]) == 0);
		}
	}
}
