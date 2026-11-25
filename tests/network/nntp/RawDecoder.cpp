#include "nzbget.h"

#include "ArticleDecoder.h"
#include "DecoderTestHelpers.h"
#include <algorithm>
#include <boost/test/unit_test.hpp>
#include <random>
#include <span>
#include <string>
#include <utility>
#include <vector>

BOOST_AUTO_TEST_SUITE(RawDecoderSuite)

namespace
{
using nntp_test::Decode;
using nntp_test::DecodeAtSourceStart;
using nntp_test::Run;

/// Whole-article reference: ".." at line start becomes "."; ".\r\n" at line start ends the article.
Run ReferenceRaw(const std::string& article)
{
	Run ref;
	bool lineStart = true;
	size_t i = 0;
	while (i < article.size())
	{
		if (lineStart && article[i] == '.')
		{
			if (article.compare(i, 3, ".\r\n") == 0)
			{
				ref.complete = true;
				return ref;
			}
			if (i + 1 < article.size() && article[i + 1] == '.')
			{
				ref.out += '.';
				i += 2;
				lineStart = false;
				continue;
			}
		}
		lineStart = article[i] == '\n';
		ref.out += article[i++];
	}
	return ref;
}

/// Feeds piece bytes at a time and writes output in dest of destSize bytes, keeping the unconsumed tail.
Run DecodeFed(const std::string& article, size_t piece, size_t destSize)
{
	network::nntp::RawDecoder decoder;
	Run run;
	std::string src;
	std::vector<char> dest(destSize);
	size_t fed = 0;
	for (;;)
	{
		const size_t take = std::min(piece, article.size() - fed);
		src.append(article, fed, take);
		fed += take;
		const network::nntp::DecodeStep step = decoder.Decode(
			std::span<const char>(src.data(), src.size()), std::span<char>(dest.data(), dest.size()));
		run.out.append(dest.data(), step.produced);
		src.erase(0, step.consumed);
		if (step.complete)
		{
			run.complete = true;
			break;
		}
		if (step.consumed == 0 && step.produced == 0 && fed == article.size())
		{
			break;
		}
	}
	return run;
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

BOOST_AUTO_TEST_CASE(RawDecoder_UnstuffsDotRunsAtLineStart)
{
	network::nntp::RawDecoder decoder;
	const Run run = Decode(decoder, "..x\r\n...\r\n.\r\n", 64);

	BOOST_CHECK(run.complete);
	BOOST_CHECK_EQUAL(run.out, ".x\r\n..\r\n");
}

BOOST_AUTO_TEST_CASE(RawDecoder_EmptyLineBeforeTerminator)
{
	network::nntp::RawDecoder decoder;
	const Run run = Decode(decoder, "\r\n.\r\n", 2);

	BOOST_CHECK(run.complete);
	BOOST_CHECK_EQUAL(run.out, "\r\n");
}

BOOST_AUTO_TEST_CASE(RawDecoder_PassesNulAndHighBytes)
{
	network::nntp::RawDecoder decoder;
	const Run run = Decode(decoder, std::string("\0\x80\xff\r\n.\r\n", 8), 1);

	BOOST_CHECK(run.complete);
	BOOST_CHECK_EQUAL(run.out, std::string("\0\x80\xff\r\n", 5));
}

BOOST_AUTO_TEST_CASE(RawDecoder_StopsConsumingAtTerminator)
{
	network::nntp::RawDecoder decoder;
	const std::string article = "a\r\n.\r\nTAIL";
	char dest[16];
	const network::nntp::DecodeStep step = decoder.Decode(
		std::span<const char>(article.data(), article.size()), std::span<char>(dest));

	BOOST_CHECK(step.complete);
	BOOST_CHECK_EQUAL(step.consumed, size_t(6));
	BOOST_CHECK_EQUAL(step.produced, size_t(3));
}

BOOST_AUTO_TEST_CASE(RawDecoder_TrailingDotIsIncomplete)
{
	for (const char* article : {"a\r\n.", "a\r\n.\r"})
	{
		network::nntp::RawDecoder decoder;
		const Run run = Decode(decoder, article, 64);

		BOOST_CHECK(!run.complete);
		BOOST_CHECK(decoder.Finish() == network::nntp::DecodeStatus::ArticleIncomplete);
	}
}

BOOST_AUTO_TEST_CASE(RawDecoder_MatchesReferenceOnRandomInput)
{
	// Bytes weighted toward dot, CR and LF so line starts and terminator lookalikes are common.
	const std::string alphabet(".\r\n.a\0\x80\xff", 8);
	std::mt19937 rng(20260626);
	for (int n = 0; n < 3000; ++n)
	{
		std::string article;
		const size_t length = rng() % 24;
		for (size_t i = 0; i < length; ++i)
		{
			article += alphabet[rng() % alphabet.size()];
		}
		if (rng() % 2)
		{
			article += "\r\n.\r\n";
		}

		const Run ref = ReferenceRaw(article);
		for (size_t piece : {size_t(1), size_t(2), size_t(3), size_t(7), size_t(4096)})
		{
			for (size_t destSize : {network::nntp::RawDecoder::MinBufferSize, size_t(256)})
			{
				const Run run = DecodeFed(article, piece, destSize);
				BOOST_REQUIRE_MESSAGE(run.complete == ref.complete,
					"article #" << n << " piece=" << piece << " dest=" << destSize);
				if (ref.complete)
				{
					BOOST_REQUIRE_MESSAGE(run.out == ref.out, "article #" << n << " piece=" << piece << " dest=" << destSize);
				}
			}
			if (!ref.complete)
			{
				continue;
			}
			for (size_t capacity : {network::nntp::RawDecoder::MinBufferSize, size_t(4), size_t(64)})
			{
				const Run run = DecodeAtSourceStart<network::nntp::RawDecoder>(article, capacity, piece);
				BOOST_REQUIRE_MESSAGE(run.complete && run.out == ref.out,
					"article #" << n << " in-situ capacity=" << capacity << " piece=" << piece);
			}
		}
	}
}

BOOST_AUTO_TEST_SUITE_END()
