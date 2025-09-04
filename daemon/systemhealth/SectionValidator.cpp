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

#include "SectionValidator.h"
#include <optional>
#include "Json.h"
#include "Xml.h"

namespace SystemHealth
{

SectionValidator::~SectionValidator() = default;

SectionReport SectionValidator::Validate() const
{
	SectionReport report;
	report.name = GetName();
	report.options.reserve(m_validators.size());

	for (const auto& validator : m_validators)
	{
		report.options.push_back({std::string(validator->GetName()), validator->Validate()});
	}

	return report;
}

Json::JsonObject ToJson(const OptionStatus& status)
{
	Json::JsonObject json;
	json["Name"] = status.name;
	json["Status"] = ToJson(status.status);
	return json;
}

Json::JsonObject ToJson(const Alert& alert)
{
	Json::JsonObject json;
	json["Name"] = alert.name;
	json["Status"] = ToJson(alert.status);
	return json;
}

Json::JsonObject ToJson(const SubsectionReport& report)
{
	Json::JsonObject json;
	Json::JsonArray optionsArrayJson;

	for (const auto& option : report.options)
	{
		optionsArrayJson.push_back(ToJson(option));
	}

	json["Name"] = report.name;
	json["Options"] = std::move(optionsArrayJson);

	return json;
}

Json::JsonObject ToJson(const SectionReport& report)
{
	Json::JsonObject json;
	Json::JsonObject reportJson;
	Json::JsonArray alertsArrayJson;
	Json::JsonArray optionsArrayJson;
	Json::JsonArray sectionsArrayJson;

	for (const auto& check : report.alerts)
	{
		alertsArrayJson.push_back(ToJson(check));
	}

	for (const auto& option : report.options)
	{
		optionsArrayJson.push_back(ToJson(option));
	}

	for (const auto& section : report.subsections)
	{
		sectionsArrayJson.push_back(ToJson(section));
	}

	json["Name"] = report.name;
	json["Alerts"] = std::move(alertsArrayJson);
	json["Options"] = std::move(optionsArrayJson);
	json["Subsections"] = std::move(sectionsArrayJson);

	return json;
}

namespace
{
xmlChar const* XmlLiteral(const char* literal) { return reinterpret_cast<const xmlChar*>(literal); }

xmlNodePtr CreateArrayNode(const char* name, const std::vector<Xml::XmlNodePtr>& nodes)
{
	xmlNodePtr memberNode = xmlNewNode(nullptr, XmlLiteral("member"));
	xmlNewChild(memberNode, nullptr, XmlLiteral("name"), XmlLiteral(name));

	xmlNodePtr valueNode = xmlNewNode(nullptr, XmlLiteral("value"));
	xmlNodePtr arrayNode = xmlNewNode(nullptr, XmlLiteral("array"));
	xmlNodePtr dataNode = xmlNewNode(nullptr, XmlLiteral("data"));

	for (Xml::XmlNodePtr node : nodes)
	{
		xmlNodePtr valueEntry = xmlNewNode(nullptr, XmlLiteral("value"));
		xmlAddChild(valueEntry, node);
		xmlAddChild(dataNode, valueEntry);
	}

	xmlAddChild(arrayNode, dataNode);
	xmlAddChild(valueNode, arrayNode);
	xmlAddChild(memberNode, valueNode);

	return memberNode;
}
}  // namespace

Xml::XmlNodePtr ToXml(const OptionStatus& status)
{
	Xml::XmlNodePtr node = xmlNewNode(nullptr, XmlLiteral("struct"));
	Xml::AddNewNode(node, "Name", "string", status.name.c_str());

	xmlNodePtr statusMember = xmlNewNode(nullptr, XmlLiteral("member"));
	xmlNewChild(statusMember, nullptr, XmlLiteral("name"), XmlLiteral("Status"));
	xmlNodePtr statusValue = xmlNewNode(nullptr, XmlLiteral("value"));
	xmlAddChild(statusValue, ToXml(status.status));
	xmlAddChild(statusMember, statusValue);
	xmlAddChild(node, statusMember);

	return node;
}

Xml::XmlNodePtr ToXml(const Alert& alert)
{
	Xml::XmlNodePtr node = xmlNewNode(nullptr, XmlLiteral("struct"));
	Xml::AddNewNode(node, "Name", "string", alert.name.c_str());

	xmlNodePtr statusMember = xmlNewNode(nullptr, XmlLiteral("member"));
	xmlNewChild(statusMember, nullptr, XmlLiteral("name"), XmlLiteral("Status"));
	xmlNodePtr statusValue = xmlNewNode(nullptr, XmlLiteral("value"));
	xmlAddChild(statusValue, ToXml(alert.status));
	xmlAddChild(statusMember, statusValue);
	xmlAddChild(node, statusMember);

	return node;
}

Xml::XmlNodePtr ToXml(const SubsectionReport& report)
{
	Xml::XmlNodePtr node = xmlNewNode(nullptr, XmlLiteral("struct"));
	Xml::AddNewNode(node, "Name", "string", report.name.c_str());

	std::vector<Xml::XmlNodePtr> optionNodes;
	optionNodes.reserve(report.options.size());
	for (const auto& option : report.options)
	{
		optionNodes.push_back(ToXml(option));
	}

	xmlAddChild(node, CreateArrayNode("Options", optionNodes));

	return node;
}

Xml::XmlNodePtr ToXml(const SectionReport& report)
{
	Xml::XmlNodePtr node = xmlNewNode(nullptr, XmlLiteral("struct"));
	Xml::AddNewNode(node, "Name", "string", report.name.c_str());

	std::vector<Xml::XmlNodePtr> alertNodes;
	alertNodes.reserve(report.alerts.size());
	for (const auto& alert : report.alerts)
	{
		alertNodes.push_back(ToXml(alert));
	}
	xmlAddChild(node, CreateArrayNode("Alerts", alertNodes));

	std::vector<Xml::XmlNodePtr> optionNodes;
	optionNodes.reserve(report.options.size());
	for (const auto& option : report.options)
	{
		optionNodes.push_back(ToXml(option));
	}
	xmlAddChild(node, CreateArrayNode("Options", optionNodes));

	std::vector<Xml::XmlNodePtr> subsectionNodes;
	subsectionNodes.reserve(report.subsections.size());
	for (const auto& subsection : report.subsections)
	{
		subsectionNodes.push_back(ToXml(subsection));
	}
	xmlAddChild(node, CreateArrayNode("Subsections", subsectionNodes));

	return node;
}
}  // namespace SystemHealth
