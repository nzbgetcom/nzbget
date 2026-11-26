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

#ifndef NETWORK_NNTP_RAW_DECODER_H
#define NETWORK_NNTP_RAW_DECODER_H

#include <span>

#include "DecodeTypes.h"

namespace network::nntp
{

/// Pass-through for raw (--raw) articles; removes NNTP dot-stuffing and the end sequence ".\r\n".
class RawDecoder final
{
public:
	/// A possible ".\r\n" terminator must fit; 3 bytes guarantee progress.
	static constexpr size_t MinBufferSize = 3;

	RawDecoder() noexcept { m_meta.format = ArticleFormat::Raw; }

	DecodeStep Decode(std::span<const char> src, std::span<char> dest);
	DecodeStatus Finish() { return m_complete ? DecodeStatus::Finished : DecodeStatus::ArticleIncomplete; }
	const ArticleMeta& Meta() const noexcept { return m_meta; }

private:
	bool m_complete{false};
	bool m_lineStart{true};
	ArticleMeta m_meta;
};

} // namespace network::nntp

#endif
