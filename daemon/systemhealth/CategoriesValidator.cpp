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

#include <memory>
#include <string>
#include <sstream>
#include "CategoriesValidator.h"
#include "CategoryValidator.h"
#include "Options.h"

namespace SystemHealth::Category
{
CategoriesValidator::CategoriesValidator(const Options& options,
										 const Options::Categories& categories)
	: SectionGroupValidator(MakeCategoryValidators(options, categories)),
	  m_options(options),
	  m_categories(categories)
{
	m_validators.push_back(std::make_unique<DuplicateNamesValidator>(categories));
}

std::vector<std::unique_ptr<SectionValidator>> CategoriesValidator::MakeCategoryValidators(
	const Options& options, const Options::Categories& categories) const
{
	std::vector<std::unique_ptr<SectionValidator>> validators;
	validators.reserve(categories.size());
	for (size_t i = 0; i < categories.size(); ++i)
	{
		validators.push_back(std::make_unique<CategoryValidator>(options, categories[i], i + 1));
	}
	return validators;
}

Status DuplicateNamesValidator::Validate() const
{
	if (m_categories.empty()) return Status::Ok();

	std::set<std::string_view> names;
	for (const auto& cat : m_categories)
	{
		std::string_view name = cat.GetName();
		if (name.empty()) continue;

		if (names.count(name))
		{
			return Status::Warning("Duplicate category name detected: '" + std::string(name) + "'");
		}
		names.insert(name);
	}
	return Status::Ok();
}

}  // namespace SystemHealth::Category
