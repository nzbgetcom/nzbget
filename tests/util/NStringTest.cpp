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

#include <boost/test/unit_test.hpp>
#include <NString.h>

BOOST_AUTO_TEST_CASE(BStringTest)
{
	BString<100> str;
	BOOST_CHECK(sizeof(str) == sizeof(char[100]));
	BOOST_CHECK(str.Empty());
	BOOST_CHECK(str);
	str = "Hello, world";
	BOOST_CHECK(!str.Empty());
	BOOST_CHECK(str);
	BOOST_CHECK(!strcmp(str, "Hello, world"));

	str.Format("Hi, %s%c: %i", "World", '!', 21);
	BOOST_CHECK(!strcmp(str, "Hi, World!: 21"));

	BString<20> str2;
	str2 = "Hello, world 01234567890123456789";
	BOOST_CHECK(!strcmp(str2, "Hello, world 012345"));

	str2.Format("0123456789 Hi, %s%c: %i", "World", '!', 21);
	BOOST_CHECK(!strcmp(str2, "0123456789 Hi, Worl"));

	BString<20> str3;
	memcpy(str3, "Hello, 0123456789 world", str3.Capacity());
	str3[str3.Capacity()] = '\0';
	BOOST_CHECK(!strcmp(str3, "Hello, 0123456789 w"));

	str3 = "String 3 Test World, Hello!";
	BOOST_CHECK(!strcmp(str3, "String 3 Test World"));

	BString<100> str4;
	str4 = "String 4 initialized";
	BOOST_CHECK(!strcmp(str4, "String 4 initialized"));

	BString<20> str5("Hi, %s%c: %i", "World", '!', 21);
	BOOST_CHECK(!strcmp(str5, "Hi, World!: 21"));

	BString<20> str6;
	str6.Append("Hello, World");
	str6.Append("String5String5");
	str6.Append("67");
	str6.Append("0123456789", 5);
	BOOST_CHECK(!strcmp(str6, "Hello, WorldString5"));

	BString<20> str7;
	str7.Append("0123456789", 5);
	BOOST_CHECK(!strcmp(str7, "01234"));

	BString<20> str8;
	str8.Append("0123456789", 5);
	str8.AppendFmt("%i:%i", 87, 65);
	BOOST_CHECK(!strcmp(str8, "0123487:65"));

	const char* txt = "String 9 initialized";
	BString<100> str9 = txt;
	BOOST_CHECK(!strcmp(str9, "String 9 initialized"));
}

BOOST_AUTO_TEST_CASE(CStringTest)
{
	CString str;
	BOOST_CHECK(sizeof(str) == sizeof(char*));
	BOOST_CHECK(str.Empty());
	BOOST_CHECK(!str);
	str = "Hello, world";
	BOOST_CHECK(!str.Empty());
	BOOST_CHECK(str);
	BOOST_CHECK(!strcmp(str, "Hello, world"));

	str.Format("Hi, %s%c: %i", "World", '!', 21);
	BOOST_CHECK(!strcmp(str, "Hi, World!: 21"));

	char* tmp = strdup("Hello there");
	CString str2;
	str2.Bind(tmp);
	const char* tmp3 = *str2;
	BOOST_CHECK(tmp == tmp3);
	BOOST_CHECK(!strcmp(str2, "Hello there"));
	BOOST_CHECK(tmp == *str2);
	free(tmp);

	char* tmp2 = str2.Unbind();
	BOOST_CHECK(tmp2 == tmp);
	BOOST_CHECK(str2.Empty());
	BOOST_CHECK(*str2 == nullptr);

	CString str3("World 12345678901234567890");
	char buf[50];
	snprintf(buf, sizeof(buf), "Hi, %s%c: %i", *str3, '!', 21);
	BOOST_CHECK(!strcmp(buf, "Hi, World 12345678901234567890!: 21"));

	CString str4;
	BOOST_CHECK(*str4 == nullptr);
	BOOST_CHECK((char*)str4 == nullptr);
	BOOST_CHECK((const char*)str4 == nullptr);
	BOOST_CHECK(str4.Str() != nullptr);
	BOOST_CHECK(*str4.Str() == '\0');

	CString str6;
	str6.Append("");
	str6.Append("Hello, World");
	str6.Append("String5String5");
	str6.Append("67");
	BOOST_CHECK(!strcmp(str6, "Hello, WorldString5String567"));

	str6.Clear();
	str6.Append("0123456789", 5);
	str6.AppendFmt("%i:%i", 87, 65);
	BOOST_CHECK(!strcmp(str6, "0123487:65"));

	std::vector<CString> vec1;
	vec1.push_back("Hello, there");
	CString& str7 = vec1.back();
	BOOST_CHECK(!strcmp(str7, "Hello, there"));

	BOOST_CHECK(!strcmp(CString::FormatStr("Many %s ago", "days"), "Many days ago"));

	CString str8("Hello, World");
	str8.Replace(1, 4, "i");
	BOOST_CHECK(!strcmp(str8, "Hi, World"));
	str8.Replace(4, 5, "everybody");
	BOOST_CHECK(!strcmp(str8, "Hi, everybody"));
	str8.Replace(4, 5, "nome", 2);
	BOOST_CHECK(!strcmp(str8, "Hi, nobody"));

	CString str9 = " Long string with spaces \t\r\n ";
	str9.TrimRight();
	BOOST_CHECK(!strcmp(str9, " Long string with spaces"));
	int pos = str9.Find("with");
	BOOST_CHECK(pos == 13);
	str9.Replace("string", "with");
	BOOST_CHECK(!strcmp(str9, " Long with with spaces"));
	str9.Replace("with", "without");
	BOOST_CHECK(!strcmp(str9, " Long without without spaces"));
	str9.Replace("without", "");
	BOOST_CHECK(!strcmp(str9, " Long   spaces"));

	str8 = "test string";
	str9 = "test string";
	BOOST_CHECK(str8 == str9);
	bool eq = str8 == "test string";
	BOOST_CHECK(eq);
}

BOOST_AUTO_TEST_CASE(StringBuilderTest)
{
	StringBuilder str6;
	str6.Append("");
	str6.Append("Hello, World");
	str6.Append("String5String5");
	str6.Append("67");
	BOOST_CHECK(!strcmp(str6, "Hello, WorldString5String567"));
}

BOOST_AUTO_TEST_CASE(CharBufferTest)
{
	CharBuffer buf(1024);
	BOOST_CHECK(buf.Size() == 1024);
	buf.Reserve(2048);
	BOOST_CHECK(buf.Size() == 2048);
	buf.Clear();
	BOOST_CHECK(buf.Size() == 0);
}
