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

#ifndef SECTION_GROUP_VALIDATOR_H
#define SECTION_GROUP_VALIDATOR_H

#include "SectionValidator.h"

namespace SystemHealth
{

class SectionGroupValidator : public SectionValidator
{
public:
	explicit SectionGroupValidator(std::vector<std::unique_ptr<SectionValidator>> sections);
	SectionReport Validate() const override;

protected:
	std::vector<std::unique_ptr<SectionValidator>> m_sections;
};

}  // namespace SystemHealth

#endif
