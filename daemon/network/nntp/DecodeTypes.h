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

#ifndef NETWORK_NNTP_DECODE_TYPES_H
#define NETWORK_NNTP_DECODE_TYPES_H

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace network::nntp
{

enum class ArticleFormat
{
	Unknown,
	Yenc,
	Ux,
	Raw,
};

enum class DecodeStatus
{
	UnknownError,
	Finished,
	ArticleIncomplete,
	CrcError,
	InvalidSize,
	NoBinaryData,
	InvalidHeader
};

/// What the caller must know about a decoded article before writing it out.
struct ArticleMeta
{
	ArticleFormat format{ArticleFormat::Unknown};
	std::string name;
	int64_t offset{0};
	int64_t size{0};
	int64_t fileSize{0};
	uint32_t crc{0};
	uint32_t calculatedCrc{0};
	bool hasCrc{false};
};

/// One step of streaming decoding.
struct DecodeStep
{
	size_t consumed{0};
	size_t produced{0};
	bool complete{false};
};

/// Fixed-capacity buffer on the object itself, no heap. Only [0, Size()) is read, so storage is not zeroed.
template <size_t Capacity>
class FixedBuffer
{
public:
	size_t Size() const noexcept { return m_size; }
	void Clear() noexcept { m_size = 0; }

	/// Returns false and writes nothing if data does not fit.
	bool Assign(std::string_view data) noexcept
	{
		m_size = 0;
		return Append(data);
	}

	bool Append(std::string_view data) noexcept
	{
		if (data.size() > Capacity - m_size)
		{
			return false;
		}
		if (!data.empty())
		{
			std::memcpy(m_data.data() + m_size, data.data(), data.size());
			m_size += data.size();
		}
		return true;
	}

	std::string_view View() const noexcept { return {m_data.data(), m_size}; }

private:
	std::array<char, Capacity> m_data;
	size_t m_size{0};
};

} // namespace network::nntp

#endif
