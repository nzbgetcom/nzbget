/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2023-2024 Denis <denis@nzbget.com>
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

#include "Json.h"

namespace Json
{
	std::optional<JsonValue> Deserialize(std::basic_istream<char>& is) noexcept
	{
		ErrorCode ec;
		StreamParser parser;
		std::string line;
		while (std::getline(is, line))
		{
			parser.write(line, ec);
			if (ec) return std::nullopt;
		}

		parser.finish(ec);

		if (ec) return std::nullopt;

		return parser.release();
	}

	std::optional<JsonValue> Deserialize(const std::string& jsonStr) noexcept
	{
		ErrorCode ec;
		JsonValue value = parse(jsonStr, ec);

		if (ec) return std::nullopt;

		return value;
	}

	std::string Serialize(const JsonObject& json) noexcept
	{
		return serialize(json);
	}
}
