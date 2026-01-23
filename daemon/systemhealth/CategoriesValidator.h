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

#ifndef CATEGORIES_VALIDATOR_H
#define CATEGORIES_VALIDATOR_H

#include "Options.h"
#include "SectionGroupValidator.h"
#include "SectionValidator.h"

namespace SystemHealth::Category
{

class CategoriesValidator final : public SectionGroupValidator
{
public:
	explicit CategoriesValidator(const Options& options, const Options::Categories& categories);
	std::string_view GetName() const override { return "Categories"; }

private:
	const Options& m_options;
	const Options::Categories& m_categories;

	std::vector<std::unique_ptr<SectionValidator>> MakeCategoryValidators(
		const Options& options, const Options::Categories& categories) const;
};

class DuplicateNamesValidator : public Validator
{
public:
	explicit DuplicateNamesValidator(const Options::Categories& categories)
		: m_categories(categories)
	{
	}
	std::string_view GetName() const override { return ""; }
	Status Validate() const override;

private:
	const Options::Categories& m_categories;
};

}  // namespace SystemHealth::Category

#endif
