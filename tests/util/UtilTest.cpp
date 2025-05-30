/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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

#include <boost/test/unit_test.hpp>
#include <limits>
#include "Util.h"

BOOST_AUTO_TEST_CASE(XmlStripTagsTest)
{
	const char* xml  = "<div><img style=\"margin-left:10px;margin-bottom:10px;float:right;\" src=\"https://xxx/cover.jpg\"/><ul><li>ID: 12345678</li><li>Name: <a href=\"https://xxx/12344\">Show name</a></li><li>Size: 3.00 GB </li><li>Attributes: Category - <a href=\"https://xxx/2040\">Movies > HD</a></li></li></ul></div>";
	const char* text = "                                                                                                        ID: 12345678         Name:                             Show name             Size: 3.00 GB          Attributes: Category -                            Movies > HD                         ";
	char* testString = strdup(xml);
	WebUtil::XmlStripTags(testString);

	BOOST_CHECK(strcmp(testString, text) == 0);

	free(testString);
}

BOOST_AUTO_TEST_CASE(XmlDecodeTest)
{
	const char* xml  = "Poster: Bob &lt;bob@home&gt; bad&mdash;and there&#039;s one thing";
	const char* text = "Poster: Bob <bob@home> bad&mdash;and there's one thing";
	char* testString = strdup(xml);
	WebUtil::XmlDecode(testString);

	BOOST_CHECK(strcmp(testString, text) == 0);

	free(testString);
}

BOOST_AUTO_TEST_CASE(XmlRemoveEntitiesTest)
{
	const char* xml  = "Poster: Bob &lt;bob@home&gt; bad&mdash;and there&#039;s one thing";
	const char* text = "Poster: Bob  bob@home  bad and there s one thing";
	char* testString = strdup(xml);
	WebUtil::XmlRemoveEntities(testString);

	BOOST_CHECK(strcmp(testString, text) == 0);

	free(testString);
}

BOOST_AUTO_TEST_CASE(URLEncodeTest)
{
	const char* badUrl = "http://www.example.com/nzb_get/12344/Debian V7 6 64 bit OS.nzb";
	const char* correctedUrl = "http://www.example.com/nzb_get/12344/Debian%20V7%206%2064%20bit%20OS.nzb";
	CString testString = WebUtil::UrlEncode(badUrl);

	BOOST_CHECK(strcmp(testString, correctedUrl) == 0);
}

BOOST_AUTO_TEST_CASE(WildMaskTest)
{
	WildMask mask("*.par2", true);
	BOOST_CHECK(mask.Match("Debian V7 6 64 bit OS.nzb") == false);
	BOOST_CHECK(mask.Match("Debian V7 6 64 bit OS.par2.nzb") == false);
	BOOST_CHECK(mask.Match("Debian V7 6 64 bit OS.par2"));
	BOOST_CHECK(mask.Match(".par2"));
	BOOST_CHECK(mask.Match("par2") == false);
}

BOOST_AUTO_TEST_CASE(RegExTest)
{
	RegEx regExRar(".*\\.rar$");
	RegEx regExRarMultiSeq(".*\\.[r-z][0-9][0-9]$");
	RegEx regExSevenZip(".*\\.7z$|.*\\.7z\\.[0-9]+$");
	RegEx regExNumExt(".*\\.[0-9]+$");

	BOOST_CHECK(regExRar.Match("filename.rar"));
	BOOST_CHECK(regExRar.Match("filename.part001.rar"));
	BOOST_CHECK(regExRar.Match("filename.rar.txt") == false);

	BOOST_CHECK(regExRarMultiSeq.Match("filename.rar") == false);
	BOOST_CHECK(regExRarMultiSeq.Match("filename.r01"));
	BOOST_CHECK(regExRarMultiSeq.Match("filename.r99"));
	BOOST_CHECK(regExRarMultiSeq.Match("filename.r001") == false);
	BOOST_CHECK(regExRarMultiSeq.Match("filename.s01"));
	BOOST_CHECK(regExRarMultiSeq.Match("filename.t99"));

	BOOST_CHECK(regExSevenZip.Match("filename.7z"));
	BOOST_CHECK(regExSevenZip.Match("filename.7z.rar") == false);
	BOOST_CHECK(regExSevenZip.Match("filename.7z.1"));
	BOOST_CHECK(regExSevenZip.Match("filename.7z.001"));
	BOOST_CHECK(regExSevenZip.Match("filename.7z.123"));
	BOOST_CHECK(regExSevenZip.Match("filename.7z.999"));

	BOOST_CHECK(regExNumExt.Match("filename.7z.1"));
	BOOST_CHECK(regExNumExt.Match("filename.7z.9"));
	BOOST_CHECK(regExNumExt.Match("filename.7z.001"));
	BOOST_CHECK(regExNumExt.Match("filename.7z.123"));
	BOOST_CHECK(regExNumExt.Match("filename.7z.999"));

	const char* testStr = "My.Show.Name.S01E02.ABC.720";
	RegEx seasonEpisode(".*S([0-9]+)E([0-9]+).*");
	BOOST_CHECK(seasonEpisode.IsValid());
	BOOST_CHECK(seasonEpisode.Match(testStr));
	BOOST_CHECK(seasonEpisode.GetMatchCount() == 3);
	BOOST_CHECK(seasonEpisode.GetMatchStart(1) == 14);
	BOOST_CHECK(seasonEpisode.GetMatchLen(1) == 2);
}

BOOST_AUTO_TEST_CASE(StrToNumTest)
{
	BOOST_CHECK_LT(Util::StrToNum<float>("3.14").value() - 3.14f, std::numeric_limits<float>::epsilon());
	BOOST_CHECK_LT(Util::StrToNum<float>("3.14 abc").value() - 3.14f, std::numeric_limits<float>::epsilon());
	BOOST_CHECK_EQUAL(Util::StrToNum<double>("3.").value(), 3.0);
	BOOST_CHECK_EQUAL(Util::StrToNum<int>("3.").value(), 3);
	BOOST_CHECK_EQUAL(Util::StrToNum<int>("3").value(), 3);
	BOOST_CHECK_EQUAL(Util::StrToNum<int>("-3").value(), -3);
	BOOST_CHECK_EQUAL(Util::StrToNum<int>("+3").value(), 3);
	BOOST_CHECK_EQUAL(Util::StrToNum<int>("   3").value(), 3);
	BOOST_CHECK_EQUAL(Util::StrToNum<int>("3   ").value(), 3);
	BOOST_CHECK_EQUAL(Util::StrToNum<int>("3 not a number").value(), 3);
	BOOST_CHECK_EQUAL(Util::StrToNum<uint8_t>("256").has_value(), false);
	BOOST_CHECK_EQUAL(Util::StrToNum<int>("not a number").has_value(), false);
	BOOST_CHECK_EQUAL(Util::StrToNum<float>("").has_value(), false);
	BOOST_CHECK_EQUAL(Util::StrToNum<float>("  ").has_value(), false);
}

BOOST_AUTO_TEST_CASE(StrCaseCmpTest)
{
	BOOST_CHECK(Util::StrCaseCmp("OPTIONS", "options"));
	BOOST_CHECK(Util::StrCaseCmp("OPTIONS", "opt1ons") == false);
	BOOST_CHECK(Util::StrCaseCmp("0PTIONS", "0ptions"));
	BOOST_CHECK(Util::StrCaseCmp("", ""));
	BOOST_CHECK(Util::StrCaseCmp("3.14", "3.12") == false);
}

BOOST_AUTO_TEST_CASE(SplitInt64Test)
{
	{
		uint32 hi, lo;
		int64 value = std::numeric_limits<int64>::max();
		Util::SplitInt64(value, &hi, &lo);
		int64 res = Util::JoinInt64(hi, lo);
		BOOST_CHECK(res == value);
	}

	{
		uint32 hi, lo;
		int64 value = std::numeric_limits<int64>::min();
		Util::SplitInt64(value, &hi, &lo);
		int64 res = Util::JoinInt64(hi, lo);
		BOOST_CHECK(res == value);
	}

	{
		uint32 hi, lo;
		int64 value = 0;
		Util::SplitInt64(value, &hi, &lo);
		int64 res = Util::JoinInt64(hi, lo);
		BOOST_CHECK(res == value);
	}
}

BOOST_AUTO_TEST_CASE(FormatSpeedTest)
{
	int64 speed1GB = 1024ll * 1024 * 1024;
	BOOST_CHECK(Util::FormatSpeed(speed1GB) == "1.00 GB/s");

	int64 speed10GB = 1024ll * 1024 * 1024 * 10;
	BOOST_CHECK(Util::FormatSpeed(speed10GB) == "10.0 GB/s");

	int64 speed100GB = 1024ll * 1024 * 1024 * 100;
	BOOST_CHECK(Util::FormatSpeed(speed100GB) == "100 GB/s");

	int64 speed1MB = 1024ll * 1024;
	BOOST_CHECK(Util::FormatSpeed(speed1MB) == "1.00 MB/s");

	int64 speed10MB = 1024ll * 1024 * 10;
	BOOST_CHECK(Util::FormatSpeed(speed10MB) == "10.0 MB/s");

	int64 speed100MB = 1024ll * 1024 * 100;
	BOOST_CHECK(Util::FormatSpeed(speed100MB) == "100 MB/s");

	int64 speed1KB = 1024;
	BOOST_CHECK(Util::FormatSpeed(speed1KB) == "1 KB/s");

	int64 speed10KB = 1024 * 10;
	BOOST_CHECK(Util::FormatSpeed(speed10KB) == "10 KB/s");

	int64 speed100KB = 1024 * 100;
	BOOST_CHECK(Util::FormatSpeed(speed100KB) == "100 KB/s");
}

BOOST_AUTO_TEST_CASE(SafeIntCastTest)
{
	{
		int64 max = std::numeric_limits<int64>::max();
		int32 res = Util::SafeIntCast<int64, int32>(max);
		BOOST_CHECK(res == 0);
	}

	{
		int64 min = std::numeric_limits<int64>::min();
		int32 res = Util::SafeIntCast<int64, int32>(min);
		BOOST_CHECK(res == 0);
	}

	{
		int64 max = std::numeric_limits<int32>::max();
		int32 res = Util::SafeIntCast<int64, int32>(max);
		BOOST_CHECK(res == std::numeric_limits<int32>::max());
	}

	{
		int64 min = std::numeric_limits<int32>::min();
		int32 res = Util::SafeIntCast<int64, int32>(min);
		BOOST_CHECK(res == std::numeric_limits<int32>::min());
	}

	{
		uint64 max = std::numeric_limits<uint64>::max();
		int32 res = Util::SafeIntCast<uint64, int32>(max);
		BOOST_CHECK(res == 0);
	}

	{
		uint64 min = std::numeric_limits<uint64>::min();
		int32 res = Util::SafeIntCast<uint64, int32>(min);
		BOOST_CHECK(res == 0);
	}

	{
		uint64 max = std::numeric_limits<int32>::max();
		int32 res = Util::SafeIntCast<uint64, int32>(max);
		BOOST_CHECK(res == std::numeric_limits<int32>::max());
	}

	{
		uint64 min = std::numeric_limits<int32>::min();
		int32 res = Util::SafeIntCast<uint64, int32>(min);
		BOOST_CHECK(res == 0);
	}

	{
		int32 max = std::numeric_limits<int32>::max();
		uint32 res = Util::SafeIntCast<int32, uint32>(max);
		BOOST_CHECK(res == std::numeric_limits<int32>::max());
	}

	{
		int32 min = std::numeric_limits<int32>::min();
		uint32 res = Util::SafeIntCast<int32, uint32>(min);
		BOOST_CHECK(res == 0);
	}
}

BOOST_AUTO_TEST_CASE(TrimTest)
{
	{
		std::string str = "  \n\r\ttext \n\r\t";
		Util::TrimLeft(str);
		BOOST_CHECK_EQUAL(str, "text \n\r\t");
	}

	{
		std::string str = "  \n\r\ttext\n\r\t";
		Util::TrimRight(str);
		BOOST_CHECK_EQUAL(str, "  \n\r\ttext");
	}

		{
		std::string str = "  \n\r\ttext \n\r\t";
		Util::TrimLeft(str);
		BOOST_CHECK_EQUAL(str, "text \n\r\t");
	}

	{
		std::string str = "  \n\r\ttext\n\r\t  ";
		Util::Trim(str);
		BOOST_CHECK_EQUAL(str, "text");
	}

	{
		std::string str = "";
		Util::Trim(str);
		BOOST_CHECK_EQUAL(str, "");
	}
}
