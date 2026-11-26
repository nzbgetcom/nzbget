#include "nzbget.h"

#include "ArticleDecoder.h"
#include "DecoderTestHelpers.h"
#include <boost/test/unit_test.hpp>
#include <string>
#include <utility>

BOOST_AUTO_TEST_SUITE(UuDecoderSuite)

namespace
{
using nntp_test::Decode;
using nntp_test::DecodeAtSourceStart;
using nntp_test::Run;

// Canonical uuencode: a length byte and 6-bit digits offset by 32, zero written as '`'.
std::string EncodeUuLine(const std::string& data)
{
	BOOST_REQUIRE_LE(data.size(), 45u);

	std::string line(1, static_cast<char>(data.size() + 32));
	for (size_t i = 0; i < data.size(); i += 3)
	{
		uint32_t value = static_cast<unsigned char>(data[i]) << 16;
		if (i + 1 < data.size())
		{
			value |= static_cast<unsigned char>(data[i + 1]) << 8;
		}
		if (i + 2 < data.size())
		{
			value |= static_cast<unsigned char>(data[i + 2]);
		}
		for (int shift = 18; shift >= 0; shift -= 6)
		{
			const uint32_t digit = (value >> shift) & 0x3f;
			line += digit == 0 ? '`' : static_cast<char>(digit + 32);
		}
	}
	return line;
}
} // namespace

BOOST_AUTO_TEST_CASE(UuDecoder_DecodesOneByteAtATime)
{
	const std::string article = "begin 644 cat.txt\r\n#0V%T\r\n`\r\nend\r\n.\r\n";

	network::nntp::UuDecoder decoder;
	const Run run = Decode(decoder, article, 1);

	BOOST_CHECK(run.complete);
	BOOST_CHECK_EQUAL(run.out, "Cat");
	BOOST_CHECK_EQUAL(decoder.Meta().name, "cat.txt");
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::Finished);
}

BOOST_AUTO_TEST_CASE(UuDecoder_DecodesWholeArticleAtOnce)
{
	const std::string article = "begin 644 cat.txt\r\n#0V%T\r\n`\r\nend\r\n.\r\n";

	network::nntp::UuDecoder decoder;
	const Run run = Decode(decoder, article, article.size());

	BOOST_CHECK(run.complete);
	BOOST_CHECK_EQUAL(run.out, "Cat");
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::Finished);
}

BOOST_AUTO_TEST_CASE(UuDecoder_DecodesUtf8Name)
{
	const std::string article = "begin 644 caf\xc3\xa9.bin\r\n#0V%T\r\n`\r\nend\r\n.\r\n";

	network::nntp::UuDecoder decoder;
	const Run run = Decode(decoder, article, 1);

	BOOST_CHECK(run.complete);
	BOOST_CHECK_EQUAL(run.out, "Cat");
	BOOST_CHECK_EQUAL(decoder.Meta().name, "caf\xc3\xa9.bin");
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::Finished);
}

BOOST_AUTO_TEST_CASE(UuDecoder_DecodesLatin1Name)
{
	const std::string article = "begin 644 caf\xe9.bin\r\n#0V%T\r\n`\r\nend\r\n.\r\n";

	network::nntp::UuDecoder decoder;
	const Run run = Decode(decoder, article, 1);

	BOOST_CHECK(run.complete);
	BOOST_CHECK_EQUAL(run.out, "Cat");
	BOOST_CHECK_EQUAL(decoder.Meta().name, "caf\xc3\xa9.bin");
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::Finished);
}

BOOST_AUTO_TEST_CASE(UuDecoder_ReportsNoBinaryDataWithoutBeginLine)
{
	network::nntp::UuDecoder decoder;
	const Run run = Decode(decoder, "\r\n.\r\n", 64);

	BOOST_CHECK(run.complete);
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::NoBinaryData);
}

BOOST_AUTO_TEST_CASE(UuDecoder_RejectsArticleWithoutEndLine)
{
	const std::string article = "begin 644 cat.txt\r\n#0V%T\r\n.\r\n";

	network::nntp::UuDecoder decoder;
	const Run run = Decode(decoder, article, 64);

	BOOST_CHECK(run.complete);
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::ArticleIncomplete);
}

BOOST_AUTO_TEST_CASE(UuDecoder_FinishRequiresTerminator)
{
	const std::string article = "begin 644 cat.txt\r\n#0V%T\r\n`\r\nend\r\n";

	network::nntp::UuDecoder decoder;
	const Run run = Decode(decoder, article, 4);

	BOOST_CHECK(!run.complete);
	BOOST_CHECK_EQUAL(run.out, "Cat");
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::ArticleIncomplete);
}

BOOST_AUTO_TEST_CASE(UuDecoder_RejectsMalformedBodyLine)
{
	const std::string article = "begin 644 cat.txt\r\n!!!\r\nend\r\n.\r\n";

	network::nntp::UuDecoder decoder;
	const Run run = Decode(decoder, article, 64);

	BOOST_CHECK(run.complete);
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::InvalidHeader);
}

BOOST_AUTO_TEST_CASE(UuDecoder_IncompleteArticleIsNotComplete)
{
	network::nntp::UuDecoder decoder;
	const Run run = Decode(decoder, "begin 644 cat.txt\r\n#0V%T\r\n", 64);

	BOOST_CHECK(!run.complete);
}

BOOST_AUTO_TEST_CASE(UuDecoder_UnstuffsDotStuffedLine)
{
	// A 14-byte line starts with "." (14 + 32), so NNTP doubles the leading dot.
	const std::string payload = "0123456789abcd";
	const std::string line = EncodeUuLine(payload);
	BOOST_REQUIRE_EQUAL(line.front(), '.');

	const std::string article = "begin 644 dot.bin\r\n." + line + "\r\n`\r\nend\r\n.\r\n";

	network::nntp::UuDecoder decoder;
	const Run run = Decode(decoder, article, 3);

	BOOST_CHECK(run.complete);
	BOOST_CHECK_EQUAL(run.out, payload);
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::Finished);
}

BOOST_AUTO_TEST_CASE(UuDecoder_DecodesZeroAndMaxLengthLines)
{
	const std::string zero = "begin 644 empty.bin\r\n \r\n`\r\nend\r\n.\r\n";
	network::nntp::UuDecoder zeroDecoder;
	const Run zeroRun = Decode(zeroDecoder, zero, 7);
	BOOST_CHECK(zeroRun.complete);
	BOOST_CHECK(zeroRun.out.empty());
	BOOST_CHECK(zeroDecoder.Finish() == network::nntp::DecodeStatus::Finished);

	const std::string payload(45, 'z');
	const std::string max = "begin 644 max.bin\r\n" + EncodeUuLine(payload) + "\r\n`\r\nend\r\n.\r\n";
	network::nntp::UuDecoder maxDecoder;
	const Run maxRun = Decode(maxDecoder, max, 5);
	BOOST_CHECK(maxRun.complete);
	BOOST_CHECK_EQUAL(maxRun.out, payload);
	BOOST_CHECK(maxDecoder.Finish() == network::nntp::DecodeStatus::Finished);
}

BOOST_AUTO_TEST_CASE(UuDecoder_DecodesWithMinimalOutputBuffer)
{
	const std::string payload(45, 'm');
	const std::string article = "begin 644 min.bin\r\n" + EncodeUuLine(payload) + "\r\n`\r\nend\r\n.\r\n";

	// The decoder needs room for one whole encoded line (MAX_LINE bytes).
	network::nntp::UuDecoder decoder;
	char buf[network::nntp::UuDecoder::MinBufferSize];
	std::string decoded;
	size_t pos = 0;
	while (pos < article.size())
	{
		const network::nntp::DecodeStep step = decoder.Decode(
			{article.data() + pos, article.size() - pos}, {buf, sizeof(buf)});
		decoded.append(buf, step.produced);
		pos += step.consumed;
		if (step.complete)
		{
			break;
		}
		if (step.consumed == 0 && step.produced == 0)
		{
			BOOST_FAIL("decoder stalled at " << pos);
		}
	}

	BOOST_CHECK(decoded == payload);
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::Finished);
}

BOOST_AUTO_TEST_CASE(UuDecoder_DoesNotTreatUppercaseBeginAsBegin)
{
	const std::string article = "BEGIN 644 cat.txt\r\n#0V%T\r\n`\r\nend\r\n.\r\n";

	network::nntp::UuDecoder decoder;
	const Run run = Decode(decoder, article, 4);

	BOOST_CHECK(decoder.Meta().name.empty());
	BOOST_CHECK_EQUAL(run.out, "Cat");
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::Finished);
}

BOOST_AUTO_TEST_CASE(UuDecoder_RejectsBodyLineShorterThanDeclared)
{
	const std::string article = "begin 644 x.bin\r\nM#0V\r\nend\r\n.\r\n";

	network::nntp::UuDecoder decoder;
	const Run run = Decode(decoder, article, 64);

	BOOST_CHECK(run.complete);
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::InvalidHeader);
}

BOOST_AUTO_TEST_CASE(UuDecoder_RejectsLineLongerThanLimit)
{
	const std::string article = "begin 644 x.bin\r\n" + std::string(2000, 'A') + "\r\nend\r\n.\r\n";

	for (const size_t piece : {size_t(128), article.size()})
	{
		network::nntp::UuDecoder decoder;
		const Run run = Decode(decoder, article, piece);

		BOOST_CHECK(run.complete);
		BOOST_CHECK(run.out.empty());
		BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::InvalidHeader);
	}
}

BOOST_AUTO_TEST_CASE(UuDecoder_IgnoresLongPreambleBeforeBegin)
{
	const std::string article = std::string(2000, 'A') + "\r\nbegin 644 x.bin\r\n#0V%T\r\n`\r\nend\r\n.\r\n";

	for (const size_t piece : {size_t(128), article.size()})
	{
		network::nntp::UuDecoder decoder;
		const Run run = Decode(decoder, article, piece);

		BOOST_CHECK(run.complete);
		BOOST_CHECK_EQUAL(run.out, "Cat");
		BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::Finished);
	}
}

BOOST_AUTO_TEST_CASE(UuDecoder_DecodesInPlaceAtSourceStartAcrossChunks)
{
	std::string payload;
	for (size_t i = 0; i < 300; i++)
	{
		payload.push_back(static_cast<char>(i * 7 + 1));
	}

	std::string article = "begin 644 chunks.bin\r\n";
	for (size_t i = 0; i < payload.size(); i += 45)
	{
		article += EncodeUuLine(payload.substr(i, 45)) + "\r\n";
	}
	article += "`\r\nend\r\n.\r\n";

	// An in-place window must hold a whole encoded line; these are at or above MAX_LINE.
	for (const auto [capacity, piece] : {std::pair<size_t, size_t>{network::nntp::UuDecoder::MAX_LINE, 37},
										 {network::nntp::UuDecoder::MAX_LINE, 61},
										 {2 * network::nntp::UuDecoder::MAX_LINE, 1},
										 {64 * 1024, 4096}})
	{
		const Run run = DecodeAtSourceStart<network::nntp::UuDecoder>(article, capacity, piece);
		BOOST_REQUIRE_MESSAGE(run.complete, "capacity=" << capacity << " piece=" << piece);
		BOOST_REQUIRE_MESSAGE(run.out == payload, "capacity=" << capacity << " piece=" << piece);
	}
}

BOOST_AUTO_TEST_SUITE_END()
