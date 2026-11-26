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

#include "DecoderTestHelpers.h"
#include "YencDecoder.h"
#include "YencFixtures.h"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

using namespace yenc_fixtures;

namespace
{

struct DecodeResult
{
	std::string decoded;
	std::string name;
	bool complete{false};
	bool finished{false};
	bool headerValid{false};
	int64_t beginOffset{-1};
	int64_t partSize{-1};
	int64_t fileSize{-1};
	int64_t produced{-1};
	int64_t endSize{-1};
};

// Feeds the whole article, headers included, through the shared window helper.
DecodeResult DecodeArticle(const std::string& article, size_t piece, bool crcCheck)
{
	DecodeResult result;
	network::nntp::YencDecoder decoder(crcCheck);

	const nntp_test::Run run = nntp_test::Decode(decoder, article, piece);
	result.decoded = run.out;
	result.complete = run.complete;
	result.finished = decoder.Finish() == network::nntp::DecodeStatus::Finished;
	result.headerValid = decoder.HeaderValid();
	result.name = decoder.Meta().name;
	result.beginOffset = decoder.Meta().offset;
	result.partSize = decoder.Meta().size;
	result.fileSize = decoder.Meta().fileSize;
	result.produced = decoder.Produced();
	result.endSize = decoder.EndSize();
	return result;
}

} // namespace

namespace
{

// Decodes into a separate output buffer: input is read at the window start, output is
// written at outPos, and outPos must never pass the consumed input.
std::string DecodeWithSeparateOutput(const std::string& article, size_t piece)
{
	std::vector<char> src(article.size() + 1);
	std::vector<char> out(article.size() + 64);
	network::nntp::YencDecoder decoder(true);

	size_t fed = 0;
	size_t end = 0;
	size_t outPos = 0;
	size_t consumedTotal = 0;
	while (fed < article.size() || end > 0)
	{
		if (fed < article.size())
		{
			const size_t take = std::min(piece, article.size() - fed);
			std::memcpy(src.data() + end, article.data() + fed, take);
			end += take;
			fed += take;
		}

		const auto step = decoder.Decode(
			std::span<const char>(src.data(), end),
			std::span<char>(out.data() + outPos, out.size() - outPos));
		outPos += step.produced;
		consumedTotal += step.consumed;
		BOOST_REQUIRE_LE(outPos, consumedTotal);
		std::memmove(src.data(), src.data() + step.consumed, end - step.consumed);
		end -= step.consumed;

		if (step.complete)
		{
			break;
		}
		if (step.consumed == 0 && step.produced == 0 && fed == article.size())
		{
			break;
		}
	}
	return std::string(out.data(), outPos);
}

using nntp_test::DecodeAtSourceStart;
using nntp_test::Run;
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
		std::to_string(payload.size()) + " part=1\r\n.\r\n";

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
	const std::string article = MakeArticle(payload, 1);
	const DecodeResult result = DecodeArticle(article.substr(0, article.find("\r\n") + 2), 7, true);

	BOOST_TEST(result.name == "test.bin");
	BOOST_TEST(result.fileSize == static_cast<int64_t>(payload.size()));
	BOOST_TEST(!result.complete);
}

BOOST_AUTO_TEST_CASE(YencDecoder_DecodesUtf8Name)
{
	const std::string article =
		"=ybegin line=128 size=3 name=caf\xc3\xa9.bin\r\n" + EncodeYenc("Cat") + "=yend size=3\r\n.\r\n";

	network::nntp::YencDecoder decoder(true);
	std::vector<char> out(16);
	const auto step =
		decoder.Decode(std::span<const char>(article.data(), article.size()), std::span<char>(out.data(), out.size()));

	BOOST_TEST(step.complete);
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::Finished);
	BOOST_CHECK_EQUAL(decoder.Meta().name, "caf\xc3\xa9.bin");
}

BOOST_AUTO_TEST_CASE(YencDecoder_DecodesLatin1Name)
{
	const std::string article =
		"=ybegin line=128 size=3 name=caf\xe9.bin\r\n" + EncodeYenc("Cat") + "=yend size=3\r\n.\r\n";

	network::nntp::YencDecoder decoder(true);
	std::vector<char> out(16);
	const auto step =
		decoder.Decode(std::span<const char>(article.data(), article.size()), std::span<char>(out.data(), out.size()));

	BOOST_TEST(step.complete);
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::Finished);
	BOOST_CHECK_EQUAL(decoder.Meta().name, "caf\xc3\xa9.bin");
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

BOOST_AUTO_TEST_CASE(YencDecoder_TakesYpartFromBeginLineAfterSpaces)
{
	// Spaces between the name and the appended "=ypart" are not part of the name,
	// and the ypart range must still be applied.
	const std::string payload(50, 'q');
	for (const char* gap : {" ", "   "})
	{
		const std::string article = std::string("=ybegin part=1 total=1 line=128 size=150 name=test.bin") + gap +
			"=ypart begin=101 end=150\r\n" + EncodeYenc(payload) +
			"=yend size=50 part=1 pcrc32=" + CrcHex(Crc32Of(payload)) + "\r\n.\r\n";

		for (size_t piece : {size_t(1), size_t(9), size_t(4096)})
		{
			const DecodeResult result = DecodeArticle(article, piece, true);
			BOOST_REQUIRE_MESSAGE(result.complete, "gap=" << gap << " piece=" << piece);
			BOOST_CHECK_MESSAGE(result.finished, "gap=" << gap << " piece=" << piece);
			BOOST_CHECK(result.headerValid);
			BOOST_CHECK_EQUAL(result.name, "test.bin");
			BOOST_CHECK_EQUAL(result.beginOffset, 100);
			BOOST_CHECK_EQUAL(result.partSize, 50);
			BOOST_CHECK(result.decoded == payload);
		}
	}
}

BOOST_AUTO_TEST_CASE(YencDecoder_TrailingSpacesAfterNameAreNotAYpart)
{
	// Trailing spaces with no "=ypart" must only be trimmed from the name; the body still decodes.
	const std::string payload(10, 'h');
	const std::string article = "=ybegin line=128 size=10 name=a.bin   \r\n" + EncodeYenc(payload) +
		"=yend size=10 crc32=" + CrcHex(Crc32Of(payload)) + "\r\n.\r\n";

	for (size_t piece : {size_t(1), size_t(7), size_t(4096)})
	{
		const DecodeResult result = DecodeArticle(article, piece, true);
		BOOST_REQUIRE_MESSAGE(result.complete, "piece=" << piece);
		BOOST_CHECK_MESSAGE(result.finished, "piece=" << piece);
		BOOST_CHECK_EQUAL(result.name, "a.bin");
		BOOST_CHECK(result.decoded == payload);
	}
}

BOOST_AUTO_TEST_CASE(YencDecoder_PartParamInNameIsNotMultipart)
{
	// part= after name= belongs to the file name, so the article is a single part.
	const std::string payload = "Cat";
	const std::string article = "=ybegin line=128 size=3 name=episode part=2.mkv\r\n" +
		EncodeYenc(payload) + "=yend size=3\r\n.\r\n";

	const DecodeResult result = DecodeArticle(article, 9, true);
	BOOST_TEST(result.complete);
	BOOST_TEST(result.finished);
	BOOST_TEST(result.name == "episode part=2.mkv");
	BOOST_TEST(result.beginOffset == 0);
	BOOST_TEST(result.partSize == 3);
	BOOST_TEST(result.decoded == payload);
}

BOOST_AUTO_TEST_CASE(YencDecoder_RejectsMultipartWithoutYpart)
{
	// A real part= before name= still requires an =ypart line.
	const std::string payload(10, 'q');
	const std::string article = "=ybegin part=1 total=2 line=128 size=20 name=a.bin\r\n" +
		EncodeYenc(payload) + "=yend size=10 part=1\r\n.\r\n";

	const DecodeResult result = DecodeArticle(article, 9, true);
	BOOST_TEST(!result.finished);
}

BOOST_AUTO_TEST_CASE(YencDecoder_NameFirstMultipartWithoutYpartIsRejected)
{
	// part= before the first name= is not seen when name comes first, so the article is judged as single part and its size mismatches.
	const std::string payload(10, 'q');
	const std::string article = "=ybegin name=a.bin part=1 total=2 line=128 size=20\r\n" +
		EncodeYenc(payload) + "=yend size=10 part=1\r\n.\r\n";

	const DecodeResult result = DecodeArticle(article, 9, true);
	BOOST_TEST(!result.finished);
}

BOOST_AUTO_TEST_CASE(YencDecoder_NameFirstMultipartWithYpartStillWorks)
{
	const std::string payload(10, 'q');
	const std::string article = "=ybegin name=a.bin part=1 total=2 line=128 size=20\r\n=ypart begin=1 end=10\r\n" +
		EncodeYenc(payload) + "=yend size=10 part=1\r\n.\r\n";

	const DecodeResult result = DecodeArticle(article, 9, true);
	BOOST_TEST(result.complete);
	BOOST_TEST(result.finished);
	BOOST_TEST(result.beginOffset == 0);
	BOOST_TEST(result.partSize == 10);
	BOOST_TEST(result.decoded == payload);
}

BOOST_AUTO_TEST_CASE(YencDecoder_PartAndNameBoundariesAreSplitInvariant)
{
	// Edge headers around part= and name=: the outcome must not depend on how the input is split.
	const std::vector<std::string> headers = {
		"=ybegin line=128 size=3 name=episode part=2.mkv",
		"=ybegin name=episode part=2.mkv line=128 size=3",
		"=ybegin size=3 name=x name=part=1",
		"=ybegin size=3 name=part=1",
		"=ybegin size=3 name=",
		"=ybegin size=3 name=a.bin part=",
		"=ybegin size=3 name=a.bin=ypart begin=1 end=3",
		"=ybegin part=1 total=1 line=128 size=3 name=a.bin",
		"=ybegin size=3 xpart=1 name=a.bin",
	};

	for (const std::string& header : headers)
	{
		const std::string article = header + "\r\n" + EncodeYenc("Cat") + "=yend size=3\r\n.\r\n";
		const DecodeResult reference = DecodeArticle(article, 4096, true);
		for (size_t piece : {size_t(1), size_t(3), size_t(7)})
		{
			const DecodeResult result = DecodeArticle(article, piece, true);
			const bool same = result.complete == reference.complete && result.finished == reference.finished &&
				result.headerValid == reference.headerValid && result.decoded == reference.decoded &&
				result.name == reference.name && result.beginOffset == reference.beginOffset &&
				result.partSize == reference.partSize && result.fileSize == reference.fileSize;
			BOOST_REQUIRE_MESSAGE(same, "header=" << header << " piece=" << piece);
		}
	}
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
	// A non-conforming encoder may escape 0x0F; "=y" is that escape, not a control line.
	const std::string article = "=ybegin part=1 total=1 line=128 size=11 name=test.bin\r\n"
		"=ypart begin=1 end=11\r\n=yabc\r\n=yendabc\r\n=yend size=11 part=1\r\n.\r\n";
	const std::string expected = "\x0f" "789\x0f;D:789";

	for (size_t piece : {size_t(1), size_t(2), size_t(3), size_t(5), size_t(7), size_t(4096)})
	{
		const DecodeResult result = DecodeArticle(article, piece, true);
		BOOST_REQUIRE_MESSAGE(result.complete, "piece=" << piece);
		BOOST_REQUIRE_MESSAGE(result.finished, "piece=" << piece);
		BOOST_REQUIRE_MESSAGE(result.headerValid, "piece=" << piece);
		BOOST_REQUIRE_MESSAGE(result.decoded == expected, "piece=" << piece);
	}

	const nntp_test::Run inPlace =
		nntp_test::DecodeAtSourceStart<network::nntp::YencDecoder>(article, 8, 3, true);
	BOOST_REQUIRE(inPlace.complete);
	BOOST_CHECK_EQUAL(inPlace.out, expected);
}

BOOST_AUTO_TEST_CASE(YencDecoder_ClassifiesSplitDataLineAsData)
{
	// "=ybe" is a prefix of "=ybegin", so the decoder must wait for more bytes before
	// deciding; the finished line "=ybeta" is data.
	const std::string article = "=ybegin line=128 size=5 name=t.bin\r\n=ybeta\r\n=yend size=5\r\n.\r\n";
	const std::string expected = "\x0f" "8;J7";

	for (size_t piece : {size_t(1), size_t(2), size_t(3), size_t(4), size_t(5), size_t(4096)})
	{
		const DecodeResult result = DecodeArticle(article, piece, true);
		BOOST_REQUIRE_MESSAGE(result.complete, "piece=" << piece);
		BOOST_REQUIRE_MESSAGE(result.finished, "piece=" << piece);
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
	BOOST_TEST(decoder.Meta().calculatedCrc == 0);
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
	BOOST_TEST(!result.finished);
	BOOST_TEST(result.decoded == payload);
}

BOOST_AUTO_TEST_CASE(YencDecoder_StopsAtTerminatorWithoutYend)
{
	// Without =yend rapidyenc reports the "." terminator itself. Bytes after it belong
	// to the next response and must not be decoded.
	const std::string payload(20, 't');
	const std::string head = "=ybegin line=128 size=20 name=a.bin\r\n" + EncodeYenc(payload);
	const std::string trailing = "TRAILING\r\n";
	const std::string article = head + ".\r\n" + trailing;

	network::nntp::YencDecoder decoder(true);
	std::vector<char> out(128);
	const auto step = decoder.Decode(std::span<const char>(article), std::span<char>(out));

	BOOST_TEST(step.complete);
	BOOST_TEST(step.produced == payload.size());
	BOOST_TEST(step.consumed == head.size() + 3);
	BOOST_TEST(std::string(out.data(), step.produced) == payload);
	BOOST_TEST((decoder.Finish() == network::nntp::DecodeStatus::ArticleIncomplete));
}

BOOST_AUTO_TEST_CASE(YencDecoder_IgnoresDotsInsideLineAfterEnd)
{
	const std::string payload = "Cat";
	for (const std::string& line : {std::string("foo.\r\n"), std::string("..\r\n")})
	{
		const std::string article =
			"=ybegin line=128 size=3 name=a.bin\r\n" + EncodeYenc(payload) + "=yend size=3\r\n" + line + ".\r\nNEXT";
		network::nntp::YencDecoder decoder(true);
		std::vector<char> out(64);

		const auto step = decoder.Decode(std::span<const char>(article), out);

		BOOST_REQUIRE_MESSAGE(step.complete, line);
		BOOST_TEST((decoder.Finish() == network::nntp::DecodeStatus::Finished));
		BOOST_TEST(std::string_view(article).substr(step.consumed) == "NEXT");
	}
}

BOOST_AUTO_TEST_CASE(YencDecoder_RejectsNegativeSizes)
{
	const std::string body = EncodeYenc("Cat");
	const std::vector<std::string> articles = {
		"=ybegin line=128 size=-1 name=a.bin\r\n" + body + "=yend size=3\r\n.\r\n",
		"=ybegin line=128 size=3 name=a.bin\r\n" + body + "=yend size=-1\r\n.\r\n",
	};

	for (const std::string& article : articles)
	{
		const DecodeResult result = DecodeArticle(article, 7, true);
		BOOST_TEST(result.complete);
		BOOST_TEST(!result.headerValid);
		BOOST_TEST(!result.finished);
	}
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

		// The caller keeps the unconsumed tail, so a partial "=y" or terminator waits for the next chunk.
		std::string pending;
		auto feed = [&](std::string_view data)
		{
			pending.append(data);
			while (!pending.empty() && !complete)
			{
				const auto step = decoder.Decode(std::span<const char>(pending.data(), pending.size()),
					std::span<char>(out.data(), out.size()));
				decoded.append(out.data(), step.produced);
				pending.erase(0, step.consumed);
				complete = step.complete;
				if (step.consumed == 0 && step.produced == 0)
				{
					break;
				}
			}
		};

		feed(std::string_view(article.data(), cut));
		feed(std::string_view(article.data() + cut, article.size() - cut));

		BOOST_REQUIRE_MESSAGE(complete, "cut=" << cut);
		BOOST_REQUIRE_MESSAGE(decoded == payload, "cut=" << cut << " got=" << decoded.size());
		BOOST_REQUIRE_MESSAGE(decoder.Finish() == network::nntp::DecodeStatus::Finished, "cut=" << cut);
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
		size_t end = 0;
		bool complete = false;
		while (pos < article.size() && !complete)
		{
			// The window starts at the first unconsumed byte and grows by one piece per call.
			end = std::min(std::max(end, pos) + piece, article.size());
			const auto step = decoder.Decode(std::span<const char>(article.data() + pos, end - pos), out);
			decoded.append(out.data(), step.produced);
			pos += step.consumed;
			complete = step.complete;
			// A short window may legitimately make no progress (a partial "=y"); only the whole article must.
			BOOST_REQUIRE_MESSAGE(step.consumed > 0 || step.produced > 0 || complete || end < article.size(),
				"piece=" << piece);
		}
		BOOST_REQUIRE_MESSAGE(decoded == payload, "piece=" << piece);
	}
}

BOOST_AUTO_TEST_CASE(YencDecoder_DecodesWithSeparateOutput)
{
	const std::string payload = MakePayload();
	const std::string article = MakeArticle(payload);

	for (size_t piece : {size_t(1), size_t(3), size_t(64), size_t(4096)})
	{
		BOOST_REQUIRE_MESSAGE(DecodeWithSeparateOutput(article, piece) == payload, "piece=" << piece);
	}
}

BOOST_AUTO_TEST_CASE(YencDecoder_DecodesInPlaceAtSourceStartAcrossChunks)
{
	// Input and output share one window that starts at the first unconsumed byte;
	// control lines and data span several windows.
	const std::string payload = MakePayload();
	const std::string article = MakeArticle(payload);

	const std::pair<size_t, size_t> cases[] = {{8, 3}, {16, 1}, {64 * 1024, 4096}};
	for (const auto& [capacity, piece] : cases)
	{
		const Run run = DecodeAtSourceStart<network::nntp::YencDecoder>(article, capacity, piece, true);
		BOOST_REQUIRE_MESSAGE(run.complete, "capacity=" << capacity << " piece=" << piece);
		BOOST_REQUIRE_MESSAGE(run.out == payload, "capacity=" << capacity << " piece=" << piece);
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

BOOST_AUTO_TEST_CASE(YencDecoder_AcceptsTrailingJunkInNumericParams)
{
	const std::string payload(10, 'h');
	const std::vector<std::string> articles = {
		"=ybegin size=10x\r\n" + EncodeYenc(payload) + "=yend size=10\r\n.\r\n",
		"=ybegin size=10\r\n" + EncodeYenc(payload) + "=yend size=10x\r\n.\r\n",
		"=ybegin part=1 total=1 line=128 size=10 name=a.bin\r\n=ypart begin=1 end=10x\r\n" + EncodeYenc(payload) +
			"=yend size=10 part=1\r\n.\r\n",
	};

	for (size_t i = 0; i < articles.size(); i++)
	{
		for (size_t piece : {size_t(1), size_t(7), size_t(4096)})
		{
			const DecodeResult result = DecodeArticle(articles[i], piece, true);
			BOOST_REQUIRE_MESSAGE(result.complete, "article=" << i << " piece=" << piece);
			BOOST_REQUIRE_MESSAGE(result.finished, "article=" << i << " piece=" << piece);
			BOOST_REQUIRE_MESSAGE(result.decoded == payload, "article=" << i << " piece=" << piece);
		}
	}
}

BOOST_AUTO_TEST_CASE(YencDecoder_ParsesParamsAfterName)
{
	const std::string payload(10, 'h');
	const std::string article = "=ybegin line=128 name=foo.bin size=10 pcrc32=deadbeef\r\n" +
		EncodeYenc(payload) + "=yend size=10\r\n.\r\n";

	const DecodeResult result = DecodeArticle(article, 7, true);
	BOOST_REQUIRE(result.complete);
	BOOST_REQUIRE(result.finished);
	// The old nzbget rule: name runs to the end of the line; size= is still parsed from the line.
	BOOST_CHECK_EQUAL(result.name, "foo.bin size=10 pcrc32=deadbeef");
	BOOST_CHECK_EQUAL(result.fileSize, 10);
	BOOST_CHECK(result.decoded == payload);
}

BOOST_AUTO_TEST_CASE(YencDecoder_KeepsParamLikeSubstringInName)
{
	const std::string payload(11, 'h');
	const std::string article = "=ybegin line=128 size=11 name=my size=5.bin\r\n" +
		EncodeYenc(payload) + "=yend size=11\r\n.\r\n";

	const DecodeResult result = DecodeArticle(article, 7, true);
	BOOST_REQUIRE(result.complete);
	BOOST_REQUIRE(result.finished);
	BOOST_CHECK_EQUAL(result.name, "my size=5.bin");
	BOOST_CHECK_EQUAL(result.fileSize, 11);
	BOOST_CHECK(result.decoded == payload);
}

BOOST_AUTO_TEST_CASE(YencDecoder_KeepsSpacesInName)
{
	const std::string article =
		"=ybegin line=128 size=3 name=my file.bin\r\n" + EncodeYenc("Cat") + "=yend size=3\r\n.\r\n";

	const DecodeResult result = DecodeArticle(article, 5, true);
	BOOST_REQUIRE(result.complete);
	BOOST_REQUIRE(result.finished);
	BOOST_CHECK_EQUAL(result.name, "my file.bin");
}

BOOST_AUTO_TEST_CASE(YencDecoder_RejectsEndWithoutSize)
{
	const std::string payload(10, 'h');
	const std::string article =
		"=ybegin line=128 size=10 name=a.bin\r\n" + EncodeYenc(payload) + "=yend\r\n.\r\n";

	const DecodeResult result = DecodeArticle(article, 7, true);
	BOOST_REQUIRE(result.complete);
	BOOST_REQUIRE(!result.finished);
}

BOOST_AUTO_TEST_CASE(YencDecoder_AcceptsNumberAtEndOfLine)
{
	const std::string payload(10, 'h');
	const std::string article = "=ybegin size=10\r\n" + EncodeYenc(payload) + "=yend size=10\r\n.\r\n";

	const DecodeResult result = DecodeArticle(article, 7, true);
	BOOST_REQUIRE(result.complete);
	BOOST_REQUIRE(result.headerValid);
	BOOST_REQUIRE(result.finished);
	BOOST_CHECK(result.decoded == payload);
}

BOOST_AUTO_TEST_SUITE_END()
