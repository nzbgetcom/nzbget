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
#include <cstdlib>
#include <cstring>

namespace
{

/// rapidyenc_crc reads 64-bit words, so only a start aligned to them is safe; the unaligned head goes through a copy.
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

/// Matches "name=" only as a whole token: "crc32=" must not match inside "pcrc32=".
const char* FindParam(std::string_view line, std::string_view name) noexcept
{
	for (auto pos = line.find(name); pos != std::string_view::npos; pos = line.find(name, pos + 1))
	{
		if (pos == 0 || line[pos - 1] == ' ')
		{
			return line.data() + pos + name.size();
		}
	}
	return nullptr;
}

bool IsControlLine(std::string_view line) noexcept
{
	for (const std::string_view keyword : {"=ybegin", "=ypart", "=yend"})
	{
		if (line.starts_with(keyword))
		{
			return line.size() == keyword.size() || line[keyword.size()] == ' ' ||
				line[keyword.size()] == '\r' || line[keyword.size()] == '\n';
		}
	}
	return false;
}

size_t IndexOf(std::span<const char> in, char c) noexcept
{
	const auto* pos = static_cast<const char*>(std::memchr(in.data(), c, in.size()));
	return pos == nullptr ? std::string_view::npos : static_cast<size_t>(pos - in.data());
}

} // namespace

namespace network::nntp
{

YencDecoder::YencDecoder(bool crcCheck) noexcept : m_crcCheck(crcCheck)
{
	m_meta.format = ArticleFormat::Yenc;
}

bool YencDecoder::ParseControlLine(std::string_view line)
{
	if (!IsControlLine(line))
	{
		return false;
	}

	if (line.starts_with("=ybegin"))
	{
		if (m_seenBegin)
		{
			m_headerValid = false;
			return true;
		}
		m_seenBegin = true;
		m_beginPart = FindParam(line, "part=") != nullptr;
		if (const char* size = FindParam(line, "size="))
		{
			m_meta.fileSize = std::strtoll(size, nullptr, 10);
		}
		if (const char* name = FindParam(line, "name="))
		{
			std::string_view value(name, static_cast<size_t>(line.data() + line.size() - name));
			// Some encoders put "=ypart" on the =ybegin line, right after the name.
			if (const size_t ypart = value.find("=ypart "); ypart != std::string_view::npos)
			{
				ParseControlLine(value.substr(ypart));
				value = value.substr(0, ypart);
			}
			m_meta.name = WebUtil::Latin1ToUtf8(std::string(value).c_str());
		}
		if (!m_beginPart && !m_hasPart)
		{
			m_meta.offset = 0;
			m_meta.size = m_meta.fileSize;
		}
		return true;
	}

	if (line.starts_with("=ypart"))
	{
		const char* begin = FindParam(line, "begin=");
		const char* end = FindParam(line, "end=");
		const int64_t first = begin ? std::strtoll(begin, nullptr, 10) : 0;
		// Without end= the part length is unknown, so a cut-off article would look complete.
		const int64_t last = end ? std::strtoll(end, nullptr, 10) : 0;
		// Data already decoded cannot be moved, so a part line after it or a second one is refused.
		if (first < 1 || last < first || m_hasPart || m_produced > 0)
		{
			m_headerValid = false;
			return true;
		}

		// The =ypart range is the truth even when =ybegin size= carries the part size.
		m_hasPart = true;
		m_meta.offset = first - 1;
		m_meta.size = last - first + 1;
		return true;
	}

	if (line.starts_with("=yend"))
	{
		// The end sequence after this line is left to the caller.
		m_complete = true;
		m_seenEnd = true;
		if (const char* size = FindParam(line, "size="))
		{
			m_endSize = std::strtoll(size, nullptr, 10);
		}
		// On a part crc32= is the whole-file CRC, so only pcrc32= applies there.
		const char* crc = Multipart() ? FindParam(line, "pcrc32=") : FindParam(line, "crc32=");
		if (crc != nullptr)
		{
			m_meta.crc = static_cast<uint32_t>(std::strtoul(crc, nullptr, 16));
			m_meta.hasCrc = true;
		}
		return true;
	}

	return false;
}

YencDecoder::DataStep YencDecoder::DecodeData(std::span<const char> src, std::span<char> dest) noexcept
{
	DataStep step;

	if (m_pendingByte)
	{
		if (dest.empty())
		{
			return step;
		}
		dest[0] = static_cast<char>(0x0f);
		step.decoded = 1;
		m_pendingByte = false;
	}

	if (step.consumed >= src.size() || step.decoded >= dest.size())
	{
		return step;
	}

	// rapidyenc has no output length parameter: it writes at most one byte per
	// input byte, so the input slice must not exceed the free output space.
	// It decodes in-situ, so dest may be the very buffer src comes from.
	const size_t take = std::min(src.size() - step.consumed, dest.size() - step.decoded);

	const void* srcPtr = src.data() + step.consumed;
	void* destPtr = dest.data() + step.decoded;
	const void* const prevSrc = srcPtr;
	void* const prevDst = destPtr;

	const RapidYencDecoderEnd end = rapidyenc_decode_incremental(&srcPtr, &destPtr, take, &m_state);

	step.decoded += static_cast<size_t>(static_cast<char*>(destPtr) - static_cast<char*>(prevDst));
	step.consumed += static_cast<size_t>(static_cast<const char*>(srcPtr) - static_cast<const char*>(prevSrc));

	if (end == RYDEC_END_CONTROL)
	{
		// rapidyenc consumed "\r\n=y": rewind to '=' so the control line is read from the caller's
		// input; a marker split across chunks leaves the rest of "=y" in m_marker.
		const size_t present = std::min<size_t>(2, step.consumed);
		step.consumed -= present;
		m_marker.Assign(std::string_view("=y", 2 - present));
		step.endControl = true;
		return step;
	}

	if (end == RYDEC_END_ARTICLE)
	{
		// rapidyenc consumes the NNTP end sequence itself, so nothing is left behind.
		m_terminated = true;
		m_complete = true;
	}

	return step;
}

void YencDecoder::StartControlLineFromFront()
{
	m_control.Assign(m_marker.View());
	m_control.Append(m_front.View().substr(m_frontPos));
	m_front.Clear();
	m_frontPos = 0;
	m_controlPending = true;
}

void YencDecoder::ResolveControlLine()
{
	m_controlPending = false;

	std::string_view line = m_control.View();
	if (!line.empty() && line.back() == '\n')
	{
		line.remove_suffix(1);
	}
	if (!line.empty() && line.back() == '\r')
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
	size_t destPos = 0;

	for (;;)
	{
		if (m_afterEnd)
		{
			size_t pos = step.consumed;
			const bool done = ConsumeEndSequence(src, pos);
			step.consumed = pos;
			if (done)
			{
				m_complete = true;
			}
			break; // without the end sequence the caller must read more
		}

		const bool replay = m_frontPos < m_front.Size();
		const std::span<const char> input = replay
			? std::span<const char>(m_front.View()).subspan(m_frontPos)
			: src.subspan(step.consumed);

		if (input.empty())
		{
			break;
		}

		if (m_controlPending)
		{
			const size_t nl = IndexOf(input, '\n');
			size_t take = nl == std::string_view::npos ? input.size() : nl + 1;
			if (m_control.Size() < CONTROL_WINDOW)
			{
				take = std::min(take, CONTROL_WINDOW - m_control.Size());
			}
			if (m_control.Size() + take <= MAX_CONTROL_LINE)
			{
				m_control.Append(std::string_view(input.data(), take));
			}
			else
			{
				m_headerValid = false;
			}
			if (replay)
			{
				m_frontPos += take;
			}
			else
			{
				step.consumed += take;
			}

			const bool lineEnd = input[take - 1] == '\n';
			if (!lineEnd && m_control.Size() < CONTROL_WINDOW)
			{
				continue; // the keyword is not complete yet
			}

			if (!IsControlLine(m_control.View()))
			{
				// Data that merely starts with "=y": rapidyenc already consumed "\r\n";
				// '=' and 'y' are skipped and 'y' decodes to 0x0F.
				m_controlPending = false;
				m_front.Assign(m_control.View());
				m_frontPos = 2;
				m_pendingByte = true;
				m_control.Clear();
				continue;
			}

			if (!lineEnd)
			{
				continue; // control line continues in the next chunk
			}

			ResolveControlLine();
			if (m_complete)
			{
				m_afterEnd = true;
			}
			continue;
		}

		const DataStep data = DecodeData(input, dest.subspan(destPos));
		if (replay)
		{
			m_frontPos += data.consumed;
		}
		else
		{
			step.consumed += data.consumed;
		}

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
			if (replay)
			{
				StartControlLineFromFront();
			}
			else
			{
				m_control.Assign(m_marker.View());
				m_controlPending = true;
			}
			continue;
		}

		if (destPos >= dest.size())
		{
			break;
		}

		if (replay && m_frontPos >= m_front.Size())
		{
			m_front.Clear();
			m_frontPos = 0;
		}
	}

	step.produced = destPos;
	step.complete = m_terminated;
	return step;
}

/**
 * Consumes the "." line which terminates a multiline response. It must be eaten
 * before the decoder reports completion: the connection goes back to the pool
 * right after, and a leftover "\r\n.\r\n" would be read as the next response.
 */
bool YencDecoder::ConsumeEndSequence(std::span<const char> src, size_t& pos) noexcept
{
	while (pos < src.size())
	{
		const char ch = src[pos++];
		switch (m_termState)
		{
			case 0:
				if (ch == '.') m_termState = 1;
				break;
			case 1:
				if (ch == '\r') m_termState = 2;
				else if (ch == '\n') m_terminated = true;
				else if (ch != '.') m_termState = 0;
				break;
			case 2:
				if (ch == '\n') m_terminated = true;
				else m_termState = (ch == '.') ? 1 : 0;
				break;
			default:
				break;
		}
		if (m_terminated)
		{
			return true;
		}
	}
	return false;
}

DecodeStatus YencDecoder::Finish()
{
	const int64_t endSize = std::max<int64_t>(0, m_endSize);
	if (!m_seenBegin)
	{
		return DecodeStatus::NoBinaryData;
	}
	if (!m_seenEnd)
	{
		return DecodeStatus::ArticleIncomplete;
	}
	if (!m_headerValid || (Multipart() && !m_hasPart) || (m_hasPart && m_meta.size != m_produced))
	{
		return DecodeStatus::InvalidHeader;
	}

	const bool sizeOk = Multipart()
		? (endSize == m_produced || (m_hasPart && m_meta.fileSize > 0 && endSize == m_meta.fileSize))
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
