#include "nzbget.h"

#include "ArticleDecoder.h"
#include "DecoderTestHelpers.h"
#include <boost/test/unit_test.hpp>
#include <string>
#include <utility>

BOOST_AUTO_TEST_SUITE(RawDecoderSuite)

namespace
{
using nntp_test::Decode;
using nntp_test::DecodeAtSourceStart;
using nntp_test::Run;
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

BOOST_AUTO_TEST_CASE(RawDecoder_UnstuffsDotAtLineStart)
{
	network::nntp::RawDecoder decoder;
	const Run run = Decode(decoder, "a\r\n..b\r\n.\r\n", 64);

	BOOST_CHECK(run.complete);
	BOOST_CHECK_EQUAL(run.out, "a\r\n.b\r\n");
}

BOOST_AUTO_TEST_CASE(RawDecoder_UnstuffsWithOneByteOutputBuffer)
{
	const std::string article = "..\r\n.\r\n";
	network::nntp::RawDecoder decoder;
	char buf[1];
	std::string out;
	size_t pos = 0;
	while (pos < article.size())
	{
		const network::nntp::DecodeStep step =
			decoder.Decode({article.data() + pos, article.size() - pos}, {buf, sizeof(buf)});
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

	BOOST_CHECK_EQUAL(out, ".\r\n");
	BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::Finished);
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

BOOST_AUTO_TEST_CASE(RawDecoder_DecodesInPlaceAtSourceStartAcrossChunks)
{
	const std::string article = "hello\r\n..world\r\n.\r\n";

	for (const auto& [capacity, piece] : {std::pair{size_t(8), size_t(3)}, std::pair{size_t(16), size_t(1)},
			std::pair{size_t(64 * 1024), size_t(4096)}})
	{
		const Run run = DecodeAtSourceStart<network::nntp::RawDecoder>(article, capacity, piece);
		BOOST_REQUIRE_MESSAGE(run.complete, "capacity=" << capacity << " piece=" << piece);
		BOOST_REQUIRE_EQUAL(run.out, "hello\r\n.world\r\n");
	}
}

BOOST_AUTO_TEST_CASE(RawDecoder_InSituWaitsForTerminatorLookalikeAcrossChunks)
{
	// The window ends on ".\r", which only the next byte can resolve as data.
	const std::string article = "a\r\n.\rx\r\n.\r\n";

	const Run run = DecodeAtSourceStart<network::nntp::RawDecoder>(article, 5, 5);
	BOOST_REQUIRE(run.complete);
	BOOST_REQUIRE_EQUAL(run.out, "a\r\n.\rx\r\n");
}

BOOST_AUTO_TEST_CASE(RawDecoder_DecodesWithMinimalWindow)
{
	// ".\r" at the start of a full window must not stall: MinBufferSize leaves room for the third byte.
	const std::string article = ".\rx\r\n.\r\n";

	for (size_t piece : {size_t(1), size_t(3)})
	{
		const Run run =
			DecodeAtSourceStart<network::nntp::RawDecoder>(article, network::nntp::RawDecoder::MinBufferSize, piece);
		BOOST_REQUIRE_MESSAGE(run.complete, "piece=" << piece);
		BOOST_REQUIRE_EQUAL(run.out, ".\rx\r\n");
	}
}

BOOST_AUTO_TEST_SUITE_END()
