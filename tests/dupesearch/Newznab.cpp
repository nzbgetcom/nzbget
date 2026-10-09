/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Denis <denis@nzbget.com>
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
#include <filesystem>
#include <fstream>
#include <sstream>
#include "Newznab.h"
#include "XmlReader.h"
#include "NzbReader.h"
#include "ReleaseName.h"

namespace fs = std::filesystem;

namespace
{

std::string ReadFixture(const char* name)
{
	std::ifstream file((fs::current_path() / "dupesearch" / name).string(), std::ios::binary);
	BOOST_REQUIRE_MESSAGE(file.good(), "cannot open " << name);
	std::stringstream stream;
	stream << file.rdbuf();
	return stream.str();
}

std::string Describe(const Newznab::Params& params)
{
	std::string text;
	for (const auto& param : params)
	{
		text += (text.empty() ? "" : "&") + param.first + "=" + param.second;
	}
	return text;
}

}

BOOST_AUTO_TEST_SUITE(DupeSearchTest)

// a real NZBHydra2 t=search page with 25 items, apikeys masked
BOOST_AUTO_TEST_CASE(NewznabParseSearchPageTest)
{
	Newznab::Page page;
	BOOST_REQUIRE(Newznab::ParseResponse(ReadFixture("search_industry_s03e07.xml"), page));
	BOOST_CHECK(!page.error);
	BOOST_REQUIRE_EQUAL(page.items.size(), 25U);

	const Newznab::Result& first = page.items[0];
	BOOST_CHECK_EQUAL(first.title, "Industry.S03E07.Useful.Idiot.2160p.MAX.WEB-DL.DDP5.1.Atmos.DV.H.265-FLUX");
	BOOST_CHECK_EQUAL(first.link, "http://127.0.0.1:5076/getnzb/api/2114962605753885140?apikey=APIKEY");
	BOOST_CHECK_EQUAL(first.size, 10896716818LL);
	BOOST_CHECK_EQUAL(first.grabs, 111);
	BOOST_CHECK_EQUAL(first.date, 1749517805);	// Tue, 10 Jun 2025 01:10:05 +0000
	BOOST_CHECK_EQUAL(first.indexer, "altHUB");

	const Newznab::Result& second = page.items[1];
	BOOST_CHECK_EQUAL(second.title, "Industry.S03E07.Useful.Idiot.2160p.MAX.WEB-DL.DDP5.1.Atmos.DV.HDR.H.265-FLUX");
	BOOST_CHECK_EQUAL(second.size, 10893813254LL);
	BOOST_CHECK_EQUAL(second.grabs, 0);
	BOOST_CHECK_EQUAL(second.date, 1728651862);
	BOOST_CHECK_EQUAL(second.indexer, "Square Eyed");

	const Newznab::Result& last = page.items[24];
	BOOST_CHECK_EQUAL(last.title, "Industry S03E07 Useful Idiot 2160p MAX WEB-DL DDP5 1 Atmos DV H 265-FLUX");
	BOOST_CHECK_EQUAL(last.size, 11692039895LL);
	BOOST_CHECK_EQUAL(last.grabs, 6);
	BOOST_CHECK_EQUAL(last.indexer, "miatrix");

	// two listings carry no usenet date
	BOOST_CHECK_EQUAL(page.items[5].date, 0);
	BOOST_CHECK_EQUAL(page.items[10].date, 0);
	BOOST_CHECK_GT(page.items[0].date, 0);
}

BOOST_AUTO_TEST_CASE(NewznabParseErrorTest)
{
	Newznab::Page page;
	BOOST_REQUIRE(Newznab::ParseResponse(ReadFixture("error_bad_apikey.xml"), page));
	BOOST_CHECK(page.error);
	BOOST_CHECK_EQUAL(page.errorCode, 100);
	BOOST_CHECK_EQUAL(page.errorText, "Wrong api key");
	BOOST_CHECK(page.items.empty());

	// the replies of an indexer at its grab limit
	BOOST_REQUIRE(Newznab::ParseResponse(
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"429\" description=\"Request limit reached\"/>", page));
	BOOST_CHECK(page.error);
	BOOST_CHECK_EQUAL(page.errorCode, 429);
}

BOOST_AUTO_TEST_CASE(NewznabParseSizeAndMalformedTest)
{
	// the size attribute wins, the <size> element is the fallback
	Newznab::Page page;
	BOOST_REQUIRE(Newznab::ParseResponse(
		"<rss xmlns:newznab=\"n\"><channel>"
		"<item><title> A </title><link>http://x/1</link><size>100</size></item>"
		"<item><title>B</title><link>http://x/2</link><size>100</size><newznab:attr name=\"size\" value=\"200\"/></item>"
		"<item><title>C</title><link>http://x/3</link></item>"
		"</channel></rss>", page));
	BOOST_REQUIRE_EQUAL(page.items.size(), 3U);
	BOOST_CHECK_EQUAL(page.items[0].title, "A");
	BOOST_CHECK_EQUAL(page.items[0].size, 100);
	BOOST_CHECK_EQUAL(page.items[1].size, 200);
	BOOST_CHECK_EQUAL(page.items[2].size, 0);

	BOOST_CHECK(!Newznab::ParseResponse("", page));
	BOOST_CHECK(!Newznab::ParseResponse("<html><body>Forbidden", page));
	BOOST_CHECK(!Newznab::ParseResponse("<rss><channel><item>", page));
}

BOOST_AUTO_TEST_CASE(NewznabRejectsEntityDeclarationsTest)
{
	// billion laughs: a document that declares entities is refused
	std::string bomb = "<?xml version=\"1.0\"?><!DOCTYPE n [<!ENTITY a \"aaaa\"><!ENTITY b \"&a;&a;&a;&a;\">]>"
		"<rss><channel><item><title>&b;</title><link>http://x/1</link></item></channel></rss>";
	Newznab::Page page;
	BOOST_CHECK(!Newznab::ParseResponse(bomb, page));

	// a standard doctype without entities is fine
	BOOST_CHECK(Newznab::ParseResponse(
		"<?xml version=\"1.0\"?><!DOCTYPE rss PUBLIC \"-//x//DTD//EN\" \"http://x/dtd\">"
		"<rss><channel><item><title>T</title><link>http://x/1</link></item></channel></rss>", page));
	BOOST_CHECK_EQUAL(page.items.size(), 1U);
}

BOOST_AUTO_TEST_CASE(NewznabParseDateTest)
{
	BOOST_CHECK_EQUAL(Newznab::ParseDate("Tue, 10 Jun 2025 01:10:05 +0000"), 1749517805);
	BOOST_CHECK_EQUAL(Newznab::ParseDate("Thu, 01 Jan 1970 00:00:00 GMT"), 0 + 0);
	BOOST_CHECK_EQUAL(Newznab::ParseDate("Thu, 01 Jan 1970 01:00:00 +0100"), 0);
	BOOST_CHECK_EQUAL(Newznab::ParseDate("10 Jun 2025 01:10:05 -0200"), 1749517805 + 7200);
	BOOST_CHECK_EQUAL(Newznab::ParseDate("Sun, 22 Sep 2024 00:00:00 +0000"), 1726963200);

	// unreadable: 0
	// B14: RFC 822 zone names, 2-digit years (RFC 2822: below 50 is 20xx) and ISO 8601
	BOOST_CHECK_EQUAL(Newznab::ParseDate("10 Jun 2025 01:10:05 EST"), 1749517805 + 5 * 3600);
	BOOST_CHECK_EQUAL(Newznab::ParseDate("10 Jun 2025 01:10:05 EDT"), 1749517805 + 4 * 3600);
	BOOST_CHECK_EQUAL(Newznab::ParseDate("10 Jun 2025 01:10:05 PST"), 1749517805 + 8 * 3600);
	BOOST_CHECK_EQUAL(Newznab::ParseDate("10 Jun 2025 01:10:05 CDT"), 1749517805 + 5 * 3600);
	BOOST_CHECK_EQUAL(Newznab::ParseDate("10 Jun 2025 01:10:05 UT"), 1749517805);
	BOOST_CHECK_EQUAL(Newznab::ParseDate("10 Jun 2025 01:10:05 Z"), 1749517805);
	BOOST_CHECK_EQUAL(Newznab::ParseDate("Tue, 10 Jun 25 01:10:05 +0000"), 1749517805);
	BOOST_CHECK_EQUAL(Newznab::ParseDate("2025-06-10T01:10:05Z"), 1749517805);
	BOOST_CHECK_EQUAL(Newznab::ParseDate("2025-06-10T03:10:05+02:00"), 1749517805);
	BOOST_CHECK_EQUAL(Newznab::ParseDate("2025-06-10 01:10:05"), 1749517805);
	BOOST_CHECK_EQUAL(Newznab::ParseDate("10 Jun 2025 01:10:05 XYZ"), 0);	// an unknown zone isn't guessed

	BOOST_CHECK_EQUAL(Newznab::ParseDate(""), 0);
	BOOST_CHECK_EQUAL(Newznab::ParseDate("yesterday"), 0);
	BOOST_CHECK_EQUAL(Newznab::ParseDate("Tue, 10 Foo 2025 01:10:05 +0000"), 0);
	BOOST_CHECK_EQUAL(Newznab::ParseDate("Tue, 40 Jun 2025 01:10:05 +0000"), 0);
}

BOOST_AUTO_TEST_CASE(NewznabBuildQueriesTest)
{
	std::string title = "Lucifer.S02E14.Candy.Morningstar.1080p.DTS-HD.MA.5.1.AVC.REMUX-FraMeSToR";

	// the normalized title, the short form, the short form with the group
	std::vector<Newznab::Params> queries = Newznab::BuildQueries(title, "", "");
	BOOST_REQUIRE_EQUAL(queries.size(), 3U);
	BOOST_CHECK_EQUAL(Describe(queries[0]), "t=search&q=lucifer s02e14 candy morningstar 1080p dts hd ma 5 1 avc remux framestor");
	BOOST_CHECK_EQUAL(Describe(queries[1]), "t=search&q=lucifer s02e14 1080p");
	BOOST_CHECK_EQUAL(Describe(queries[2]), "t=search&q=lucifer s02e14 1080p framestor");

	// ids from the nzb-file add a movie and a tv search
	queries = Newznab::BuildQueries(title, "tt1234567", "tvdb-371796");
	BOOST_REQUIRE_EQUAL(queries.size(), 5U);
	BOOST_CHECK_EQUAL(Describe(queries[3]), "t=movie&imdbid=1234567");
	BOOST_CHECK_EQUAL(Describe(queries[4]), "t=tvsearch&tvdbid=371796&season=2&ep=14");

	// no episode: no tv search; a short query equal to the full one is not repeated; no group: no group query
	queries = Newznab::BuildQueries("Some.Movie.2020", "", "55");
	BOOST_REQUIRE_EQUAL(queries.size(), 1U);
	BOOST_CHECK_EQUAL(Describe(queries[0]), "t=search&q=some movie 2020");
	queries = Newznab::BuildQueries("Some.Movie.2020.Extended.2160p.UHD-GRP", "", "");
	BOOST_REQUIRE_EQUAL(queries.size(), 3U);
	BOOST_CHECK_EQUAL(Describe(queries[1]), "t=search&q=some movie 2020 2160p");
	BOOST_CHECK_EQUAL(Describe(queries[2]), "t=search&q=some movie 2020 2160p grp");
	queries = Newznab::BuildQueries("NoMarkers-GRP", "", "");
	BOOST_REQUIRE_EQUAL(queries.size(), 1U);
}

BOOST_AUTO_TEST_CASE(NewznabBuildUrlAndMaskTest)
{
	Newznab::Params params = { { "t", "search" }, { "q", "show s01e02 1080p" }, { "limit", "100" } };
	std::string url = Newznab::BuildUrl("http://127.0.0.1:5076/api", params, "k&y 1");
	BOOST_CHECK_EQUAL(url, "http://127.0.0.1:5076/api?t=search&q=show+s01e02+1080p&limit=100&apikey=k%26y+1");
	BOOST_CHECK_EQUAL(Newznab::BuildUrl("http://h/api?x=1", {}, "key"), "http://h/api?x=1&apikey=key");
	// an address without a path is the indexer's web page (B91): its api is at /api
	BOOST_CHECK_EQUAL(Newznab::BuildUrl("http://h:5076", {}, "key"), "http://h:5076/api?apikey=key");
	BOOST_CHECK_EQUAL(Newznab::BuildUrl("http://h:5076/", {}, "key"), "http://h:5076/api?apikey=key");
	BOOST_CHECK_EQUAL(Newznab::BuildUrl("https://h?x=1", {}, "key"), "https://h/api?x=1&apikey=key");
	BOOST_CHECK_EQUAL(Newznab::BuildUrl("http://h/nzbhydra/api", {}, "key"), "http://h/nzbhydra/api?apikey=key");

	BOOST_CHECK_EQUAL(Newznab::Mask("http://h/api?t=search&apikey=SECRET&q=x"), "http://h/api?t=search&apikey=***&q=x");
	BOOST_CHECK_EQUAL(Newznab::Mask("http://h/api?APIKEY=SECRET"), "http://h/api?APIKEY=***");
	BOOST_CHECK_EQUAL(Newznab::Mask("http://u:p@h/jsonrpc /admin:pw/jsonrpc"), "http://***@h/jsonrpc /admin:pw/jsonrpc");
	BOOST_CHECK_EQUAL(Newznab::Mask("nothing to hide"), "nothing to hide");
}

BOOST_AUTO_TEST_CASE(HostileLengthsTest)
{
	// std::regex recurses once per repeated character: indexer strings of a
	// megabyte ran the thread out of stack. They're cut or refused first
	std::string spaces(1000000, ' ');
	BOOST_CHECK_EQUAL(Newznab::ParseDate(spaces + "2025-06-10T01:10:05Z"), 0);
	std::string longKey = "apikey=" + std::string(1000000, 'k');
	BOOST_CHECK(Newznab::Mask(longKey).size() <= 1024);
	std::string title = "Some.Release.2025.1080p-rakuv" + std::string(1000000, 'a');
	BOOST_CHECK(ReleaseName::Clean(title).size() <= ReleaseName::MaxNameLength);
	ReleaseName::Parse(title);
}

BOOST_AUTO_TEST_CASE(NzbReaderMessageIdTest)
{
	// a message-id with a line break would write a second command into the
	// STAT batch: it isn't taken
	std::string nzb =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<nzb xmlns=\"http://www.newzbin.com/DTD/2003/nzb\">\n"
		"<file poster=\"p\" date=\"1\" subject=\"&quot;a.rar&quot; yEnc (1/3)\">\n"
		"<groups><group>alt.binaries.test</group></groups>\n<segments>\n"
		"<segment bytes=\"100\" number=\"1\">good1@test</segment>\n"
		"<segment bytes=\"100\" number=\"2\">bad&#13;&#10;QUIT&#13;&#10;x@test</segment>\n"
		"<segment bytes=\"100\" number=\"3\">bad space@test</segment>\n"
		"</segments>\n</file>\n</nzb>\n";
	NzbSummary info;
	BOOST_REQUIRE(NzbReader::Parse(nzb, info));
	BOOST_REQUIRE_EQUAL(info.messageIds.size(), 1);
	BOOST_CHECK_EQUAL(info.messageIds[0], "good1@test");
}

BOOST_AUTO_TEST_SUITE_END()
