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

#include "Utf8.h"

using namespace std;
using namespace Utf8;

BOOST_AUTO_TEST_CASE(Utf8Test)
{
	{
		string emptyString = "";
		wstring expectedWide = L"";
		wstring actualWide = Utf8ToWide(emptyString).value();

		string expectedUtf8 = "";
		string actualUtf8 = WideToUtf8(expectedWide).value();

		BOOST_CHECK(wcscmp(actualWide.c_str(), expectedWide.c_str()) == 0);
		BOOST_CHECK(strcmp(actualUtf8.c_str(), expectedUtf8.c_str()) == 0);
	}

	{
		string asciiString = "Hello, World!";
		wstring expectedWide = L"Hello, World!";
		wstring actualWide = Utf8ToWide(asciiString).value();

		string expectedUtf8 = "Hello, World!";
		string actualUtf8 = WideToUtf8(expectedWide).value();

		BOOST_CHECK(wcscmp(actualWide.c_str(), expectedWide.c_str()) == 0);
		BOOST_CHECK(strcmp(actualUtf8.c_str(), expectedUtf8.c_str()) == 0);
	}

	{
		string nonAsciiString = "Привет, мир!";
		wstring expectedWide = L"Привет, мир!";
		wstring actualWide = Utf8ToWide(nonAsciiString).value();

		string expectedUtf8 = "Привет, мир!";
		string actualUtf8 = WideToUtf8(expectedWide).value();

		BOOST_CHECK(wcscmp(actualWide.c_str(), expectedWide.c_str()) == 0);
		BOOST_CHECK(strcmp(actualUtf8.c_str(), expectedUtf8.c_str()) == 0);
	}

	{
		string specialCharsString = "This string has: !@#$%^&*()_+=-`~[]{}:;'<>,.?/";
		wstring expectedWide = L"This string has: !@#$%^&*()_+=-`~[]{}:;'<>,.?/";
		wstring actualWide = Utf8ToWide(specialCharsString).value();

		string expectedUtf8 = "This string has: !@#$%^&*()_+=-`~[]{}:;'<>,.?/";
		string actualUtf8 = WideToUtf8(expectedWide).value();

		BOOST_CHECK(wcscmp(actualWide.c_str(), expectedWide.c_str()) == 0);
		BOOST_CHECK(strcmp(actualUtf8.c_str(), expectedUtf8.c_str()) == 0);
	}

	{
		string multiLangString = "Привет! こんにちは世界! 안녕하세요!";
		wstring expectedWide = L"Привет! こんにちは世界! 안녕하세요!";
		wstring actualWide = Utf8ToWide(multiLangString).value();

		string expectedUtf8 = "Привет! こんにちは世界! 안녕하세요!";
		string actualUtf8 = WideToUtf8(expectedWide).value();

		BOOST_CHECK(wcscmp(actualWide.c_str(), expectedWide.c_str()) == 0);
		BOOST_CHECK(strcmp(actualUtf8.c_str(), expectedUtf8.c_str()) == 0);
	}

	{
		string veryLongString = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. \
		Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. \
		Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi \
		ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit \
		in voluptate velit esse cillum dolore eu fugiat nulla pariatur.";

		wstring expectedWide = L"Lorem ipsum dolor sit amet, consectetur adipiscing elit. \
		Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. \
		Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi \
		ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit \
		in voluptate velit esse cillum dolore eu fugiat nulla pariatur.";
		wstring actualWide = Utf8ToWide(veryLongString).value();

		string expectedUtf8 = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. \
		Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. \
		Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi \
		ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit \
		in voluptate velit esse cillum dolore eu fugiat nulla pariatur.";
		string actualUtf8 = WideToUtf8(expectedWide).value();

		BOOST_CHECK(wcscmp(actualWide.c_str(), expectedWide.c_str()) == 0);
		BOOST_CHECK(strcmp(actualUtf8.c_str(), expectedUtf8.c_str()) == 0);
	}
}
