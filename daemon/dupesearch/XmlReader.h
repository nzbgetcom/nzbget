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


#ifndef XMLREADER_H
#define XMLREADER_H

#include <string>
#include <utility>
#include <vector>

/*
 * Reads XML that comes from outside (an indexer's search result, an nzb-file)
 * through libxml2's SAX interface. Only the predefined entities are known and
 * a document that declares entities is rejected, so entity expansion
 * ("billion laughs") is impossible; no external DTD is loaded. Element names
 * reach the handler without their namespace prefix.
 */
class XmlReader
{
public:
	using Attrs = std::vector<std::pair<std::string, std::string>>;

	class Handler
	{
	public:
		virtual ~Handler() = default;
		virtual void Start(const std::string& name, const Attrs& attrs) { (void)name; (void)attrs; }
		virtual void End(const std::string& name) { (void)name; }
		virtual void Text(const std::string& text) { (void)text; }
	};

	/* false for a malformed document or one that declares entities */
	static bool Parse(const std::string& data, Handler& handler);

	static std::string Attr(const Attrs& attrs, const char* name);
};

#endif
