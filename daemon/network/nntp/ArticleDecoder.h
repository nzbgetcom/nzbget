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

#ifndef NETWORK_NNTP_ARTICLE_DECODER_H
#define NETWORK_NNTP_ARTICLE_DECODER_H

#include <algorithm>
#include <concepts>
#include <span>
#include <string_view>
#include <variant>

#include "DecodeTypes.h"
#include "RawDecoder.h"
#include "UuDecoder.h"
#include "YencDecoder.h"

namespace network::nntp
{

/// MinBufferSize: smallest free dest space with which Decode() always makes progress.
template <typename T>
concept DecoderImpl = std::movable<T> &&
	requires(T& decoder, std::span<const char> src, std::span<char> dest) {
		{ T::MinBufferSize } -> std::convertible_to<size_t>;
		{ decoder.Decode(src, dest) } -> std::same_as<DecodeStep>;
		{ decoder.Finish() } -> std::same_as<DecodeStatus>;
		{ decoder.Meta() } -> std::same_as<const ArticleMeta&>;
	};

/// Decodes one article at a time without heap allocations. dest may start at src.data().
class ArticleDecoder
{
public:
	/// Worst case over all formats: the format is known only after Create().
	static constexpr size_t MinBufferSize = std::max({YencDecoder::MinBufferSize, UuDecoder::MinBufferSize, RawDecoder::MinBufferSize});

	/// Picks the format from the first line of the article. Raw mode always yields Raw.
	/// The line must be complete, including its line ending: a cut uu line is not recognized.
	/// ArticleFormat::Unknown means the line is preamble and can be dropped.
	static ArticleFormat DetectFormat(std::string_view line, bool rawMode);

	/// Replaces any previous article and invalidates every ArticleMeta reference taken before.
	/// Unknown leaves the decoder idle: nothing is decoded.
	void Create(ArticleFormat format, bool crcCheck);

	/// An idle decoder (no Create or Unknown) returns an empty step every time, so a caller
	/// must stop when a step has neither consumed nor produced bytes and is not complete.
	/// May throw std::bad_alloc while converting the article name; catch it at the task boundary.
	DecodeStep Decode(std::span<const char> src, std::span<char> dest);

	/// Self-check after complete: header consistency, declared sizes, checksum.
	DecodeStatus Finish();

	/// The reference points into the active format and is valid only until the next Create().
	const ArticleMeta& Meta() const;

private:
	std::variant<std::monostate, YencDecoder, UuDecoder, RawDecoder> m_decoder;
};

static_assert(DecoderImpl<YencDecoder>);
static_assert(DecoderImpl<UuDecoder>);
static_assert(DecoderImpl<RawDecoder>);
static_assert(DecoderImpl<ArticleDecoder>);

} // namespace network::nntp

#endif
