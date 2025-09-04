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

#include "SectionGroupValidator.h"
#include <optional>

namespace SystemHealth
{
SectionGroupValidator::SectionGroupValidator(
	std::vector<std::unique_ptr<SectionValidator>> sections)
	: m_sections(std::move(sections))
{
}

SectionReport SectionGroupValidator::Validate() const
{
	SectionReport report;
	report.name = GetName();
	report.alerts.reserve(m_validators.size());

	for (const auto& section : m_sections)
	{
		auto sectionReport = section->Validate();
		report.subsections.push_back(
			{std::move(sectionReport.name), std::move(sectionReport.options)});
	}

	for (const auto& validator : m_validators)
	{
		Status status = validator->Validate();
		if (!status.IsOk())
		{
			report.alerts.push_back({std::string(validator->GetName()), std::move(status)});
		}
	}

	return report;
}

}  // namespace SystemHealth
