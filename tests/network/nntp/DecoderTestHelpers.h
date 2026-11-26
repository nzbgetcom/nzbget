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

#ifndef NNTP_DECODER_TEST_HELPERS_H
#define NNTP_DECODER_TEST_HELPERS_H

#include <algorithm>
#include <boost/test/unit_test.hpp>
#include <cstring>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ArticleDecoder.h"

namespace nntp_test
{

struct Run
{
	std::string out;
	bool complete{false};
};

/// Feeds piece bytes at a time into a window that keeps the unconsumed tail, like the
/// async caller does; output goes to a separate buffer.
template <network::nntp::DecoderImpl T> Run Decode(T& decoder, const std::string& article, size_t piece)
{
	Run run;
	std::vector<char> src(article.size() + 1);
	std::vector<char> dest(std::max<size_t>(T::MinBufferSize, 256));
	size_t fed = 0;
	size_t end = 0;
	while (fed < article.size() || end > 0)
	{
		if (fed < article.size())
		{
			const size_t take = std::min(piece, article.size() - fed);
			std::memcpy(src.data() + end, article.data() + fed, take);
			end += take;
			fed += take;
		}

		const network::nntp::DecodeStep step =
			decoder.Decode(std::span<const char>(src.data(), end), std::span<char>(dest.data(), dest.size()));
		run.out.append(dest.data(), step.produced);
		std::memmove(src.data(), src.data() + step.consumed, end - step.consumed);
		end -= step.consumed;
		if (step.complete)
		{
			run.complete = true;
			break;
		}
		if (step.consumed == 0 && step.produced == 0 && fed == article.size())
		{
			break; // an incomplete article: no more input to add
		}
	}
	return run;
}

/// Mirrors ArticleFetcher: input and output share one window that starts at the
/// first unconsumed byte, and the unconsumed tail is moved to the front between calls.
template <network::nntp::DecoderImpl T, typename... Args>
Run DecodeAtSourceStart(const std::string& article, size_t capacity, size_t piece, Args&&... args)
{
	Run run;
	std::vector<char> buf(capacity);
	T decoder{std::forward<Args>(args)...};

	size_t fed = 0;
	size_t end = 0;
	for (;;)
	{
		if (end < capacity && fed < article.size())
		{
			const size_t take = std::min({piece, capacity - end, article.size() - fed});
			std::memcpy(buf.data() + end, article.data() + fed, take);
			end += take;
			fed += take;
		}

		const std::span<char> window(buf.data(), end);
		const network::nntp::DecodeStep step = decoder.Decode(window, window);
		run.out.append(buf.data(), step.produced);
		std::memmove(buf.data(), buf.data() + step.consumed, end - step.consumed);
		end -= step.consumed;

		if (step.complete)
		{
			run.complete = true;
			break;
		}
		if (step.consumed == 0 && step.produced == 0 && (fed == article.size() || end == capacity))
		{
			BOOST_FAIL("decoder stalled at " << fed);
		}
	}
	return run;
}

} // namespace nntp_test

#endif // NNTP_DECODER_TEST_HELPERS_H
