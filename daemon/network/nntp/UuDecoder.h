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

#ifndef NETWORK_NNTP_UU_DECODER_H
#define NETWORK_NNTP_UU_DECODER_H

#include <cstddef>
#include <span>
#include <string_view>

#include "DecodeTypes.h"

namespace network::nntp
{

/// Streaming uuencode decoder; keeps at most one unterminated line. complete waits for the NNTP end sequence.
class UuDecoder final
{
public:
	static constexpr size_t MAX_LINE = 1024;
	static constexpr size_t MAX_DECODED_LINE = 45;

	/// A whole decoded line must fit in dest.
	static constexpr size_t MinBufferSize = MAX_DECODED_LINE;

	UuDecoder() noexcept { m_meta.format = ArticleFormat::Ux; }

	DecodeStep Decode(std::span<const char> src, std::span<char> dest);
	DecodeStatus Finish();
	const ArticleMeta& Meta() const noexcept { return m_meta; }

	/// UUencode "begin <octal mode> <name>" line.
	static bool IsUuBegin(std::string_view line);
	/// UUencode body line: a length character followed by the encoded groups.
	static bool IsUuBody(std::string_view line);

private:
	void HandleLine(std::string_view line, std::span<char> dest, size_t& destPos);

	bool m_body{false};
	bool m_end{false};
	bool m_complete{false};
	bool m_valid{true};
	bool m_discardLine{false};
	FixedBuffer<MAX_LINE> m_line;
	ArticleMeta m_meta;
};

} // namespace network::nntp

#endif
