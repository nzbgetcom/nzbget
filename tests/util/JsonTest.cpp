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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"

#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>
#include "Json.h"

BOOST_AUTO_TEST_CASE(JsonDeserializeTest)
{
	{
		std::string validJSON = "{\"name\": \"John\", \"secondName\": \"Doe\"}";
		std::stringstream is;
		is << validJSON;

		{
			auto res = Json::Deserialize(is);
			BOOST_CHECK(res.has_value());
		}

		{
			auto res = Json::Deserialize(validJSON);
			BOOST_CHECK(res.has_value());
		}

		std::string invalidJSON = "{\"name\": \"John\", \"secondName\":}";

		is.clear();
		is << invalidJSON;

		{
			auto res = Json::Deserialize(is);
			BOOST_CHECK(!res.has_value());
		}

		{
			auto res = Json::Deserialize(invalidJSON);
			BOOST_CHECK(!res.has_value());
		}
	}
}
