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

#ifndef JSON_H
#define JSON_H

#include <boost/json.hpp>
#include <iostream>

namespace Json
{
	using namespace boost::json;
	using JsonValue = boost::json::value;
	using JsonObject = boost::json::object;
	using JsonArray = boost::json::array;
	using StreamParser = boost::json::stream_parser;
	using ErrorCode = boost::system::error_code;

	JsonValue Deserialize(std::istream& is, ErrorCode& ec);
	std::string Serialize(const JsonObject& json);
}

#endif
