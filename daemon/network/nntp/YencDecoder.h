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

#ifndef NETWORK_NNTP_YENC_DECODER_H
#define NETWORK_NNTP_YENC_DECODER_H

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <rapidyenc.h>

#include "DecodeTypes.h"

namespace network::nntp
{

/**
 * Streaming yEnc decoder. Takes the article body as it comes from the connection,
 * dot-stuffing and end sequence included, and decodes in-situ into dest. Control
 * lines (=ybegin/=ypart/=yend) are parsed here, also when split across chunks.
 * The host must call rapidyenc_decode_init() and rapidyenc_crc_init() before the first decoder.
 */
class YencDecoder final
{
public:
	/// rapidyenc writes at most one byte per input byte, so one free dest byte always makes progress.
	static constexpr size_t MinBufferSize = 1;

	explicit YencDecoder(bool crcCheck) noexcept;

	DecodeStep Decode(std::span<const char> src, std::span<char> dest);
	DecodeStatus Finish();
	const ArticleMeta& Meta() const noexcept { return m_meta; }

	/// False when a header line contradicts another or the declared sizes; see ParseControlLine().
	bool HeaderValid() const noexcept { return m_headerValid; }
	bool IsComplete() const noexcept { return m_complete; }
	int64_t Produced() const noexcept { return m_produced; }
	int64_t EndSize() const noexcept { return m_endSize; }

private:
	struct DataStep
	{
		size_t consumed{0};
		size_t decoded{0};
		bool endControl{false};
	};

	/// A longer control line is refused, never decoded as data.
	static constexpr size_t MAX_CONTROL_LINE = 8192;

	enum class Stage
	{
		Header,  // before =ybegin
		Body,    // between =ybegin and =yend
		Trailer  // =yend seen, waiting for the NNTP terminator
	};

	enum class Line
	{
		Data,      // plain body data
		Candidate, // rapidyenc saw "\r\n=y"; the line is not classified yet
		Control    // a control line is being collected into m_control
	};

	enum class Term
	{
		AtLineStart,
		AfterDot,
		AfterDotCr,
		MidLine
	};

	/// How =ybegin part= and =ypart begin=/end= declare the article's range.
	enum class Part
	{
		None,     // single-file article
		Declared, // =ybegin part= seen, =ypart not yet
		Ranged    // =ypart seen
	};

	bool Multipart() const noexcept { return m_part != Part::None; }
	/// Sets m_headerValid = false on a malformed or contradicting line.
	void ParseControlLine(std::string_view line);
	DataStep DecodeData(std::span<const char> src, std::span<char> dest) noexcept;
	void ResolveControlLine();
	/// Consumes the ".\r\n" line which follows =yend, so the connection stays in sync.
	void ConsumeEndSequence(std::span<const char> src, size_t& pos) noexcept;

	bool m_crcCheck;
	bool m_headerValid{true};
	Part m_part{Part::None};
	bool m_complete{false}; // the NNTP "." terminator has been consumed
	Stage m_stage{Stage::Header};
	Line m_line{Line::Data};
	Term m_term{Term::AtLineStart};
	int64_t m_produced{0};
	int64_t m_endSize{-1};
	ArticleMeta m_meta;

	RapidYencDecoderState m_state{RYDEC_STATE_CRLF};

	FixedBuffer<MAX_CONTROL_LINE> m_control;
};

} // namespace network::nntp

#endif
