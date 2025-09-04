/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2025 Denis <denis@nzbget.com>
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

#include <iostream>
#include "Check.h"

namespace HealthCheck
{
	Json::JsonObject ToJson(const Check& check)
	{
		Json::JsonObject json;

		json["Status"] = StatusToStr(check.GetStatus());
		json["Message"] = check.GetMessage();

		return json;
	}

	Xml::XmlNodePtr ToXml(const Check& check)
	{
		std::string_view status = StatusToStr(check.GetStatus());
		const std::string& message = check.GetMessage();

		Xml::XmlNodePtr node = xmlNewNode(nullptr, BAD_CAST "struct");
		Xml::AddNewNode(node, "Status", "string", status.data());
		Xml::AddNewNode(node, "Message", "string", message.c_str());

		return node;
	}

	std::string_view StatusToStr(Status status)
	{
		switch (status)
		{
		case Status::Ok: return "Ok";
		case Status::Info: return "Info";
		case Status::Warning: return "Warning";
		case Status::Error: return "Error";
		}
	}
}
