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

#include "Status.h"

namespace SystemHealth
{
namespace
{
const xmlChar* XmlLiteral(const char* literal) { return reinterpret_cast<const xmlChar*>(literal); }
}  // namespace

Json::JsonObject ToJson(const Status& status)
{
	Json::JsonObject json;

	json["Severity"] = SeverityToStr(status.GetSeverity());
	json["Message"] = status.GetMessage();

	return json;
}

Xml::XmlNodePtr ToXml(const Status& status)
{
	std::string_view severity = SeverityToStr(status.GetSeverity());
	const std::string& message = status.GetMessage();

	Xml::XmlNodePtr node = xmlNewNode(nullptr, XmlLiteral("struct"));
	Xml::AddNewNode(node, "Severity", "string", severity.data());
	Xml::AddNewNode(node, "Message", "string", message.c_str());

	return node;
}

std::string_view SeverityToStr(Status::Severity severity)
{
	switch (severity)
	{
		case Status::Severity::Ok:
			return "Ok";
		case Status::Severity::Info:
			return "Info";
		case Status::Severity::Warning:
			return "Warning";
		case Status::Severity::Error:
			return "Error";
	}
	return "Ok";
}
}  // namespace SystemHealth
