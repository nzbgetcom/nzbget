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

#include "Xml.h"

namespace Xml {
	std::string Serialize(const xmlNodePtr rootNode)
	{
		std::string result;

		xmlBufferPtr buffer = xmlBufferCreate();
		if (buffer == nullptr) {
			return result;
		}

		int size = xmlNodeDump(buffer, rootNode->doc, rootNode, 0, 0);
		if (size > 0) {
			result = std::string(reinterpret_cast<const char*>(buffer->content), size);
		}

		xmlBufferFree(buffer);
		return result;
	}

	void AddNewNode(xmlNodePtr rootNode, const char* name, const char* type, const char* value)
	{
		xmlNodePtr memberNode = xmlNewNode(nullptr, BAD_CAST "member");
		xmlNodePtr valueNode = xmlNewNode(nullptr, BAD_CAST "value");
		xmlNewChild(memberNode, nullptr, BAD_CAST "name", BAD_CAST name);
		xmlNewChild(valueNode, nullptr, BAD_CAST type, BAD_CAST value);
		xmlAddChild(memberNode, valueNode);
		xmlAddChild(rootNode, memberNode);
	}

	const char* BoolToStr(bool value) noexcept
	{
		return value ? "1" : "0";
	}
}
