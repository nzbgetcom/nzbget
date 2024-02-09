/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2023 Denis <denis@nzbget.com>
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

namespace Json {
	JsonValue Deserialize(std::istream& is, ErrorCode& ec)
	{
		StreamParser parser;
		std::string line;

		while (std::getline(is, line))
		{
			parser.write(line, ec);
			if (ec)
			{
				return nullptr;
			}
		}

		parser.finish(ec);

		if (ec)
		{
			return nullptr;
		}

		return parser.release();
	}

	std::string Serialize(const JsonObject& json)
	{
		return serialize(json);
	}
}
