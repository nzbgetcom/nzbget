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

#include "nzbget.h"

#include <algorithm>
#include <charconv>
#include <ranges>

#include "Parser.h"
#include "Util.h"

namespace network::http
{
namespace
{

bool ParseNumber(std::string_view text, auto& value, int base = 10)
{
	const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value, base);
	return !text.empty() && ec == std::errc{} && end == text.data() + text.size();
}

/** RFC 7230 tchar: the only characters allowed in a header field name. */
bool IsTokenChar(unsigned char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
		|| c == '!' || c == '#' || c == '$' || c == '%' || c == '&' || c == '\'' || c == '*'
		|| c == '+' || c == '-' || c == '.' || c == '^' || c == '_' || c == '`' || c == '|' || c == '~';
}

/** Field values may hold any visible character plus HTAB; other controls and DEL are rejected. */
bool IsValidFieldValue(std::string_view value)
{
	return std::ranges::all_of(value, [](unsigned char c)
	{
		return c == '\t' || (c >= 0x20 && c != 0x7F);
	});
}

bool IsNoBodyStatus(unsigned status)
{
	return status == 101 || status == 204 || status == 304;
}

} // namespace

const std::string* Response::Header(std::string_view name) const
{
	const auto it = std::ranges::find_if(headers,
		[&](const auto& header) { return Util::StrCaseCmp(header.first, name); });
	return it == headers.end() ? nullptr : &it->second;
}

Parser::Parser(const Limits& limits)
	: m_limits(limits)
{
}

void Parser::Reset() noexcept
{
	m_state = State::Head;
	m_response = Response{};
	m_line.clear();
	m_partial.clear();
	m_pending.clear();
	m_trailerSize = 0;
	m_bodySize = 0;
	m_remaining = 0;
}

Result<void> Parser::Fail(Error error) noexcept
{
	m_state = State::Failed;
	return error;
}

Result<void> Parser::Feed(std::string_view data, BodySink& sink)
{
	if (m_state == State::Failed)
	{
		return Error::ProtocolViolation;
	}

	m_pending.clear();
	std::string_view input = data;
	while (!input.empty())
	{
		switch (m_state)
		{
			case State::Done:
				return Fail(Error::ProtocolViolation);

			case State::Head:
			{
				const auto end = input.find("\r\n\r\n");
				if (end != std::string_view::npos)
				{
					if (m_line.size() + end + 4 > m_limits.maxHeaderSize)
					{
						return Fail(Error::MessageTooLarge);
					}
					m_line.append(input.substr(0, end));
					input = input.substr(end + 4);
				}
				else
				{
					// The terminator may straddle the boundary of this call.
					m_line.append(input);
					input = {};
					if (m_line.size() > m_limits.maxHeaderSize)
					{
						return Fail(Error::MessageTooLarge);
					}
					const auto split = m_line.find("\r\n\r\n");
					if (split == std::string::npos)
					{
						return {};
					}
					auto tail = std::string(m_line.substr(split + 4));
					m_line.resize(split);
					m_pending = std::move(tail);
					input = m_pending;
				}

				// The head block is complete; ParseHead consumes m_line.
				if (auto parsed = ParseHead(); parsed.has_error())
				{
					return parsed;
				}
				if (m_response.status >= 100 && m_response.status < 200 && m_response.status != 101)
				{
					m_response = Response{};
					continue; // interim reply: the real head follows
				}
				if (auto started = StartBody(); started.has_error())
				{
					return started;
				}
				break;
			}

			case State::Body:
			{
				const size_t take = std::min(m_remaining, input.size());
				if (auto sunk = sink.OnBody(input.substr(0, take)); sunk.has_error())
				{
					return sunk;
				}
				input.remove_prefix(take);
				m_remaining -= take;
				if (m_remaining == 0)
				{
					m_state = State::Done;
				}
				break;
			}

			case State::CloseDelimited:
			{
				if (m_bodySize + input.size() > m_limits.maxBodySize)
				{
					return Fail(Error::MessageTooLarge);
				}
				m_bodySize += input.size();
				if (auto sunk = sink.OnBody(input); sunk.has_error())
				{
					return sunk;
				}
				input = {};
				break;
			}

			case State::ChunkSize:
			{
				if (auto line = TakeLine(input, m_line); line.has_error())
				{
					return line.error();
				}
				else if (line.value() == LineStatus::NeedMore)
				{
					break; // need more input
				}

				size_t size;
				const auto number = std::string_view{m_line}.substr(0, m_line.find(';'));
				if (!ParseNumber(number, size, 16))
				{
					return Fail(Error::ProtocolViolation);
				}
				if (size == 0)
				{
					m_state = State::Trailers;
				}
				else
				{
					if (size > m_limits.maxBodySize - m_bodySize)
					{
						return Fail(Error::MessageTooLarge);
					}
					m_remaining = size;
					m_state = State::ChunkData;
				}
				break;
			}

			case State::ChunkData:
			{
				const size_t take = std::min(m_remaining, input.size());
				if (auto sunk = sink.OnBody(input.substr(0, take)); sunk.has_error())
				{
					return sunk;
				}
				input.remove_prefix(take);
				m_remaining -= take;
				m_bodySize += take;
				if (m_remaining == 0)
				{
					m_state = State::ChunkCrlf;
				}
				break;
			}

			case State::ChunkCrlf:
			{
				if (auto line = TakeLine(input, m_line); line.has_error())
				{
					return line.error();
				}
				else if (line.value() == LineStatus::NeedMore)
				{
					break;
				}
				if (!m_line.empty())
				{
					return Fail(Error::ProtocolViolation);
				}
				m_state = State::ChunkSize;
				break;
			}

			case State::Trailers:
			{
				if (auto line = TakeLine(input, m_line); line.has_error())
				{
					return line.error();
				}
				else if (line.value() == LineStatus::NeedMore)
				{
					break;
				}
				if (m_line.empty())
				{
					m_state = State::Done;
					break;
				}
				m_trailerSize += m_line.size() + 2;
				if (m_trailerSize > m_limits.maxHeaderSize)
				{
					return Fail(Error::MessageTooLarge);
				}
				break;
			}

			case State::Failed:
				return Error::ProtocolViolation;
		}
	}

	return {};
}

/** Takes one CRLF-terminated line; the CRLF may be split across calls. */
Result<Parser::LineStatus> Parser::TakeLine(std::string_view& data, std::string& line)
{
	const auto nl = data.find('\n');
	if (nl == std::string_view::npos)
	{
		if (m_partial.size() + data.size() > m_limits.maxLineSize)
		{
			return Error::ProtocolViolation;
		}
		m_partial.append(data);
		data = {};
		return LineStatus::NeedMore;
	}

	std::string taken;
	taken.reserve(m_partial.size() + nl);
	taken.append(m_partial);
	taken.append(data.substr(0, nl));
	data.remove_prefix(nl + 1);
	m_partial.clear();

	if (taken.empty() || taken.back() != '\r')
	{
		return Error::ProtocolViolation;
	}
	taken.pop_back();
	if (taken.size() > m_limits.maxLineSize)
	{
		return Error::ProtocolViolation;
	}
	line = std::move(taken);
	return LineStatus::Complete;
}

Result<void> Parser::ParseHead()
{
	std::string_view head{m_line};

	const auto lineEnd = head.find("\r\n");
	const auto status = head.substr(0, lineEnd);
	if (!status.starts_with("HTTP/1.") || status.size() < 12 || status[8] != ' '
		|| !ParseNumber(status.substr(9, 3), m_response.status))
	{
		return Fail(Error::ProtocolViolation);
	}

	head.remove_prefix(lineEnd == std::string_view::npos ? head.size() : lineEnd + 2);
	while (!head.empty())
	{
		const auto end = head.find("\r\n");
		const auto line = head.substr(0, end);
		const auto colon = line.find(':');
		if (line.size() > m_limits.maxLineSize || colon == std::string_view::npos
			|| line.front() == ' ' || line.front() == '\t')
		{
			return Fail(Error::ProtocolViolation);
		}
		const auto name = line.substr(0, colon);
		const auto value = line.substr(colon + 1);
		if (name.empty() || !std::ranges::all_of(name, [](unsigned char c) { return IsTokenChar(c); })
			|| !IsValidFieldValue(value))
		{
			return Fail(Error::ProtocolViolation);
		}
		m_response.headers.emplace_back(std::string(name), std::string(Util::TrimView(value)));
		head.remove_prefix(end == std::string_view::npos ? head.size() : end + 2);
	}

	m_line.clear();
	return {};
}

Result<void> Parser::StartBody()
{
	const std::string* transferEncoding = m_response.Header("transfer-encoding");
	const std::string* contentLength = m_response.Header("content-length");

	if (transferEncoding && contentLength)
	{
		return Fail(Error::ProtocolViolation);
	}

	const std::string* firstLength = nullptr;
	for (const auto& header : m_response.headers)
	{
		if (!Util::StrCaseCmp(header.first, "content-length"))
		{
			continue;
		}
		if (firstLength && *firstLength != header.second)
		{
			return Fail(Error::ProtocolViolation);
		}
		firstLength = &header.second;
	}

	if (IsNoBodyStatus(m_response.status))
	{
		m_state = State::Done;
		return {};
	}

	if (transferEncoding)
	{
		if (!Util::StrCaseCmp(Util::TrimView(*transferEncoding), "chunked"))
		{
			return Fail(Error::ProtocolViolation);
		}
		m_state = State::ChunkSize;
		return {};
	}

	if (contentLength)
	{
		size_t length;
		if (!ParseNumber(*contentLength, length))
		{
			return Fail(Error::ProtocolViolation);
		}
		if (length > m_limits.maxBodySize)
		{
			return Fail(Error::MessageTooLarge);
		}
		m_remaining = length;
		m_state = length == 0 ? State::Done : State::Body;
		return {};
	}

	m_state = State::CloseDelimited;
	return {};
}

Result<void> Parser::Finish()
{
	switch (m_state)
	{
		case State::Done:
			return {};
		case State::CloseDelimited:
			m_state = State::Done;
			return {};
		default:
			return Fail(Error::ConnectionLost);
	}
}

} // namespace network::http
