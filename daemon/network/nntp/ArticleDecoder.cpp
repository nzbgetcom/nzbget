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
#include "ArticleDecoder.h"

namespace network::nntp
{

namespace
{

const ArticleMeta NO_ARTICLE{};

} // namespace

ArticleFormat ArticleDecoder::DetectFormat(std::string_view line, bool rawMode)
{
	if (rawMode)
	{
		return ArticleFormat::Raw;
	}

	// The header line is recognized even when its tail has not arrived yet.
	if (line.starts_with("=ybegin "))
	{
		return ArticleFormat::Yenc;
	}

	// Dot-stuffing doubles a leading '.', which is also the length char of a 14-byte line.
	const std::string_view body = line.starts_with("..") ? line.substr(1) : line;
	if (UuDecoder::IsUuBegin(line) || UuDecoder::IsUuBody(body))
	{
		return ArticleFormat::Ux;
	}

	return ArticleFormat::Unknown;
}

void ArticleDecoder::Create(ArticleFormat format, bool crcCheck)
{
	if (format == ArticleFormat::Yenc)
	{
		m_decoder.emplace<YencDecoder>(crcCheck);
	}
	else if (format == ArticleFormat::Ux)
	{
		m_decoder.emplace<UuDecoder>();
	}
	else if (format == ArticleFormat::Raw)
	{
		m_decoder.emplace<RawDecoder>();
	}
	else
	{
		m_decoder.emplace<std::monostate>();
	}
}

DecodeStep ArticleDecoder::Decode(std::span<const char> src, std::span<char> dest)
{
	return std::visit([&](auto& decoder) -> DecodeStep {
		if constexpr (DecoderImpl<std::decay_t<decltype(decoder)>>)
		{
			return decoder.Decode(src, dest);
		}
		else
		{
			return DecodeStep{};
		}
	}, m_decoder);
}

DecodeStatus ArticleDecoder::Finish()
{
	return std::visit([](auto& decoder) -> DecodeStatus {
		if constexpr (DecoderImpl<std::decay_t<decltype(decoder)>>)
		{
			return decoder.Finish();
		}
		else
		{
			return DecodeStatus::NoBinaryData;
		}
	}, m_decoder);
}

const ArticleMeta& ArticleDecoder::Meta() const
{
	return std::visit([](const auto& decoder) -> const ArticleMeta& {
		if constexpr (DecoderImpl<std::decay_t<decltype(decoder)>>)
		{
			return decoder.Meta();
		}
		else
		{
			return NO_ARTICLE;
		}
	}, m_decoder);
}

} // namespace network::nntp
