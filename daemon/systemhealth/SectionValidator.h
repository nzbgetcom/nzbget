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

#ifndef SECTION_VALIDATOR_H
#define SECTION_VALIDATOR_H

#include <string>
#include <vector>
#include <optional>
#include <boost/filesystem.hpp>
#include "Validators.h"
#include "Status.h"

namespace SystemHealth
{
struct OptionStatus
{
	std::string name;
	Status status;
};

struct SubsectionReport
{
	std::string name;
	std::vector<OptionStatus> options;
};

struct SectionReport
{
	std::string name;
	std::vector<Status> issues;
	std::vector<OptionStatus> options;
	std::vector<SubsectionReport> subsections;
};

class SectionValidator
{
public:
	virtual ~SectionValidator();
	virtual std::string_view GetName() const = 0;
	virtual SectionReport Validate() const;

protected:
	std::vector<std::unique_ptr<Validator>> m_validators;
};

Json::JsonObject ToJson(const OptionStatus& status);
Json::JsonObject ToJson(const SubsectionReport& report);
Json::JsonObject ToJson(const SectionReport& report);

Xml::XmlNodePtr ToXml(const OptionStatus& status);
Xml::XmlNodePtr ToXml(const SubsectionReport& report);
Xml::XmlNodePtr ToXml(const SectionReport& report);

}  // namespace SystemHealth

#endif
