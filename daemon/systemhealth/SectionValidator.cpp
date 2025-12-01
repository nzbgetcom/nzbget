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

}  // namespace SystemHealth
