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
		auto status = validator->Validate();
		if (!status.IsOk())
			report.options.push_back({std::string(validator->GetName()), std::move(status)});
	}

	return report;
}

Json::JsonObject ToJson(const OptionStatus& status)
{
	Json::JsonObject json;
	json["Name"] = status.name;
	json["Severity"] = SeverityToStr(status.status.GetSeverity());
	json["Message"] = status.status.GetMessage();
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
	Json::JsonArray issuesArrayJson;
	Json::JsonArray optionsArrayJson;
	Json::JsonArray sectionsArrayJson;

	for (const auto& issue : report.issues)
	{
		issuesArrayJson.push_back(ToJson(issue));
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
	json["Issues"] = std::move(issuesArrayJson);
	json["Options"] = std::move(optionsArrayJson);
	json["Subsections"] = std::move(sectionsArrayJson);

	return json;
}

// <Option>
//   <Name>...</Name>
//   <Severity>...</Severity>
//   <Message>...</Message>
// </Option>
Xml::XmlNodePtr ToXml(const OptionStatus& status)
{
	xmlNodePtr structNode = Xml::CreateStructNode();

	const auto severity = SeverityToStr(status.status.GetSeverity());
	Xml::AddNewNode(structNode, "Name", "string", status.name.c_str());
	Xml::AddNewNode(structNode, "Severity", "string", severity.data());
	Xml::AddNewNode(structNode, "Message", "string", status.status.GetMessage().c_str());

	return structNode->parent;
}

// <Subsection>
//   <Name>...</Name>
//   <Options>...</Options>
// </Subsection>
Xml::XmlNodePtr ToXml(const SubsectionReport& report)
{
	xmlNodePtr structNode = Xml::CreateStructNode();

	Xml::AddNewNode(structNode, "Name", "string", report.name.c_str());

	std::vector<xmlNodePtr> optionsNodes;
	for (const auto& option : report.options)
	{
		optionsNodes.push_back(ToXml(option));
	}
	Xml::AddArrayNode(structNode, "Options", optionsNodes);

	return structNode->parent;
}

// <Section>
//   <Name>...</Name>
//   <Issues>...</Issues>
//   <Options>...</Options>
//   <Subsections>...</Subsections>
// </Section>
Xml::XmlNodePtr ToXml(const SectionReport& report)
{
	xmlNodePtr structNode = Xml::CreateStructNode();

	Xml::AddNewNode(structNode, "Name", "string", report.name.c_str());

	std::vector<xmlNodePtr> issueNodes;
	for (const auto& issue : report.issues)
	{
		xmlNodePtr sNode = Xml::CreateStructNode();
		const auto severity = SeverityToStr(issue.GetSeverity());
		Xml::AddNewNode(sNode, "Severity", "string", severity.data());
		Xml::AddNewNode(sNode, "Message", "string", issue.GetMessage().c_str());
		issueNodes.push_back(sNode->parent);
	}
	Xml::AddArrayNode(structNode, "Issues", issueNodes);

	std::vector<xmlNodePtr> optionNodes;
	for (const auto& option : report.options)
	{
		optionNodes.push_back(ToXml(option));
	}
	Xml::AddArrayNode(structNode, "Options", optionNodes);

	std::vector<xmlNodePtr> subNodes;
	for (const auto& sub : report.subsections)
	{
		subNodes.push_back(ToXml(sub));
	}
	Xml::AddArrayNode(structNode, "Subsections", subNodes);

	return structNode->parent;
}

}  // namespace SystemHealth
