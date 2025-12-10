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

#include "CategoryValidator.h"
#include "Options.h"
#include <memory>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <cctype>

namespace SystemHealth::Category
{

CategoryValidator::CategoryValidator(const Options& options, const Options::Category& category,
									 size_t id)
	: m_options(options), m_category(category), m_name("Category" + std::to_string(id))
{
	m_validators.reserve(2);
	m_validators.push_back(std::make_unique<NameValidator>(category));
	m_validators.push_back(std::make_unique<UnpackValidator>(options, category));
}

Status NameValidator::Validate() const
{
	std::string_view name = m_category.GetName();
	if (name.empty())
	{
		return Status::Warning("Category name is empty. Categories should have a name");
	}
	return Status::Ok();
}

Status UnpackValidator::Validate() const
{
	bool globalUnpack = m_options.GetUnpack();
	bool catUnpack = m_category.GetUnpack();

	if (globalUnpack && !catUnpack)
	{
		return Status::Info(
			"Unpack is disabled. Files will remain packed after download");
	}

	if (!globalUnpack && catUnpack)
	{
		return Status::Info("Global Unpack is disabled, so this category setting has no effect");
	}

	return Status::Ok();
}
}  // namespace SystemHealth::Category
