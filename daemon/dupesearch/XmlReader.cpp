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

#include <libxml/parser.h>
#include "XmlReader.h"

namespace
{

struct Context
{
	XmlReader::Handler* handler;
	bool entityDeclared = false;
};

std::string LocalName(const xmlChar* name)
{
	std::string text = (const char*)name;
	size_t colon = text.rfind(':');
	return colon == std::string::npos ? text : text.substr(colon + 1);
}

void StartElement(void* ctx, const xmlChar* name, const xmlChar** atts)
{
	XmlReader::Attrs attrs;
	for (int i = 0; atts && atts[i]; i += 2)
	{
		attrs.emplace_back(LocalName(atts[i]), atts[i + 1] ? (const char*)atts[i + 1] : "");
	}
	((Context*)ctx)->handler->Start(LocalName(name), attrs);
}

void EndElement(void* ctx, const xmlChar* name)
{
	((Context*)ctx)->handler->End(LocalName(name));
}

void Characters(void* ctx, const xmlChar* text, int len)
{
	if (text && len > 0)
	{
		((Context*)ctx)->handler->Text(std::string((const char*)text, len));
	}
}

xmlEntityPtr GetEntity(void*, const xmlChar* name)
{
	return xmlGetPredefinedEntity(name);
}

void EntityDecl(void* ctx, const xmlChar*, int, const xmlChar*, const xmlChar*, xmlChar*)
{
	((Context*)ctx)->entityDeclared = true;
}

void Ignore(void*, const char*, ...)
{
}

}

bool XmlReader::Parse(const std::string& data, Handler& handler)
{
	if (data.empty())
	{
		return false;
	}

	xmlSAXHandler sax{};
	sax.startElement = StartElement;
	sax.endElement = EndElement;
	sax.getEntity = GetEntity;
	sax.entityDecl = EntityDecl;
	sax.error = Ignore;
	sax.warning = Ignore;
	sax.fatalError = Ignore;
	// libxml2 2.14+ sends CDATA through cdataBlock, older versions through characters
	sax.characters = Characters;
	sax.cdataBlock = Characters;

	Context context;
	context.handler = &handler;
	int result = xmlSAXUserParseMemory(&sax, &context, data.data(), (int)data.size());
	return result == 0 && !context.entityDeclared;
}

std::string XmlReader::Attr(const Attrs& attrs, const char* name)
{
	for (const auto& attr : attrs)
	{
		if (attr.first == name)
		{
			return attr.second;
		}
	}
	return "";
}
