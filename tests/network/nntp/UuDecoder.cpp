#include "nzbget.h"

#include <algorithm>
#include <boost/test/unit_test.hpp>
#include <vector>
#include "ArticleDecoder.h"

BOOST_AUTO_TEST_SUITE(UuDecoderSuite)

namespace
{

struct Run
{
	std::string out;
	bool complete{false};
};

template <network::nntp::DecoderImpl T>
Run Decode(T& decoder, const std::string& article, size_t piece)
{
	Run run;
	char buf[256];
	size_t pos = 0;
	while (pos < article.size())
	{
		const size_t take = std::min(piece, article.size() - pos);
		const network::nntp::DecodeStep step = decoder.Decode({article.data() + pos, take}, {buf, sizeof(buf)});
		run.out.append(buf, step.produced);
		pos += step.consumed;
		if (step.complete)
		{
			run.complete = true;
			break;
		}
		if (step.consumed == 0 && step.produced == 0)
		{
			break;
		}
	}
	return run;
}

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

// Decodes inside one buffer: input is read at pos, output is written at outPos,
// and outPos must never pass pos.
std::string DecodeInPlace(const std::string& article, size_t piece)
{
	std::vector<char> buf(article.begin(), article.end());
	buf.resize(article.size() + 64); // a line needs 45 bytes of output room
	network::nntp::UuDecoder decoder;

	size_t pos = 0;
	size_t outPos = 0;
	while (pos < article.size())
	{
		const size_t take = std::min(piece, article.size() - pos);
		const network::nntp::DecodeStep step = decoder.Decode(
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

	// The decoder needs room for one whole line (45 bytes).
	network::nntp::UuDecoder decoder;
	char buf[45];
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

BOOST_AUTO_TEST_CASE(UuDecoder_DropsLineLongerThanLimit)
{
	const std::string article = "begin 644 x.bin\r\n" + std::string(2000, 'A') + "\r\nend\r\n.\r\n";

	network::nntp::UuDecoder decoder;
	const Run run = Decode(decoder, article, 128);

	BOOST_CHECK(run.complete);
	BOOST_CHECK(run.out.empty());
}

BOOST_AUTO_TEST_CASE(UuDecoder_DecodesInPlace)
{
	std::string payload;
	for (size_t i = 0; i < 100; i++)
	{
		payload.push_back(static_cast<char>('A' + i % 26));
	}

	std::string article = "begin 644 inplace.bin\r\n";
	for (size_t i = 0; i < payload.size(); i += 45)
	{
		article += EncodeUuLine(payload.substr(i, 45)) + "\r\n";
	}
	article += "`\r\nend\r\n.\r\n";

	for (size_t piece : {size_t(1), size_t(7), size_t(64), size_t(4096)})
	{
		BOOST_REQUIRE_MESSAGE(DecodeInPlace(article, piece) == payload, "piece=" << piece);
	}
}

BOOST_AUTO_TEST_SUITE_END()
