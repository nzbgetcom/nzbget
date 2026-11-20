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

#include <string>
#include <string_view>

#include "daemon/network/http/Parser.h"

using namespace network;

BOOST_AUTO_TEST_SUITE(NetworkTest)

namespace
{

struct Collector final : http::BodySink
{
	std::string data;

	Result<void> OnBody(std::string_view chunk) override
	{
		data.append(chunk);
		return {};
	}
};

/// Feeds `text` in slices of `step` bytes, mimicking arbitrary TCP fragmentation.
Result<void> FeedInSteps(http::Parser& parser, Collector& sink, std::string_view text, size_t step)
{
	for (size_t offset = 0; offset < text.size(); offset += step)
	{
		if (auto fed = parser.Feed(text.substr(offset, step), sink); fed.has_error())
		{
			return fed;
		}
	}
	return {};
}

}

BOOST_AUTO_TEST_CASE(HttpParser_ParsesStatusHeadersAndContentLengthBody)
{
	http::Parser parser;
	Collector sink;

	auto r = parser.Feed("HTTP/1.1 200 OK\r\nContent-Length: 5\r\nX-Test: yes\r\n\r\nhello", sink);
	BOOST_REQUIRE(r.has_value());
	BOOST_CHECK_EQUAL(parser.Head().status, 200u);
	BOOST_CHECK_EQUAL(*parser.Head().Header("x-test"), "yes");
	BOOST_CHECK_EQUAL(sink.data, "hello");
	BOOST_CHECK(parser.Done());
}

BOOST_AUTO_TEST_CASE(HttpParser_HandlesByteByByteInput)
{
	const std::string text = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nabc";

	http::Parser parser;
	Collector sink;
	BOOST_REQUIRE(FeedInSteps(parser, sink, text, 1).has_value());

	BOOST_CHECK_EQUAL(sink.data, "abc");
	BOOST_CHECK(parser.Done());
}

BOOST_AUTO_TEST_CASE(HttpParser_DecodesChunkedBodyWithExtensionsAndTrailers)
{
	const std::string text = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
		"3;ext=1\r\nabc\r\n2\r\nde\r\n0\r\nX-Trailer: 1\r\n\r\n";

	http::Parser parser;
	Collector sink;
	BOOST_REQUIRE(FeedInSteps(parser, sink, text, 7).has_value());

	BOOST_CHECK_EQUAL(sink.data, "abcde");
	BOOST_CHECK(parser.Done());
}

BOOST_AUTO_TEST_CASE(HttpParser_RejectsConflictingContentLengths)
{
	http::Parser parser;
	Collector sink;

	auto r = parser.Feed("HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Length: 6\r\n\r\nhello", sink);
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::ProtocolViolation);
}

BOOST_AUTO_TEST_CASE(HttpParser_RejectsTransferEncodingWithContentLength)
{
	http::Parser parser;
	Collector sink;

	auto r = parser.Feed("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: 5\r\n\r\n0\r\n\r\n", sink);
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::ProtocolViolation);
}

BOOST_AUTO_TEST_CASE(HttpParser_RejectsUnsupportedTransferEncoding)
{
	http::Parser parser;
	Collector sink;

	auto r = parser.Feed("HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip\r\n\r\n", sink);
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::ProtocolViolation);
}

BOOST_AUTO_TEST_CASE(HttpParser_SkipsInterimResponse)
{
	http::Parser parser;
	Collector sink;

	auto r = parser.Feed("HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok", sink);
	BOOST_REQUIRE(r.has_value());
	BOOST_CHECK_EQUAL(parser.Head().status, 200u);
	BOOST_CHECK_EQUAL(sink.data, "ok");
}

BOOST_AUTO_TEST_CASE(HttpParser_FramingWithoutBodyFor204And304And101)
{
	for (unsigned status : {204u, 304u, 101u})
	{
		http::Parser parser;
		Collector sink;
		const auto text = std::format("HTTP/1.1 {} Done\r\n\r\n", status);
		BOOST_REQUIRE(parser.Feed(text, sink).has_value());
		BOOST_CHECK(parser.Done());
		BOOST_CHECK(sink.data.empty());
	}
}

BOOST_AUTO_TEST_CASE(HttpParser_CloseDelimitedBodyCompletesOnFinish)
{
	http::Parser parser;
	Collector sink;

	BOOST_REQUIRE(parser.Feed("HTTP/1.1 200 OK\r\n\r\npart", sink).has_value());
	BOOST_CHECK(!parser.Done());
	BOOST_REQUIRE(parser.Feed("s", sink).has_value());
	BOOST_REQUIRE(parser.Finish().has_value());

	BOOST_CHECK_EQUAL(sink.data, "parts");
	BOOST_CHECK(parser.Done());
}

BOOST_AUTO_TEST_CASE(HttpParser_TruncatedContentLengthFailsOnFinish)
{
	http::Parser parser;
	Collector sink;

	BOOST_REQUIRE(parser.Feed("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhel", sink).has_value());
	auto r = parser.Finish();
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::ConnectionLost);
}

BOOST_AUTO_TEST_CASE(HttpParser_RejectsObsFoldAndBadHeaderNames)
{
	for (const std::string text : {
		"HTTP/1.1 200 OK\r\nX-Test: a\r\n folded\r\n\r\n",
		"HTTP/1.1 200 OK\r\nBad Name: a\r\n\r\n",
		"HTTP/1.1 200 OK\r\n: novalue\r\n\r\n" })
	{
		http::Parser parser;
		Collector sink;
		auto r = parser.Feed(text, sink);
		BOOST_REQUIRE(r.has_error());
		BOOST_CHECK(r.error() == Error::ProtocolViolation);
	}
}

BOOST_AUTO_TEST_CASE(HttpParser_RejectsHeaderBlockOverLimit)
{
	http::Parser parser;
	Collector sink;

	std::string text = "HTTP/1.1 200 OK\r\n";
	text += "X-Big: " + std::string(70000, 'a') + "\r\n\r\n";

	auto r = parser.Feed(text, sink);
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::MessageTooLarge);
}

BOOST_AUTO_TEST_CASE(HttpParser_RejectsBodyOverLimit)
{
	http::Parser parser(http::Parser::Limits{.maxBodySize = 4});
	Collector sink;

	auto r = parser.Feed("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello", sink);
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::MessageTooLarge);
}

BOOST_AUTO_TEST_CASE(HttpParser_RejectsMalformedChunkSize)
{
	http::Parser parser;
	Collector sink;

	auto r = parser.Feed("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\n", sink);
	BOOST_REQUIRE(r.has_error());
	BOOST_CHECK(r.error() == Error::ProtocolViolation);
}

BOOST_AUTO_TEST_SUITE_END()
