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

#ifndef DISPLAY_VALIDATOR_H
#define DISPLAY_VALIDATOR_H

#include "SectionValidator.h"
#include "Options.h"

namespace SystemHealth::Display
{

class DisplayValidator final : public SectionValidator
{
public:
	explicit DisplayValidator(const Options& options);
	std::string_view GetName() const override { return "Display"; }
	SectionReport Validate() const override { return SectionValidator::Validate(); }

private:
	const Options& m_options;
};

class OutputModeValidator final : public Validator
{
public:
	explicit OutputModeValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::OUTPUTMODE; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class UpdateIntervalValidator final : public Validator
{
public:
	explicit UpdateIntervalValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::UPDATEINTERVAL; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class CursesNzbNameValidator final : public Validator
{
public:
	explicit CursesNzbNameValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::CURSESNZBNAME; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class CursesGroupValidator final : public Validator
{
public:
	explicit CursesGroupValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::CURSESGROUP; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class CursesTimeValidator final : public Validator
{
public:
	explicit CursesTimeValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::CURSESTIME; }
	Status Validate() const override;

private:
	const Options& m_options;
};

}  // namespace SystemHealth::Display

#endif
