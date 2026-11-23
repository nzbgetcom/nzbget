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
#include <boost/test/unit_test.hpp>

#include "YencDecoder.h"
#include "YencFixtures.h"

#include <string>
#include <vector>

using namespace yenc_fixtures;

namespace
{

struct DecodeResult
{
	std::string decoded;
	bool complete{false};
	bool finished{false};
	bool headerValid{false};
	int64_t beginOffset{-1};
	int64_t partSize{-1};
	int64_t produced{-1};
	int64_t endSize{-1};
};

// Feeds the whole article, headers included, in fixed-size pieces.
DecodeResult DecodeArticle(const std::string& article, size_t piece, bool crcCheck)
{
	DecodeResult result;
	network::nntp::YencDecoder decoder(crcCheck);

	std::vector<char> out(4096);
	size_t pos = 0;
	while (pos < article.size())
	{
		const size_t take = std::min(piece, article.size() - pos);
		const auto step = decoder.Decode(
			std::span<const char>(article.data() + pos, take),
			std::span<char>(out.data(), out.size()));

		result.decoded.append(out.data(), step.produced);
		pos += step.consumed;

		if (step.complete)
		{
			result.complete = true;
			break;
		}

		if (step.consumed == 0 && step.produced == 0)
		{
			BOOST_FAIL("decoder stalled at " << pos);
		}
	}

	result.finished = decoder.Finish() == network::nntp::DecodeStatus::Finished;
	result.headerValid = decoder.HeaderValid();
	result.beginOffset = decoder.BeginOffset();
	result.partSize = decoder.PartSize();
	result.produced = decoder.Produced();
	result.endSize = decoder.EndSize();
	return result;
}

} // namespace

namespace
{

// Decodes inside one buffer: input is read at pos, output is written at outPos,
// and outPos must never pass pos.
std::string DecodeInPlace(const std::string& article, size_t piece)
{
	std::vector<char> buf(article.begin(), article.end());
	buf.resize(article.size() + 64); // output room past the article, as a real buffer has
	network::nntp::YencDecoder decoder(true);

	size_t pos = 0;
	size_t outPos = 0;
	while (pos < article.size())
	{
		const size_t take = std::min(piece, article.size() - pos);
		const auto step = decoder.Decode(
			std::span<const char>(buf.data() + pos, take),
			std::span<char>(buf.data() + outPos, buf.size() - outPos));
		outPos += step.produced;
		pos += step.consumed;
		BOOST_REQUIRE_LE(outPos, pos);
		if (step.complete)
		{
			break;
		}
		if (step.consumed == 0 && step.produced == 0)
		{
			BOOST_FAIL("decoder stalled at " << pos);
		}
	}
	return std::string(buf.data(), outPos);
}

} // namespace

BOOST_AUTO_TEST_SUITE(YencDecoderSuite)

BOOST_AUTO_TEST_CASE(YencDecoder_RoundTripsSinglePartInSmallPieces)
{
	const std::string payload = MakePayload();
	const std::string article = MakeArticle(payload);

	for (size_t piece : {size_t(1), size_t(3), size_t(64), size_t(4096)})
	{
		const DecodeResult result = DecodeArticle(article, piece, true);
		BOOST_REQUIRE_MESSAGE(result.complete, "piece=" << piece);
		BOOST_REQUIRE_MESSAGE(result.decoded == payload, "piece=" << piece
			<< " got=" << result.decoded.size() << " want=" << payload.size());
		BOOST_REQUIRE_MESSAGE(result.finished, "piece=" << piece);
		BOOST_REQUIRE_EQUAL(result.beginOffset, 0);
		BOOST_REQUIRE_EQUAL(result.partSize, static_cast<int64_t>(payload.size()));
	}
}

BOOST_AUTO_TEST_CASE(YencDecoder_ReportsPartOffset)
{
	const std::string payload(1000, 'x');
	const std::string article = MakeArticle(payload, 1001);

	const DecodeResult result = DecodeArticle(article, 17, true);
	BOOST_REQUIRE(result.complete);
	BOOST_REQUIRE_EQUAL(result.decoded, payload);
	BOOST_REQUIRE_EQUAL(result.beginOffset, 1000);
	BOOST_REQUIRE_EQUAL(result.partSize, 1000);
}

BOOST_AUTO_TEST_CASE(YencDecoder_DetectsCrcMismatch)
{
	const std::string payload = MakePayload();
	const std::string article = MakeArticle(payload, 1, 0xdeadbeef, true);

	const DecodeResult result = DecodeArticle(article, 64, true);
	BOOST_REQUIRE(result.complete);
	BOOST_REQUIRE_EQUAL(result.decoded, payload);
	BOOST_REQUIRE(!result.finished);
}

BOOST_AUTO_TEST_CASE(YencDecoder_AcceptsMultipartPcrc32)
{
	const std::string payload = MakePayload();
	const std::string article = MakeArticle(payload, 2001, 0, true);

	const DecodeResult result = DecodeArticle(article, 37, true);
	BOOST_REQUIRE(result.complete);
	BOOST_REQUIRE_EQUAL(result.decoded, payload);
	BOOST_REQUIRE(result.finished);
	BOOST_REQUIRE_EQUAL(result.beginOffset, 2000);
}

BOOST_AUTO_TEST_CASE(YencDecoder_StitchesControlLineAcrossEverySplitPoint)
{
	const std::string payload(300, 'z');
	const std::string article = MakeArticle(payload);

	// Every split point must survive, including the ones inside "\r\n=yend".
	for (size_t piece = 1; piece <= 40; piece++)
	{
		const DecodeResult result = DecodeArticle(article, piece, true);
		BOOST_REQUIRE_MESSAGE(result.complete, "piece=" << piece);
		BOOST_REQUIRE_MESSAGE(result.decoded == payload, "piece=" << piece
			<< " got=" << result.decoded.size() << " want=" << payload.size());
		BOOST_REQUIRE_MESSAGE(result.finished, "piece=" << piece);
	}
}

BOOST_AUTO_TEST_CASE(YencDecoder_FillsOutputBufferAndResumes)
{
	const std::string payload = MakePayload();
	network::nntp::YencDecoder decoder(true);

	// Small output buffer: every call stops on a full buffer and resumes.
	const std::string body = EncodeYenc(payload) + "=yend size=" +
		std::to_string(payload.size()) + " part=1\r\n";

	std::vector<char> out(16);
	std::string decoded;
	size_t pos = 0;
	bool sawFull = false;
	while (pos < body.size())
	{
		const size_t take = std::min<size_t>(100, body.size() - pos);
		const auto step = decoder.Decode(
			std::span<const char>(body.data() + pos, take),
			std::span<char>(out.data(), out.size()));
		decoded.append(out.data(), step.produced);
		pos += step.consumed;
		sawFull = sawFull || step.produced == out.size();
		if (step.complete)
		{
			break;
		}
	}

	BOOST_REQUIRE(sawFull);
	BOOST_REQUIRE_EQUAL(decoded, payload);
	BOOST_REQUIRE(decoder.IsComplete());
}

BOOST_AUTO_TEST_CASE(YencDecoder_ReportsProducedBytesAndEndSize)
{
	const std::string payload = MakePayload();
	const DecodeResult result = DecodeArticle(MakeArticle(payload), 7, true);

	BOOST_TEST(result.produced == static_cast<int64_t>(payload.size()));
	BOOST_TEST(result.endSize == static_cast<int64_t>(payload.size()));
}

BOOST_AUTO_TEST_CASE(YencDecoder_SkipsCrcOnPartWithoutPcrc32)
{
	// On a part, crc32= is the whole-file CRC and cannot be checked against this part.
	const std::string payload = MakePayload();
	const DecodeResult result = DecodeArticle(MakeArticle(payload, 1, 0xdeadbeef, false), 64, true);

	BOOST_TEST(result.complete);
	BOOST_TEST(result.finished);
}

BOOST_AUTO_TEST_CASE(YencDecoder_SinglePartIgnoresPcrc32)
{
	// pcrc32= contains "crc32=": it must not be taken as the single-part CRC.
	const std::string payload = MakePayload();
	const std::string article = "=ybegin line=128 size=" + std::to_string(payload.size()) +
		" name=test.bin\r\n" + EncodeYenc(payload) + "=yend size=" + std::to_string(payload.size()) +
		" pcrc32=deadbeef\r\n.\r\n";
	const DecodeResult result = DecodeArticle(article, 64, true);

	BOOST_TEST(result.complete);
	BOOST_TEST(result.finished);
}

BOOST_AUTO_TEST_CASE(YencDecoder_ReadsBeginLineMetadata)
{
	const std::string payload = MakePayload();
	network::nntp::YencDecoder decoder(true);
	const std::string article = MakeArticle(payload, 1);
	decoder.ParseControlLine(std::string_view(article).substr(0, article.find("\r\n")));

	BOOST_TEST(decoder.HasBegin());
	BOOST_TEST(decoder.Multipart());
	BOOST_TEST(decoder.FileSize() == static_cast<int64_t>(payload.size()));
	BOOST_TEST(decoder.Name() == "test.bin");
}

BOOST_AUTO_TEST_CASE(YencDecoder_TakesYpartFromBeginLine)
{
	// Some encoders put "=ypart" right after the name on the =ybegin line.
	const std::string payload(50, 'q');
	const std::string article = "=ybegin part=1 total=1 line=128 size=150 name=test.bin=ypart begin=101 end=150\r\n" +
		EncodeYenc(payload) + "=yend size=50 part=1 pcrc32=" + CrcHex(Crc32Of(payload)) + "\r\n.\r\n";

	const DecodeResult result = DecodeArticle(article, 9, true);
	BOOST_TEST(result.complete);
	BOOST_TEST(result.decoded == payload);
	BOOST_TEST(result.finished);
	BOOST_TEST(result.beginOffset == 100);
	BOOST_TEST(result.partSize == 50);
}

BOOST_AUTO_TEST_CASE(YencDecoder_KeepsYpartFromBeginLineWithoutPartParam)
{
	// Without part= on =ybegin the embedded =ypart must not be replaced by the whole file.
	const std::string payload(50, 'q');
	const std::string article = "=ybegin size=150 name=test.bin=ypart begin=101 end=150\r\n" +
		EncodeYenc(payload) + "=yend size=50 part=1 pcrc32=" + CrcHex(Crc32Of(payload)) + "\r\n.\r\n";

	const DecodeResult result = DecodeArticle(article, 9, true);
	BOOST_TEST(result.complete);
	BOOST_TEST(result.decoded == payload);
	BOOST_TEST(result.finished);
	BOOST_TEST(result.beginOffset == 100);
	BOOST_TEST(result.partSize == 50);
}

BOOST_AUTO_TEST_CASE(YencDecoder_AcceptsLongYbeginLine)
{
	// A long but valid name must not be mistaken for data.
	const std::string payload = MakePayload();
	const std::string n = std::to_string(payload.size());
	const std::string article = "=ybegin part=1 total=1 line=128 size=" + n + " name=" + std::string(2000, 'n') +
		"\r\n=ypart begin=1 end=" + n + "\r\n" + EncodeYenc(payload) +
		"=yend size=" + n + " part=1 pcrc32=" + CrcHex(Crc32Of(payload)) + "\r\n.\r\n";

	for (size_t piece : {size_t(1), size_t(7), size_t(4096)})
	{
		const DecodeResult result = DecodeArticle(article, piece, true);
		BOOST_REQUIRE_MESSAGE(result.complete, "piece=" << piece);
		BOOST_REQUIRE_MESSAGE(result.headerValid, "piece=" << piece);
		BOOST_REQUIRE_MESSAGE(result.decoded == payload, "piece=" << piece);
		BOOST_REQUIRE_MESSAGE(result.finished, "piece=" << piece);
	}
}

BOOST_AUTO_TEST_CASE(YencDecoder_RefusesControlLineOverLimitWithoutDecodingIt)
{
	// The over-long line is refused, never replayed as data: the payload after it still decodes.
	const std::string payload = MakePayload();
	const std::string n = std::to_string(payload.size());
	const std::string article = "=ybegin part=1 total=1 line=128 size=" + n + " name=" + std::string(9000, 'n') +
		"\r\n=ypart begin=1 end=" + n + "\r\n" + EncodeYenc(payload) + "=yend size=" + n + " part=1\r\n.\r\n";

	for (size_t piece : {size_t(7), size_t(4096)})
	{
		const DecodeResult result = DecodeArticle(article, piece, true);
		BOOST_REQUIRE_MESSAGE(result.complete, "piece=" << piece);
		BOOST_REQUIRE_MESSAGE(!result.headerValid, "piece=" << piece);
		BOOST_REQUIRE_MESSAGE(result.decoded == payload, "piece=" << piece);
	}
}

BOOST_AUTO_TEST_CASE(YencDecoder_KeepsDataLinesStartingWithYAsData)
{
	// "=y" is an escape for byte 0x0F; "=yabc" and "=yendabc" are data, not control lines.
	const std::string article = "=ybegin part=1 total=1 line=128 size=11 name=test.bin\r\n"
		"=ypart begin=1 end=11\r\n=yabc\r\n=yendabc\r\n=yend size=11 part=1\r\n.\r\n";
	const std::string expected = std::string("\x0f" "789" "\x0f" ";D:789");

	for (size_t piece : {size_t(1), size_t(2), size_t(3), size_t(5), size_t(7), size_t(4096)})
	{
		const DecodeResult result = DecodeArticle(article, piece, true);
		BOOST_REQUIRE_MESSAGE(result.complete, "piece=" << piece);
		BOOST_REQUIRE_MESSAGE(result.headerValid, "piece=" << piece);
		BOOST_REQUIRE_MESSAGE(result.decoded == expected, "piece=" << piece);
	}
}

BOOST_AUTO_TEST_CASE(YencDecoder_SkipsCrcWorkWhenCheckIsOff)
{
	const std::string payload = MakePayload();
	network::nntp::YencDecoder decoder(false);
	std::vector<char> out(payload.size() + 4096);
	const std::string article = MakeArticle(payload);
	decoder.Decode(std::span<const char>(article), std::span<char>(out));

	BOOST_TEST(decoder.IsComplete());
	BOOST_TEST(decoder.CalculatedCrc() == 0);
}

BOOST_AUTO_TEST_CASE(YencDecoder_AcceptsPartEndBeyondWholeFileSize)
{
	// Some encoders put the part size into =ybegin size=; the =ypart range is the truth.
	const std::string payload(50, 'q');
	const std::string article = "=ybegin part=1 total=2 line=128 size=50 name=test.bin\r\n"
		"=ypart begin=101 end=150\r\n" + EncodeYenc(payload) + "=yend size=50 part=1\r\n.\r\n";

	const DecodeResult result = DecodeArticle(article, 9, true);
	BOOST_TEST(result.complete);
	BOOST_TEST(result.headerValid);
	BOOST_TEST(result.decoded == payload);
	BOOST_TEST(result.beginOffset == 100);
	BOOST_TEST(result.partSize == 50);
}

BOOST_AUTO_TEST_CASE(YencDecoder_RejectsPartWithoutEnd)
{
	const std::string payload(50, 'q');
	const std::string article = "=ybegin part=1 total=2 line=128 size=150 name=test.bin\r\n"
		"=ypart begin=101\r\n" + EncodeYenc(payload) + "=yend size=50 part=1\r\n.\r\n";

	const DecodeResult result = DecodeArticle(article, 9, true);
	BOOST_TEST(!result.headerValid);
}

BOOST_AUTO_TEST_CASE(YencDecoder_AcceptsEmptyArticle)
{
	const std::string article = "=ybegin line=128 size=0 name=empty.bin\r\n=yend size=0\r\n.\r\n";

	const DecodeResult result = DecodeArticle(article, 64, true);

	BOOST_TEST(result.complete);
	BOOST_TEST(result.finished);
	BOOST_TEST(result.decoded.empty());
	BOOST_TEST(result.produced == 0);
}

BOOST_AUTO_TEST_CASE(YencDecoder_EndWithoutTerminatorIsProtocolIncomplete)
{
	// The content is valid, but the caller must keep reading until the "." line.
	const std::string payload(16, 'k');
	const std::string article = "=ybegin line=128 size=16 name=a.bin\r\n" +
		EncodeYenc(payload) + "=yend size=16\r\n";

	const DecodeResult result = DecodeArticle(article, 5, true);

	BOOST_TEST(!result.complete);
	BOOST_TEST(result.finished);
	BOOST_TEST(result.decoded == payload);
}

BOOST_AUTO_TEST_CASE(YencDecoder_ConsumesEndAndTerminatorSplitAtEveryPoint)
{
	const std::string payload(20, 's');
	const std::string article = MakeArticle(payload);

	for (size_t cut = 0; cut <= article.size(); cut++)
	{
		network::nntp::YencDecoder decoder(true);
		std::vector<char> out(64);
		std::string decoded;
		bool complete = false;

		auto feed = [&](std::span<const char> data)
		{
			while (!data.empty() && !complete)
			{
				const auto step = decoder.Decode(data, std::span<char>(out.data(), out.size()));
				decoded.append(out.data(), step.produced);
				data = data.subspan(step.consumed);
				complete = step.complete;
				if (step.consumed == 0 && step.produced == 0)
				{
					BOOST_FAIL("decoder stalled at cut=" << cut);
				}
			}
		};

		feed(std::span<const char>(article.data(), cut));
		feed(std::span<const char>(article.data() + cut, article.size() - cut));

		BOOST_REQUIRE_MESSAGE(complete, "cut=" << cut);
		BOOST_REQUIRE_MESSAGE(decoded == payload, "cut=" << cut << " got=" << decoded.size());
	}
}

BOOST_AUTO_TEST_CASE(YencDecoder_RejectsInvalidPartRanges)
{
	const std::string payload(10, 'p');
	const std::string body = EncodeYenc(payload) + "=yend size=10 part=1\r\n.\r\n";

	const std::string beginZero = "=ybegin part=1 total=2 line=128 size=100 name=a.bin\r\n"
		"=ypart begin=0 end=10\r\n" + body;
	BOOST_TEST(!DecodeArticle(beginZero, 7, true).finished);

	const std::string endBeforeBegin = "=ybegin part=1 total=2 line=128 size=100 name=a.bin\r\n"
		"=ypart begin=11 end=10\r\n" + body;
	BOOST_TEST(!DecodeArticle(endBeforeBegin, 7, true).finished);

	// A part line after data would move the write offset of bytes already written.
	const std::string latePart = "=ybegin part=1 total=2 line=128 size=100 name=a.bin\r\n"
		"=ypart begin=1 end=10\r\n" + EncodeYenc(payload) +
		"=ypart begin=11 end=20\r\n=yend size=10 part=1\r\n.\r\n";
	BOOST_TEST(!DecodeArticle(latePart, 7, true).finished);
}

BOOST_AUTO_TEST_CASE(YencDecoder_RejectsOverflowingFileSize)
{
	const std::string payload(10, 'o');
	const std::string article = "=ybegin line=128 size=99999999999999999999 name=a.bin\r\n" +
		EncodeYenc(payload) + "=yend size=10\r\n.\r\n";

	const DecodeResult result = DecodeArticle(article, 7, true);

	BOOST_TEST(result.complete);
	BOOST_TEST(!result.finished);
	BOOST_TEST(result.decoded == payload);
}

BOOST_AUTO_TEST_CASE(YencDecoder_RejectsBareLfBody)
{
	// yEnc framing is CRLF; a bare LF is decoded as data and the size check catches it.
	const std::string article = "=ybegin line=128 size=3 name=a.bin\n"
		"\x8b\x8c\x8d\n=yend size=3\n.\n";

	const DecodeResult result = DecodeArticle(article, 4, true);

	BOOST_TEST(!result.finished);
}

BOOST_AUTO_TEST_CASE(YencDecoder_ReadsOnlyWithinInputSpan)
{
	const std::string payload = MakePayload();
	const std::string text = MakeArticle(payload);
	// Heap buffer of exactly the article size: AddressSanitizer flags any read past the span.
	const std::vector<char> article(text.begin(), text.end());

	for (size_t piece : {size_t(1), size_t(5), size_t(200), size_t(4096), article.size()})
	{
		network::nntp::YencDecoder decoder(true);
		std::vector<char> out(8192);
		std::string decoded;
		size_t pos = 0;
		bool complete = false;
		while (pos < article.size() && !complete)
		{
			const size_t take = std::min(piece, article.size() - pos);
			const auto step = decoder.Decode(std::span<const char>(article.data() + pos, take), out);
			decoded.append(out.data(), step.produced);
			pos += step.consumed;
			complete = step.complete;
			BOOST_REQUIRE_MESSAGE(step.consumed > 0 || step.produced > 0 || complete, "piece=" << piece);
		}
		BOOST_REQUIRE_MESSAGE(decoded == payload, "piece=" << piece);
	}
}

BOOST_AUTO_TEST_CASE(YencDecoder_DecodesInPlace)
{
	const std::string payload = MakePayload();
	const std::string article = MakeArticle(payload);

	for (size_t piece : {size_t(1), size_t(3), size_t(64), size_t(4096)})
	{
		BOOST_REQUIRE_MESSAGE(DecodeInPlace(article, piece) == payload, "piece=" << piece);
	}
}

BOOST_AUTO_TEST_CASE(YencDecoder_InPlaceReplayNeverPassesUnconsumedInput)
{
	// Replayed "=y" data lines are written after their bytes were already consumed; the
	// in-place output must still stay behind the input position.
	const std::string article = "=ybegin part=1 total=1 line=128 size=11 name=test.bin\r\n"
		"=ypart begin=1 end=11\r\n=yabc\r\n=yendabc\r\n=yend size=11 part=1\r\n.\r\n";
	const std::string expected = std::string("\x0f" "789" "\x0f" ";D:789");

	for (size_t piece : {size_t(1), size_t(2), size_t(3), size_t(5), size_t(7), size_t(4096)})
	{
		BOOST_REQUIRE_MESSAGE(DecodeInPlace(article, piece) == expected, "piece=" << piece);
	}
}

namespace
{

/// Single-part article whose =ybegin line is exactly `total` bytes including CRLF.
std::string ArticleWithBeginLine(size_t total, const std::string& payload)
{
	const std::string n = std::to_string(payload.size());
	const std::string head = "=ybegin part=1 total=1 line=128 size=" + n + " name=";
	const std::string begin = head + std::string(total - head.size() - 2, 'n') + "\r\n";
	return begin + "=ypart begin=1 end=" + n + "\r\n" + EncodeYenc(payload) +
		"=yend size=" + n + " part=1 pcrc32=" + CrcHex(Crc32Of(payload)) + "\r\n.\r\n";
}

} // namespace

BOOST_AUTO_TEST_CASE(YencDecoder_AcceptsControlLineExactlyAtLimit)
{
	const std::string payload = MakePayload();
	const std::string article = ArticleWithBeginLine(8192, payload);

	for (size_t piece : {size_t(1), size_t(7), size_t(4096)})
	{
		const DecodeResult result = DecodeArticle(article, piece, true);
		BOOST_REQUIRE_MESSAGE(result.complete, "piece=" << piece);
		BOOST_REQUIRE_MESSAGE(result.headerValid, "piece=" << piece);
		BOOST_REQUIRE_MESSAGE(result.finished, "piece=" << piece);
	}
}

BOOST_AUTO_TEST_CASE(YencDecoder_RefusesControlLineOneByteOverLimit)
{
	const std::string payload = MakePayload();
	const std::string article = ArticleWithBeginLine(8193, payload);

	for (size_t piece : {size_t(1), size_t(7), size_t(4096)})
	{
		const DecodeResult result = DecodeArticle(article, piece, true);
		BOOST_REQUIRE_MESSAGE(result.complete, "piece=" << piece);
		BOOST_REQUIRE_MESSAGE(!result.headerValid, "piece=" << piece);
		BOOST_REQUIRE_MESSAGE(!result.finished, "piece=" << piece);
	}
}

BOOST_AUTO_TEST_CASE(YencDecoder_NeverFinishesOnHostileHeaderValues)
{
	const std::string body = EncodeYenc(std::string(10, 'h'));
	const std::string begin = "=ybegin part=1 total=1 line=128 size=10 name=a.bin\r\n";
	const std::string end = "=yend size=10 part=1\r\n.\r\n";
	const std::vector<std::string> articles = {
		begin + "=ypart begin=-1 end=10\r\n" + body + end,
		begin + "=ypart begin=1 end=9223372036854775807\r\n" + body + end,
		begin + "=ypart begin=9223372036854775807 end=9223372036854775807\r\n" + body + end,
		begin + "=ypart begin=1 end=10\r\n" + body + "=yend size=-9223372036854775808 part=1\r\n.\r\n",
		begin + "=ypart begin=1 end=10\r\n" + body + "=yend size=10 part=1 pcrc32=zzzz\r\n.\r\n",
		"=ybegin part=1 total=1 line=128 size=9223372036854775807 name=a.bin\r\n" + body + end,
		begin + "=ypart begin=1 end=10 end=\r\n" + body + "=yend size=99999999999999999999999 part=1\r\n.\r\n",
	};

	for (size_t i = 0; i < articles.size(); i++)
	{
		for (size_t piece : {size_t(1), size_t(7), size_t(4096)})
		{
			const DecodeResult result = DecodeArticle(articles[i], piece, true);
			BOOST_REQUIRE_MESSAGE(result.complete, "article=" << i << " piece=" << piece);
			BOOST_REQUIRE_MESSAGE(!result.finished, "article=" << i << " piece=" << piece);
		}
	}
}

BOOST_AUTO_TEST_SUITE_END()
