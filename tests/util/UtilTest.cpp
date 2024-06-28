/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
	BOOST_CHECK(Util::StrToNum("3.14").value() == 3.14);
	BOOST_CHECK(Util::StrToNum("3.").value() == 3.0);
	BOOST_CHECK(Util::StrToNum("3").value() == 3);
	BOOST_CHECK(Util::StrToNum("3 not a number").has_value() == false);
	BOOST_CHECK(Util::StrToNum("not a number").has_value() == false);
}

BOOST_AUTO_TEST_CASE(StrCaseCmpTest)
{
	BOOST_CHECK(Util::StrCaseCmp("OPTIONS", "options"));
	BOOST_CHECK(Util::StrCaseCmp("OPTIONS", "opt1ons") == false);
	BOOST_CHECK(Util::StrCaseCmp("0PTIONS", "0ptions"));
	BOOST_CHECK(Util::StrCaseCmp("", ""));
	BOOST_CHECK(Util::StrCaseCmp("3.14", "3.12") == false);
}
