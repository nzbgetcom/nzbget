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
#include "RawDecoder.h"

#include <algorithm>
#include <cstring>
#include <string_view>

namespace network::nntp
{

DecodeStep RawDecoder::Decode(std::span<const char> src, std::span<char> dest)
{
	DecodeStep step;
	if (m_complete)
	{
		step.complete = true;
		return step;
	}
	size_t destPos = 0;
	size_t pos = 0;

	// The caller keeps the tail; a partial terminator stays in src, so dest == src is safe.
	// ".." at line start is unstuffed to "."; the ".\r\n" terminator is not output.
	while (pos < src.size())
	{
		if (m_lineStart && src[pos] == '.')
		{
			const size_t left = src.size() - pos;
			if (left < 2 || (src[pos + 1] == '\r' && left < 3))
			{
				break; // the rest of a possible terminator is in the next chunk
			}
			if (src[pos + 1] == '\r' && src[pos + 2] == '\n')
			{
				m_complete = true;
				pos += 3;
				break;
			}
			if (src[pos + 1] == '.')
			{
				if (destPos >= dest.size())
				{
					break; // no room for the unstuffed dot; the caller drains and comes back
				}
				dest[destPos++] = '.';
				pos += 2;
				m_lineStart = false;
				continue;
			}
		}

		if (destPos >= dest.size())
		{
			break;
		}

		const size_t nl = IndexOf(src.subspan(pos), '\n');
		size_t take = nl == std::string_view::npos ? src.size() - pos : nl + 1;
		take = std::min(take, dest.size() - destPos);
		std::memmove(dest.data() + destPos, src.data() + pos, take);
		m_lineStart = src[pos + take - 1] == '\n';
		pos += take;
		destPos += take;
	}

	step.consumed = pos;
	step.produced = destPos;
	step.complete = m_complete;
	return step;
}

} // namespace network::nntp
