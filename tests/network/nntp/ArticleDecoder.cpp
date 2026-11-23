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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "nzbget.h"

#include <algorithm>
#include <boost/test/unit_test.hpp>
#include <cstring>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ArticleDecoder.h"
#include "YencFixtures.h"

namespace nntp = network::nntp;

namespace
{

/// Feeds the article in pieces, reusing dest for every call; fails on a stall.
std::string DecodeAll(nntp::ArticleDecoder& decoder, const std::string& article, std::span<char> dest, size_t piece)
{
	std::string out;
	size_t pos = 0;
	while (pos < article.size())
	{
		const size_t take = std::min(piece, article.size() - pos);
		const nntp::DecodeStep step = decoder.Decode(std::span<const char>(article.data() + pos, take), dest);
		out.append(dest.data(), step.produced);
		pos += step.consumed;
		if (step.complete)
		{
			break;
		}
		BOOST_REQUIRE_MESSAGE(step.consumed > 0 || step.produced > 0, "stalled at " << pos << " take=" << take << " dest=" << dest.size() << " article=" << article.size());
	}
	return out;
}

} // namespace

BOOST_AUTO_TEST_SUITE(DecoderSuite)

BOOST_AUTO_TEST_CASE(DetectFormat_RecognizesYencBeginLine)
{
	BOOST_CHECK(nntp::ArticleDecoder::DetectFormat("=ybegin part=1 line=128 size=10 name=a.bin\r\n", false) == nntp::ArticleFormat::Yenc);
	BOOST_CHECK(nntp::ArticleDecoder::DetectFormat("=ybegin ", false) == nntp::ArticleFormat::Yenc);
}

BOOST_AUTO_TEST_CASE(DetectFormat_RecognizesUuencodeLines)
{
	BOOST_CHECK(nntp::ArticleDecoder::DetectFormat("begin 644 cat.txt\r\n", false) == nntp::ArticleFormat::Ux);
	BOOST_CHECK(nntp::ArticleDecoder::DetectFormat("#0V%T\r\n", false) == nntp::ArticleFormat::Ux);
	// A body line cut short does not match its length byte: callers must pass whole lines.
	BOOST_CHECK(nntp::ArticleDecoder::DetectFormat("M" + std::string(60, '#') + "\r\n", false) == nntp::ArticleFormat::Ux);
	BOOST_CHECK(nntp::ArticleDecoder::DetectFormat("M" + std::string(30, '#'), false) == nntp::ArticleFormat::Unknown);
}

BOOST_AUTO_TEST_CASE(DetectFormat_PreambleIsUnknown)
{
	BOOST_CHECK(nntp::ArticleDecoder::DetectFormat("Some preamble text\r\n", false) == nntp::ArticleFormat::Unknown);
	BOOST_CHECK(nntp::ArticleDecoder::DetectFormat("", false) == nntp::ArticleFormat::Unknown);
}

BOOST_AUTO_TEST_CASE(DetectFormat_RawModeIgnoresContent)
{
	BOOST_CHECK(nntp::ArticleDecoder::DetectFormat("=ybegin part=1\r\n", true) == nntp::ArticleFormat::Raw);
	BOOST_CHECK(nntp::ArticleDecoder::DetectFormat("", true) == nntp::ArticleFormat::Raw);
}

BOOST_AUTO_TEST_CASE(DetectFormat_RequiresYencKeywordAtLineStart)
{
	BOOST_CHECK(nntp::ArticleDecoder::DetectFormat(" =ybegin part=1\r\n", false) == nntp::ArticleFormat::Unknown);
	BOOST_CHECK(nntp::ArticleDecoder::DetectFormat("=ybeginX\r\n", false) == nntp::ArticleFormat::Unknown);
	BOOST_CHECK(nntp::ArticleDecoder::DetectFormat("=ybeg\r\n", false) == nntp::ArticleFormat::Unknown);
}

BOOST_AUTO_TEST_CASE(DetectFormat_RequiresLowercaseUuBegin)
{
	BOOST_CHECK(nntp::ArticleDecoder::DetectFormat("BEGIN 644 x\r\n", false) == nntp::ArticleFormat::Unknown);
	BOOST_CHECK(nntp::ArticleDecoder::DetectFormat("begin 644\r\n", false) == nntp::ArticleFormat::Unknown);
}

BOOST_AUTO_TEST_CASE(DetectFormat_EmptyLineIsUnknown)
{
	BOOST_CHECK(nntp::ArticleDecoder::DetectFormat("\r\n", false) == nntp::ArticleFormat::Unknown);
}

BOOST_AUTO_TEST_CASE(Create_SelectsDecoderForFormat)
{
	nntp::ArticleDecoder decoder;
	decoder.Create(nntp::ArticleFormat::Yenc, true);
	BOOST_CHECK(decoder.Meta().format == nntp::ArticleFormat::Yenc);
	decoder.Create(nntp::ArticleFormat::Ux, false);
	BOOST_CHECK(decoder.Meta().format == nntp::ArticleFormat::Ux);
	decoder.Create(nntp::ArticleFormat::Raw, false);
	BOOST_CHECK(decoder.Meta().format == nntp::ArticleFormat::Raw);
}

BOOST_AUTO_TEST_CASE(Create_UnknownDecodesNothing)
{
	nntp::ArticleDecoder decoder;
	decoder.Create(nntp::ArticleFormat::Unknown, false);
	const std::string article = "abc\r\n";
	char out[8];
	const nntp::DecodeStep step = decoder.Decode(article, out);
	BOOST_CHECK_EQUAL(step.produced, 0u);
	BOOST_CHECK_EQUAL(step.consumed, 0u);
	BOOST_CHECK(decoder.Meta().format == nntp::ArticleFormat::Unknown);
	BOOST_CHECK(decoder.Finish() == nntp::DecodeStatus::NoBinaryData);
}

BOOST_AUTO_TEST_CASE(Create_ReusesDecoderForNextArticle)
{
	const std::string payload = yenc_fixtures::MakePayload();
	const std::string article = yenc_fixtures::MakeArticle(payload);
	nntp::ArticleDecoder decoder;

	for (int round = 0; round < 2; round++)
	{
		decoder.Create(nntp::ArticleFormat::Yenc, true);
		std::vector<char> dest(payload.size() + 64);
		BOOST_CHECK(DecodeAll(decoder, article, dest, article.size()) == payload);
		BOOST_CHECK(decoder.Finish() == nntp::DecodeStatus::Finished);
	}
}

BOOST_AUTO_TEST_CASE(Decoder_ProgressesWithMinBufferSize)
{
	BOOST_CHECK_EQUAL(nntp::ArticleDecoder::MinBufferSize, 45u);

	const std::string payload = yenc_fixtures::MakePayload();
	const std::string article = yenc_fixtures::MakeArticle(payload);
	nntp::ArticleDecoder decoder;
	decoder.Create(nntp::ArticleFormat::Yenc, true);
	std::vector<char> dest(nntp::ArticleDecoder::MinBufferSize);

	BOOST_CHECK(DecodeAll(decoder, article, dest, 64) == payload);
	BOOST_CHECK(decoder.Finish() == nntp::DecodeStatus::Finished);
}

BOOST_AUTO_TEST_CASE(Decoder_CrcIsIndependentOfOutputAlignment)
{
	const std::string payload = yenc_fixtures::MakePayload();
	const std::string article = yenc_fixtures::MakeArticle(payload);

	for (size_t offset = 0; offset < 8; offset++)
	{
		nntp::ArticleDecoder decoder;
		decoder.Create(nntp::ArticleFormat::Yenc, true);
		std::vector<char> buf(payload.size() + 16);
		const std::span<char> dest(buf.data() + offset, buf.size() - offset);
		BOOST_CHECK_MESSAGE(DecodeAll(decoder, article, dest, article.size()) == payload, "offset=" << offset);
		BOOST_CHECK_MESSAGE(decoder.Finish() == nntp::DecodeStatus::Finished, "offset=" << offset);
	}
}

BOOST_AUTO_TEST_CASE(FixedBuffer_FillsCapacityAndAcceptsEmptyData)
{
	nntp::FixedBuffer<4> buf;
	buf.Assign("");
	BOOST_CHECK_EQUAL(buf.Size(), 0u);
	buf.Append("ab");
	buf.Append("");
	buf.Append("cd");
	BOOST_CHECK(buf.View() == "abcd");
	buf.Assign("xy");
	BOOST_CHECK(buf.View() == "xy");
	buf.Clear();
	BOOST_CHECK_EQUAL(buf.Size(), 0u);
}

BOOST_AUTO_TEST_CASE(FixedBuffer_RejectsOverflow)
{
	nntp::FixedBuffer<4> buf;
	BOOST_TEST(buf.Append("ab"));
	BOOST_TEST(!buf.Append("cde"));
	BOOST_CHECK(buf.View() == "ab");
	BOOST_TEST(buf.Append("cd"));
	BOOST_CHECK(buf.View() == "abcd");
	BOOST_TEST(!buf.Assign("abcde"));
	BOOST_CHECK_EQUAL(buf.Size(), 0u);
}

BOOST_AUTO_TEST_CASE(Create_SwitchesFormatsBetweenArticles)
{
	const std::string payload = yenc_fixtures::MakePayload();
	const std::string yenc = yenc_fixtures::MakeArticle(payload);
	const std::string raw = "hello\r\n.\r\n";
	nntp::ArticleDecoder decoder;
	std::vector<char> dest(payload.size() + 64);

	decoder.Create(nntp::ArticleFormat::Yenc, true);
	BOOST_CHECK(DecodeAll(decoder, yenc, dest, yenc.size()) == payload);
	BOOST_CHECK(decoder.Finish() == nntp::DecodeStatus::Finished);
	BOOST_CHECK(decoder.Meta().name == "test.bin");

	decoder.Create(nntp::ArticleFormat::Raw, false);
	BOOST_CHECK(DecodeAll(decoder, raw, dest, raw.size()) == "hello\r\n");
	BOOST_CHECK(decoder.Finish() == nntp::DecodeStatus::Finished);
	// The new article starts with fresh metadata, not the previous article's name.
	BOOST_CHECK(decoder.Meta().name.empty());

	decoder.Create(nntp::ArticleFormat::Yenc, true);
	BOOST_CHECK(DecodeAll(decoder, yenc, dest, 7) == payload);
	BOOST_CHECK(decoder.Finish() == nntp::DecodeStatus::Finished);
}

BOOST_AUTO_TEST_CASE(Decoder_SplitAtAnyPositionGivesSameResult)
{
	const std::string payload = yenc_fixtures::MakePayload();
	const std::vector<std::pair<nntp::ArticleFormat, std::pair<std::string, std::string>>> cases = {
		{nntp::ArticleFormat::Yenc, {yenc_fixtures::MakeArticle(payload), payload}},
		{nntp::ArticleFormat::Ux, {"begin 644 cat.txt\r\n#0V%T\r\n`\r\nend\r\n.\r\n", "Cat"}},
		{nntp::ArticleFormat::Raw, {"Hello\r\nWorld\r\n.\r\n", "Hello\r\nWorld\r\n"}},
	};

	for (const auto& [format, articleAndExpected] : cases)
	{
		const auto& [article, expected] = articleAndExpected;
		for (const size_t destSize : {nntp::ArticleDecoder::MinBufferSize, std::max(nntp::ArticleDecoder::MinBufferSize, article.size() + 1)})
		{
			for (size_t piece = 1; piece <= article.size(); piece++)
			{
				nntp::ArticleDecoder decoder;
				decoder.Create(format, true);
				std::vector<char> dest(destSize);
				BOOST_REQUIRE_MESSAGE(DecodeAll(decoder, article, dest, piece) == expected,
					"format=" << static_cast<int>(format) << " piece=" << piece << " dest=" << destSize);
				BOOST_REQUIRE_MESSAGE(decoder.Finish() == nntp::DecodeStatus::Finished,
					"format=" << static_cast<int>(format) << " piece=" << piece);
			}
		}
	}
}

BOOST_AUTO_TEST_SUITE_END()
