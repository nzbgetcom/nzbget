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

#ifndef INCOMING_NZB_VALIDATOR_H
#define INCOMING_NZB_VALIDATOR_H

#include "SectionValidator.h"
#include "Options.h"

namespace SystemHealth::IncomingNzb
{
class IncomingNzbValidator final : public SectionValidator
{
public:
	explicit IncomingNzbValidator(const Options& options);
	std::string_view GetName() const override { return "IncomingNzbs"; }

private:
	const Options& m_options;
};

class AppendCategoryDirValidator final : public Validator
{
public:
	explicit AppendCategoryDirValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::APPENDCATEGORYDIR; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class NzbDirIntervalValidator final : public Validator
{
public:
	explicit NzbDirIntervalValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::NZBDIRINTERVAL; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class NzbDirFileAgeValidator final : public Validator
{
public:
	explicit NzbDirFileAgeValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::NZBDIRFILEAGE; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class DupeCheckValidator final : public Validator
{
public:
	explicit DupeCheckValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::DUPECHECK; }
	Status Validate() const override;

private:
	const Options& m_options;
};

}  // namespace SystemHealth::IncomingNzb

#endif
