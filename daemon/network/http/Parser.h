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
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef NETWORK_HTTP_PARSER_H
#define NETWORK_HTTP_PARSER_H

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Error.h"

namespace network::http
{

struct Response
{
	unsigned status{0};
	std::vector<std::pair<std::string, std::string>> headers;

	/** First header with this name, matched case-insensitively; nullptr when absent. */
	const std::string* Header(std::string_view name) const;
};

/** Receives body chunks as views into the caller's input; copy what must outlive the call. */
class BodySink
{
public:
	virtual ~BodySink() = default;
	virtual Result<void> OnBody(std::string_view data) = 0;
};

struct Limits
{
	size_t maxHeaderSize{size_t{64} * 1024};
	size_t maxLineSize{size_t{8} * 1024};
	size_t maxBodySize{size_t{64} * 1024 * 1024};
};

/**
 * Incremental HTTP/1.x response parser, independent of any transport.
 *
 * Framing comes from the head alone: Content-Length, chunked, no body
 * (1xx/204/304/101), or connection close. Header bytes are copied once into a
 * bounded line buffer; body bytes go to the sink as views and are never copied.
 */
class Parser final
{
public:
	using Limits = http::Limits;

	explicit Parser(const Limits& limits = {});

	/** Consumes `data`. Any protocol error moves the parser into a failed state. */
	Result<void> Feed(std::string_view data, BodySink& sink);

	/** Peer closed the connection: legal only for close-delimited or empty bodies. */
	Result<void> Finish();

	bool Done() const noexcept { return m_state == State::Done; }

	/** Valid once the head has been parsed. */
	Response& Head() noexcept { return m_response; }

	void Reset() noexcept;

private:
	enum class State { Head, Body, ChunkSize, ChunkData, ChunkCrlf, Trailers, CloseDelimited, Done, Failed };
	enum class LineStatus { Complete, NeedMore };

	Result<void> ParseHead();
	Result<void> StartBody();
	Result<LineStatus> TakeLine(std::string_view& data, std::string& line);
	Result<void> Fail(Error error) noexcept;

	Limits m_limits;
	State m_state{State::Head};
	Response m_response;
	std::string m_line; // completed control line: head block, chunk size or trailer
	std::string m_partial; // tail of a control line split across Feed calls
	std::string m_pending; // post-head bytes when a head was split across Feed calls
	size_t m_trailerSize{0};
	size_t m_bodySize{0};
	size_t m_remaining{0};
};

}

#endif // NETWORK_HTTP_PARSER_H
