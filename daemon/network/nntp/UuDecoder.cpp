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
#include "UuDecoder.h"
#include "Util.h"

#include <array>

namespace network::nntp
{

namespace
{

/// uuencode digit: 0..63, backtick is an alternative zero. -1 marks a broken line.
constexpr std::array<int8_t, 256> MakeUuTable() noexcept
{
	std::array<int8_t, 256> table{};
	table.fill(-1);
	for (int ch = ' '; ch <= '_'; ++ch)
	{
		table[static_cast<size_t>(ch)] = static_cast<int8_t>((ch - ' ') & 077);
	}
	table[static_cast<size_t>('`')] = 0;
	return table;
}

constexpr std::array<int8_t, 256> UU_TABLE = MakeUuTable();

int UuChar(char ch) noexcept
{
	return UU_TABLE[static_cast<unsigned char>(ch)];
}

std::string_view StripEol(std::string_view line) noexcept
{
	while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
	{
		line.remove_suffix(1);
	}
	return line;
}

} // namespace

bool UuDecoder::IsUuBegin(std::string_view line)
{
	if (!line.starts_with("begin "))
	{
		return false;
	}

	size_t pos = 6;
	while (pos < line.size() && line[pos] >= '0' && line[pos] <= '7')
	{
		pos++;
	}
	return pos > 6 && pos < line.size() && line[pos] == ' ';
}

bool UuDecoder::IsUuBody(std::string_view line)
{
	line = StripEol(line);
	if (line.size() < 2 || line.size() > 61)
	{
		return false;
	}

	const int length = UuChar(line[0]);
	if (length < 0 || length > 45)
	{
		return false;
	}

	const size_t groups = (static_cast<size_t>(length) + 2) / 3;
	return groups * 4 + 1 == line.size();
}

DecodeStep UuDecoder::Decode(std::span<const char> src, std::span<char> dest)
{
	DecodeStep step;
	if (m_complete)
	{
		step.complete = true;
		return step;
	}
	size_t destPos = 0;
	size_t pos = 0;

	// A line is decoded only when fully in src; the caller keeps the tail, so dest == src is safe.
	while (pos < src.size())
	{
		if (m_discardLine)
		{
			const size_t nl = IndexOf(src.subspan(pos), '\n');
			if (nl == std::string_view::npos)
			{
				pos = src.size();
				break;
			}
			pos += nl + 1;
			m_discardLine = false;
			continue;
		}

		const size_t nl = IndexOf(src.subspan(pos), '\n');
		if (nl == std::string_view::npos)
		{
			if (src.size() - pos < MAX_LINE)
			{
				break; // the rest of the line is in the next chunk
			}
			// A line longer than any valid uuencode line is dropped. Before begin it is
			// a preamble; in the body it makes the article invalid, so Finish() cannot
			// report a truncated payload as complete.
			if (m_body)
			{
				m_valid = false;
			}
			pos = src.size();
			m_discardLine = true;
			continue;
		}

		const size_t take = nl + 1;
		if (take > MAX_LINE)
		{
			if (m_body)
			{
				m_valid = false;
			}
			pos += take;
			continue;
		}
		if (destPos + take > dest.size())
		{
			break; // the whole line must fit in dest; the caller drains and comes back
		}

		HandleLine(std::string_view(src.data() + pos, take), dest, destPos);
		pos += take;
		if (m_complete)
		{
			break;
		}
	}

	step.consumed = pos;
	step.produced = destPos;
	step.complete = m_complete;
	return step;
}

void UuDecoder::HandleLine(std::string_view line, std::span<char> dest, size_t& destPos)
{
	std::string_view content = StripEol(line);

	if (content == ".")
	{
		m_complete = true;
		return;
	}

	// NNTP dot-stuffing: a body line that starts with '.' arrives as "..".
	if (content.starts_with(".."))
	{
		content.remove_prefix(1);
	}

	if (!m_body)
	{
		if (IsUuBegin(content))
		{
			const size_t nameStart = content.find(' ', 6);
			if (nameStart != std::string_view::npos)
			{
				const std::string_view name = content.substr(nameStart + 1);
				WebUtil::DecodeName(name, m_meta.name);
			}
			m_body = true;
			return;
		}
		if (!IsUuBody(content))
		{
			return; // preamble before "begin" or the first body line
		}
		m_body = true;
	}

	if (content.empty())
	{
		return;
	}

	if (content == "end" || content.starts_with("end ") || content[0] == '`')
	{
		m_end = true;
		return;
	}

	if (m_end)
	{
		return;
	}

	const int length = UuChar(content[0]);
	if (length < 0 || length > static_cast<int>(MAX_DECODED_LINE))
	{
		m_valid = false;
		return;
	}

	const size_t groups = (static_cast<size_t>(length) + 2) / 3;
	if (content.size() < groups * 4 + 1)
	{
		m_valid = false;
		return;
	}

	const char* encoded = content.data() + 1;
	int remaining = length;
	while (remaining > 0)
	{
		const int a = UuChar(encoded[0]);
		const int b = UuChar(encoded[1]);
		const int c = UuChar(encoded[2]);
		const int d = UuChar(encoded[3]);
		if (a < 0 || b < 0 || c < 0 || d < 0)
		{
			m_valid = false;
			return;
		}

		dest[destPos++] = static_cast<char>((a << 2) | (b >> 4));
		if (--remaining == 0)
		{
			break;
		}
		dest[destPos++] = static_cast<char>((b << 4) | (c >> 2));
		if (--remaining == 0)
		{
			break;
		}
		dest[destPos++] = static_cast<char>((c << 6) | d);
		--remaining;
		encoded += 4;
	}
}

DecodeStatus UuDecoder::Finish()
{
	if (!m_body)
	{
		return DecodeStatus::NoBinaryData;
	}
	if (!m_valid)
	{
		return DecodeStatus::InvalidHeader;
	}
	if (!m_end || !m_complete)
	{
		// Without the "end" line or the NNTP terminator a cut-off article would pass.
		return DecodeStatus::ArticleIncomplete;
	}
	return DecodeStatus::Finished;
}

} // namespace network::nntp
