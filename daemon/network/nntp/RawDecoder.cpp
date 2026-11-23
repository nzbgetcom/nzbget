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

namespace network::nntp
{

DecodeStep RawDecoder::Decode(std::span<const char> src, std::span<char> dest)
{
	DecodeStep step;
	size_t destPos = 0;
	size_t pos = 0;

	// The end sequence is ".\r\n" at the start of a line; the CRLF that ends the last
	// body line stays in the output.
	while (pos < src.size())
	{
		// A held prefix that turned out to be body data is flushed one byte at a time,
		// so a tiny output buffer can never stall the decoder.
		if (m_pendingData)
		{
			if (destPos >= dest.size())
			{
				break;
			}
			dest[destPos++] = m_pending[m_pendingPos++];
			if (m_pendingPos == m_pendingLen)
			{
				m_pendingData = false;
				m_pendingLen = 0;
				m_pendingPos = 0;
			}
			continue;
		}

		const char ch = src[pos];

		if (m_pendingLen > 0)
		{
			const char expected = m_pendingLen == 1 ? '\r' : '\n';
			if (ch == expected)
			{
				++pos;
				if (m_pendingLen == 2)
				{
					m_complete = true;
					break;
				}
				m_pending[m_pendingLen++] = ch;
				continue;
			}

			m_pendingData = true; // not the end sequence: flush it, then look at ch again
			continue;
		}

		if (m_lineStart && ch == '.')
		{
			m_pending[m_pendingLen++] = ch;
			m_lineStart = false;
			++pos;
			continue;
		}

		if (destPos + 1 > dest.size())
		{
			break;
		}
		dest[destPos++] = ch;
		m_lineStart = ch == '\n';
		++pos;
	}

	step.consumed = pos;
	step.produced = destPos;
	step.complete = m_complete;
	return step;
}

} // namespace network::nntp
