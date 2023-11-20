/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include <nzbget.h>	

#define BOOST_TEST_MODULE "NStringTest"
#include <boost/test/included/unit_test.hpp>

#include <NString.h>

BOOST_AUTO_TEST_CASE(BStringTest)
{
	BString<100> str;
	BOOST_TEST(sizeof(str) == sizeof(char[100]));
	BOOST_TEST(str.Empty());
	BOOST_TEST(str);
	str = "Hello, world";
	BOOST_TEST(!str.Empty());
	BOOST_TEST(str);
	BOOST_TEST(!strcmp(str, "Hello, world"));

	str.Format("Hi, %s%c: %i", "World", '!', 21);
	BOOST_TEST(!strcmp(str, "Hi, World!: 21"));

	BString<20> str2;
	str2 = "Hello, world 01234567890123456789";
	BOOST_TEST(!strcmp(str2, "Hello, world 012345"));

	str2.Format("0123456789 Hi, %s%c: %i", "World", '!', 21);
	BOOST_TEST(!strcmp(str2, "0123456789 Hi, Worl"));

	BString<20> str3;
	memcpy(str3, "Hello, 0123456789 world", str3.Capacity());
	str3[str3.Capacity()] = '\0';
	BOOST_TEST(!strcmp(str3, "Hello, 0123456789 w"));

	str3 = "String 3 Test World, Hello!";
	BOOST_TEST(!strcmp(str3, "String 3 Test World"));

	BString<100> str4;
	str4 = "String 4 initialized";
	BOOST_TEST(!strcmp(str4, "String 4 initialized"));

	BString<20> str5("Hi, %s%c: %i", "World", '!', 21);
	BOOST_TEST(!strcmp(str5, "Hi, World!: 21"));

	BString<20> str6;
	str6.Append("Hello, World");
	str6.Append("String5String5");
	str6.Append("67");
	str6.Append("0123456789", 5);
	BOOST_TEST(!strcmp(str6, "Hello, WorldString5"));

	BString<20> str7;
	str7.Append("0123456789", 5);
	BOOST_TEST(!strcmp(str7, "01234"));

	BString<20> str8;
	str8.Append("0123456789", 5);
	str8.AppendFmt("%i:%i", 87, 65);
	BOOST_TEST(!strcmp(str8, "0123487:65"));

	const char* txt = "String 9 initialized";
	BString<100> str9 = txt;
	BOOST_TEST(!strcmp(str9, "String 9 initialized"));
}

BOOST_AUTO_TEST_CASE(CStringTest)
{
	CString str;
	BOOST_TEST(sizeof(str) == sizeof(char*));
	BOOST_TEST(str.Empty());
	BOOST_TEST(!str);
	str = "Hello, world";
	BOOST_TEST(!str.Empty());
	BOOST_TEST(str);
	BOOST_TEST(!strcmp(str, "Hello, world"));

	str.Format("Hi, %s%c: %i", "World", '!', 21);
	BOOST_TEST(!strcmp(str, "Hi, World!: 21"));

	char* tmp = strdup("Hello there");
	CString str2;
	str2.Bind(tmp);
	const char* tmp3 = *str2;
	BOOST_TEST(tmp == tmp3);
	BOOST_TEST(!strcmp(str2, "Hello there"));
	BOOST_TEST(tmp == *str2);
	free(tmp);

	char* tmp2 = str2.Unbind();
	BOOST_TEST(tmp2 == tmp);
	BOOST_TEST(str2.Empty());
	BOOST_TEST(*str2 == nullptr);

	CString str3("World 12345678901234567890");
	char buf[50];
	snprintf(buf, sizeof(buf), "Hi, %s%c: %i", *str3, '!', 21);
	BOOST_TEST(!strcmp(buf, "Hi, World 12345678901234567890!: 21"));

	CString str4;
	BOOST_TEST(*str4 == nullptr);
	BOOST_TEST((char*)str4 == nullptr);
	BOOST_TEST((const char*)str4 == nullptr);
	BOOST_TEST(str4.Str() != nullptr);
	BOOST_TEST(*str4.Str() == '\0');

	CString str6;
	str6.Append("");
	str6.Append("Hello, World");
	str6.Append("String5String5");
	str6.Append("67");
	BOOST_TEST(!strcmp(str6, "Hello, WorldString5String567"));

	str6.Clear();
	str6.Append("0123456789", 5);
	str6.AppendFmt("%i:%i", 87, 65);
	BOOST_TEST(!strcmp(str6, "0123487:65"));

	std::vector<CString> vec1;
	vec1.push_back("Hello, there");
	CString& str7 = vec1.back();
	BOOST_TEST(!strcmp(str7, "Hello, there"));

	BOOST_TEST(!strcmp(CString::FormatStr("Many %s ago", "days"), "Many days ago"));

	CString str8("Hello, World");
	str8.Replace(1, 4, "i");
	BOOST_TEST(!strcmp(str8, "Hi, World"));
	str8.Replace(4, 5, "everybody");
	BOOST_TEST(!strcmp(str8, "Hi, everybody"));
	str8.Replace(4, 5, "nome", 2);
	BOOST_TEST(!strcmp(str8, "Hi, nobody"));

	CString str9 = " Long string with spaces \t\r\n ";
	str9.TrimRight();
	BOOST_TEST(!strcmp(str9, " Long string with spaces"));
	int pos = str9.Find("with");
	BOOST_TEST(pos == 13);
	str9.Replace("string", "with");
	BOOST_TEST(!strcmp(str9, " Long with with spaces"));
	str9.Replace("with", "without");
	BOOST_TEST(!strcmp(str9, " Long without without spaces"));
	str9.Replace("without", "");
	BOOST_TEST(!strcmp(str9, " Long   spaces"));

	str8 = "test string";
	str9 = "test string";
	BOOST_TEST(str8 == str9);
	bool eq = str8 == "test string";
	BOOST_TEST(eq);
}

BOOST_AUTO_TEST_CASE(StringBuilderTest)
{
	StringBuilder str6;
	str6.Append("");
	str6.Append("Hello, World");
	str6.Append("String5String5");
	str6.Append("67");
	BOOST_TEST(!strcmp(str6, "Hello, WorldString5String567"));
}

BOOST_AUTO_TEST_CASE(CharBufferTest)
{
	CharBuffer buf(1024);
	BOOST_TEST(buf.Size() == 1024);
	buf.Reserve(2048);
	BOOST_TEST(buf.Size() == 2048);
	buf.Clear();
	BOOST_TEST(buf.Size() == 0);
}
