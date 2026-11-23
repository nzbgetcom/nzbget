#include "nzbget.h"

#include <algorithm>
#include <boost/test/unit_test.hpp>
#include <vector>
#include "ArticleDecoder.h"

BOOST_AUTO_TEST_SUITE(RawDecoderSuite)

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

// Decodes inside one buffer: input is read at pos, output is written at outPos,
// and outPos must never pass pos.
std::string DecodeInPlace(const std::string& article, size_t piece)
{
	std::vector<char> buf(article.begin(), article.end());
	buf.resize(article.size() + 64);
	network::nntp::RawDecoder decoder;

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

BOOST_AUTO_TEST_CASE(RawDecoder_PassesBodyAndDropsTerminator)
{
	network::nntp::RawDecoder decoder;
	const Run run = Decode(decoder, "hello\r\nworld\r\n.\r\n", 64);

	BOOST_CHECK(run.complete);
	BOOST_CHECK_EQUAL(run.out, "hello\r\nworld\r\n");
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::Finished);
	BOOST_CHECK(decoder.Meta().format == network::nntp::ArticleFormat::Raw);
}

BOOST_AUTO_TEST_CASE(RawDecoder_SplitTerminatorAcrossChunks)
{
	network::nntp::RawDecoder decoder;
	const Run run = Decode(decoder, "hello\r\nworld\r\n.\r\n", 1);

	BOOST_CHECK(run.complete);
	BOOST_CHECK_EQUAL(run.out, "hello\r\nworld\r\n");
}

BOOST_AUTO_TEST_CASE(RawDecoder_KeepsPartialTerminatorLookalikes)
{
	network::nntp::RawDecoder decoder;
	const Run run = Decode(decoder, "a\rb\r\n.x\r\n.\r\n", 1);

	BOOST_CHECK(run.complete);
	BOOST_CHECK_EQUAL(run.out, "a\rb\r\n.x\r\n");
}

BOOST_AUTO_TEST_CASE(RawDecoder_KeepsDotStuffing)
{
	network::nntp::RawDecoder decoder;
	const Run run = Decode(decoder, "a\r\n..b\r\n.\r\n", 64);

	BOOST_CHECK(run.complete);
	BOOST_CHECK_EQUAL(run.out, "a\r\n..b\r\n");
}

BOOST_AUTO_TEST_CASE(RawDecoder_MissingTerminatorIsIncomplete)
{
	network::nntp::RawDecoder decoder;
	const Run run = Decode(decoder, "hello\r\n", 64);

	BOOST_CHECK(!run.complete);
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::ArticleIncomplete);
}

BOOST_AUTO_TEST_CASE(RawDecoder_EmptyBody)
{
	network::nntp::RawDecoder decoder;
	const Run run = Decode(decoder, ".\r\n", 8);

	BOOST_CHECK(run.complete);
	BOOST_CHECK(run.out.empty());
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::Finished);
}

BOOST_AUTO_TEST_CASE(RawDecoder_OneByteOutputBuffer)
{
	const std::string article = "ab\r\ncd\r\n.\r\n";
	network::nntp::RawDecoder decoder;
	char buf[1];
	std::string out;
	size_t pos = 0;
	while (pos < article.size())
	{
		const network::nntp::DecodeStep step = decoder.Decode(
			{article.data() + pos, article.size() - pos}, {buf, sizeof(buf)});
		out.append(buf, step.produced);
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

	BOOST_CHECK_EQUAL(out, article.substr(0, article.size() - 3));
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::Finished);
}

BOOST_AUTO_TEST_CASE(RawDecoder_LfOnlyTerminatorIsNotAccepted)
{
	network::nntp::RawDecoder decoder;
	const Run run = Decode(decoder, "x\n.\n", 1);

	BOOST_CHECK(!run.complete);
	BOOST_CHECK_EQUAL(run.out, "x\n.\n");
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::ArticleIncomplete);
}

BOOST_AUTO_TEST_CASE(RawDecoder_DotAfterLoneCrIsData)
{
	network::nntp::RawDecoder decoder;
	const Run run = Decode(decoder, "x\r.\r\n", 1);

	BOOST_CHECK(!run.complete);
	BOOST_CHECK_EQUAL(run.out, "x\r.\r\n");
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::ArticleIncomplete);
}

BOOST_AUTO_TEST_CASE(RawDecoder_TinyOutputBufferFlushesFalseTerminator)
{
	// ".\r" at the start of a line is held until the next byte proves it is data.
	// A one-byte output buffer must flush it across calls instead of stalling.
	const std::string article = ".\rx\r\n.\r\n";
	network::nntp::RawDecoder decoder;
	char buf[1];
	std::string out;
	size_t pos = 0;
	while (pos < article.size())
	{
		const network::nntp::DecodeStep step = decoder.Decode(
			{article.data() + pos, article.size() - pos}, {buf, sizeof(buf)});
		out.append(buf, step.produced);
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

	BOOST_CHECK_EQUAL(out, ".\rx\r\n");
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::Finished);
}

BOOST_AUTO_TEST_CASE(RawDecoder_DecodesInPlace)
{
	const std::string article = "hello\r\nworld\r\n.\r\n";

	for (size_t piece : {size_t(1), size_t(2), size_t(64)})
	{
		BOOST_REQUIRE_EQUAL(DecodeInPlace(article, piece), "hello\r\nworld\r\n");
	}
}

BOOST_AUTO_TEST_SUITE_END()
