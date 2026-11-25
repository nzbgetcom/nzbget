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
#include "YencDecoder.h"
#include "Util.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <system_error>

namespace
{

/// rapidyenc_crc loads words without aligning short inputs, so the unaligned head goes through a copy.
uint32_t UpdateCrc(uint32_t crc, const char* data, size_t size) noexcept
{
	constexpr size_t ALIGN = alignof(uint64_t);
	alignas(ALIGN) std::array<char, ALIGN> head{};
	const size_t headSize = std::min(size, (ALIGN - reinterpret_cast<uintptr_t>(data) % ALIGN) % ALIGN);
	if (headSize > 0)
	{
		std::memcpy(head.data(), data, headSize);
		crc = rapidyenc_crc(head.data(), headSize, crc);
		data += headSize;
		size -= headSize;
	}
	return size > 0 ? rapidyenc_crc(data, size, crc) : crc;
}

/// Matches a key only as a whole token (line start or after a space) and returns its value.
const char* FindParam(const char* begin, const char* end, std::string_view key) noexcept
{
	const char* pos = begin;
	while ((pos = std::search(pos, end, key.begin(), key.end())) != end)
	{
		if (pos == begin || pos[-1] == ' ')
		{
			return pos + key.size();
		}
		++pos;
	}
	return nullptr;
}

/// from_chars stops at the first non-digit, so a value ends at the next space or at the line end.
template <typename T> bool ParseNumber(const char* begin, const char* end, T& out, int base = 10) noexcept
{
	return std::from_chars(begin, end, out, base).ec == std::errc();
}

template <typename T> bool ParseParam(const char* begin, const char* end, std::string_view key, T& out, int base = 10) noexcept
{
	const char* value = FindParam(begin, end, key);
	return value && ParseNumber(value, end, out, base);
}

/// A missing key is fine; a present key must hold a non-negative number, written to out only then.
bool ReadSize(const char* begin, const char* end, std::string_view key, int64_t& out) noexcept
{
	const char* value = FindParam(begin, end, key);
	if (!value)
	{
		return true;
	}
	int64_t size = 0;
	if (!ParseNumber(value, end, size) || size < 0)
	{
		return false;
	}
	out = size;
	return true;
}

enum class LineStart
{
	NeedMore,
	Control,
	Data
};

/// Decides whether the bytes after "=y" are a control keyword, a prefix that still needs input, or data.
LineStart ClassifyLine(std::string_view afterY) noexcept
{
	for (const std::string_view keyword : {"begin", "part", "end"})
	{
		const size_t n = std::min(afterY.size(), keyword.size());
		if (afterY.substr(0, n) != keyword.substr(0, n))
		{
			continue;
		}
		if (afterY.size() <= keyword.size())
		{
			return LineStart::NeedMore;
		}
		const char sep = afterY[keyword.size()];
		if (sep == ' ' || sep == '\r' || sep == '\n')
		{
			return LineStart::Control;
		}
		return LineStart::Data;
	}
	return LineStart::Data;
}

} // namespace

namespace network::nntp
{

YencDecoder::YencDecoder(bool crcCheck) noexcept : m_crcCheck(crcCheck)
{
	m_meta.format = ArticleFormat::Yenc;
}

void YencDecoder::ParseControlLine(std::string_view line)
{
	// Only lines the classifier passed here: keyword followed by a space or line end.
	const char* const begin = line.data();
	const char* const end = begin + line.size();

	if (line.starts_with("=ybegin"))
	{
		if (m_stage != Stage::Header)
		{
			m_headerValid = false;
			return;
		}
		m_stage = Stage::Body;
		const char* nameValue = FindParam(begin, end, "name=");
		// part= counts only before name=: a file name may contain "part=".
		if (m_part == Part::None && FindParam(begin, nameValue ? nameValue - 5 : end, "part="))
		{
			m_part = Part::Declared;
		}

		if (!ReadSize(begin, end, "size=", m_meta.fileSize))
		{
			m_headerValid = false;
		}

		if (const char* value = nameValue)
		{
			// Name runs to the end of the line; only an appended "=ypart " ends it, and that suffix is parsed as well.
			constexpr std::string_view YPART = "=ypart ";
			const char* nameEnd = std::search(value, end, YPART.begin(), YPART.end());
			while (nameEnd > value && nameEnd[-1] == ' ')
			{
				--nameEnd;
			}
			if (nameEnd < end)
			{
				ParseControlLine(std::string_view(nameEnd, end - nameEnd));
			}
			WebUtil::DecodeName(std::string_view(value, nameEnd - value), m_meta.name);
		}

		if (m_part == Part::None)
		{
			m_meta.offset = 0;
			m_meta.size = m_meta.fileSize;
		}
		return;
	}

	if (line.starts_with("=ypart"))
	{
		int64_t first = 0;
		int64_t last = 0;
		// Data already decoded cannot be moved, so a part line after it or a second one is refused.
		if (!ParseParam(begin, end, "begin=", first) || !ParseParam(begin, end, "end=", last) ||
			first < 1 || last < first || m_part == Part::Ranged || m_produced > 0)
		{
			m_headerValid = false;
			return;
		}

		// The =ypart range is the truth even when =ybegin size= carries the part size.
		m_part = Part::Ranged;
		m_meta.offset = first - 1;
		m_meta.size = last - first + 1;
		return;
	}

	if (m_stage == Stage::Body)
	{
		m_stage = Stage::Trailer;
	}
	if (!ReadSize(begin, end, "size=", m_endSize))
	{
		m_headerValid = false;
	}
	// On a part crc32= is the whole-file CRC, so only pcrc32= applies there.
	const std::string_view crcName = Multipart() ? "pcrc32=" : "crc32=";
	if (const char* value = FindParam(begin, end, crcName))
	{
		uint32_t crc = 0;
		if (!ParseNumber(value, end, crc, 16))
		{
			m_headerValid = false;
		}
		else
		{
			m_meta.crc = crc;
			m_meta.hasCrc = true;
		}
	}
}

YencDecoder::DataStep YencDecoder::DecodeData(std::span<const char> src, std::span<char> dest) noexcept
{
	DataStep step;

	// rapidyenc has no output length parameter: it writes at most one byte per
	// input byte, so the input slice must not exceed the free output space.
	// It decodes in-situ, so dest may be the very buffer src comes from.
	const size_t take = std::min(src.size(), dest.size());
	if (take == 0)
	{
		return step;
	}

	const void* srcPtr = src.data();
	void* destPtr = dest.data();
	const void* const prevSrc = srcPtr;
	void* const prevDst = destPtr;

	const RapidYencDecoderEnd end = rapidyenc_decode_incremental(&srcPtr, &destPtr, take, &m_state);

	step.decoded = static_cast<size_t>(static_cast<char*>(destPtr) - static_cast<char*>(prevDst));
	step.consumed = static_cast<size_t>(static_cast<const char*>(srcPtr) - static_cast<const char*>(prevSrc));

	if (end == RYDEC_END_CONTROL)
	{
		// rapidyenc consumed "\r\n=y"; the rest of the line follows in the input.
		step.endControl = true;
	}
	else if (end == RYDEC_END_ARTICLE)
	{
		// rapidyenc consumes the NNTP end sequence itself, so nothing is left behind.
		m_complete = true;
	}

	return step;
}

void YencDecoder::ResolveControlLine()
{
	m_line = Line::Data;

	std::string_view line = m_control.View();
	while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
	{
		line.remove_suffix(1);
	}

	ParseControlLine(line);
	// rapidyenc leaves its state at NONE after END_CONTROL; the next line starts fresh.
	m_state = RYDEC_STATE_CRLF;
	m_control.Clear();
}

DecodeStep YencDecoder::Decode(std::span<const char> src, std::span<char> dest)
{
	DecodeStep step;
	if (m_complete)
	{
		step.complete = true;
		return step;
	}
	size_t destPos = 0;

	for (;;)
	{
		if (m_stage == Stage::Trailer)
		{
			ConsumeEndSequence(src, step.consumed);
			break; // without the end sequence the caller must read more
		}

		const std::span<const char> input = src.subspan(step.consumed);
		if (input.empty())
		{
			break;
		}

		if (m_line == Line::Candidate)
		{
			const LineStart kind = ClassifyLine(std::string_view(input.data() + 1, input.size() - 1));
			if (kind == LineStart::NeedMore)
			{
				break; // the caller keeps 'y' and reads more
			}
			if (kind == LineStart::Control)
			{
				m_control.Assign("=y");
				m_line = Line::Control;
				step.consumed += 1;
				continue;
			}
			// Data: "=y" is the escape for 0x0F; write it and consume the 'y'.
			if (destPos >= dest.size())
			{
				break; // keep 'y' for the next call
			}
			const char escape = '\x0f';
			dest[destPos++] = escape;
			if (m_crcCheck)
			{
				m_meta.calculatedCrc = UpdateCrc(m_meta.calculatedCrc, &escape, 1);
			}
			++m_produced;
			step.consumed += 1;
			m_line = Line::Data;
			continue;
		}

		if (m_line == Line::Control)
		{
			const size_t nl = IndexOf(input, '\n');
			const size_t take = nl == std::string_view::npos ? input.size() : nl + 1;
			const size_t room = MAX_CONTROL_LINE - m_control.Size();
			if (take > room)
			{
				m_headerValid = false;
				m_control.Append(std::string_view(input.data(), room));
			}
			else
			{
				m_control.Append(std::string_view(input.data(), take));
			}
			step.consumed += take;

			if (nl == std::string_view::npos)
			{
				continue; // the rest of the line is in the next chunk
			}

			ResolveControlLine();
			continue;
		}

		const DataStep data = DecodeData(input, dest.subspan(destPos));
		if (data.consumed == 0 && data.decoded == 0 && !data.endControl)
		{
			break;
		}
		step.consumed += data.consumed;

		if (data.decoded > 0)
		{
			if (m_crcCheck)
			{
				m_meta.calculatedCrc = UpdateCrc(m_meta.calculatedCrc, dest.data() + destPos, data.decoded);
			}
			destPos += data.decoded;
			m_produced += static_cast<int64_t>(data.decoded);
		}

		if (data.endControl)
		{
			// rapidyenc consumed "=y" (at least 'y'); keep 'y' so the line can be classified first.
			step.consumed -= 1;
			m_line = Line::Candidate;
			continue;
		}

		if (m_complete)
		{
			break; // the rest of the buffer belongs to the next response
		}

		if (destPos >= dest.size())
		{
			break;
		}
	}

	step.produced = destPos;
	step.complete = m_complete;
	return step;
}

/// Consumes ".\r\n" after =yend; a leftover would be read as the next response.
void YencDecoder::ConsumeEndSequence(std::span<const char> src, size_t& pos) noexcept
{
	while (pos < src.size())
	{
		const char ch = src[pos++];
		switch (m_term)
		{
			case Term::AtLineStart:
				if (ch == '.')
				{
					m_term = Term::AfterDot;
				}
				else if (ch != '\n')
				{
					m_term = Term::MidLine;
				}
				break;
			case Term::AfterDot:
				if (ch == '\r')
				{
					m_term = Term::AfterDotCr;
				}
				else if (ch == '\n')
				{
					m_complete = true;
				}
				else
				{
					m_term = Term::MidLine; // ".." or ".x" is body text, not a terminator
				}
				break;
			case Term::AfterDotCr:
				if (ch == '\n')
				{
					m_complete = true;
				}
				else
				{
					m_term = Term::MidLine;
				}
				break;
			case Term::MidLine: // ignore dots until the next line
				if (ch == '\n')
				{
					m_term = Term::AtLineStart;
				}
				break;
		}
		if (m_complete)
		{
			return;
		}
	}
}

DecodeStatus YencDecoder::Finish()
{
	const int64_t endSize = std::max<int64_t>(0, m_endSize);
	if (m_stage == Stage::Header)
	{
		return DecodeStatus::NoBinaryData;
	}
	if (m_stage != Stage::Trailer || !m_complete)
	{
		return DecodeStatus::ArticleIncomplete;
	}
	if (!m_headerValid || m_part == Part::Declared || (m_part == Part::Ranged && m_meta.size != m_produced))
	{
		return DecodeStatus::InvalidHeader;
	}

	const bool sizeOk = Multipart()
		? (endSize == m_produced || (m_part == Part::Ranged && m_meta.fileSize > 0 && endSize == m_meta.fileSize))
		: (m_meta.fileSize == endSize && endSize == m_produced);
	if (!sizeOk)
	{
		return DecodeStatus::InvalidSize;
	}

	if (m_crcCheck && m_meta.hasCrc && m_meta.crc != m_meta.calculatedCrc)
	{
		return DecodeStatus::CrcError;
	}
	return DecodeStatus::Finished;
}

} // namespace network::nntp
