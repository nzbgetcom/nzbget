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

#ifndef CATEGORY_VALIDATOR_H
#define CATEGORY_VALIDATOR_H

#include "SectionValidator.h"
#include "Options.h"

namespace SystemHealth::Category
{
class CategoryValidator final : public SectionValidator
{
public:
	explicit CategoryValidator(const Options& options, const Options::Category& category,
							   size_t id);
	std::string_view GetName() const override { return m_name; }

private:
	const Options& m_options;
	const Options::Category& m_category;
	const std::string m_name;
};

class NameValidator : public Validator
{
public:
	explicit NameValidator(const Options::Category& category) : m_category(category) {}

	std::string_view GetName() const override { return "Name"; }
	Status Validate() const override;

private:
	const Options::Category& m_category;
};

class UnpackValidator : public Validator
{
public:
	UnpackValidator(const Options& options, const Options::Category& category)
		: m_options(options), m_category(category)
	{
	}

	std::string_view GetName() const override { return "Unpack"; }
	Status Validate() const override;

private:
	const Options& m_options;
	const Options::Category& m_category;
};

}  // namespace SystemHealth::Category

#endif
