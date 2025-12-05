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

#include "DisplayValidator.h"
#include "Status.h"

namespace SystemHealth::Display
{

DisplayValidator::DisplayValidator(const Options& options) : m_options(options)
{
	m_validators.reserve(6);
	m_validators.push_back(std::make_unique<OutputModeValidator>(options));
	m_validators.push_back(std::make_unique<UpdateIntervalValidator>(options));
	m_validators.push_back(std::make_unique<CursesNzbNameValidator>(options));
	m_validators.push_back(std::make_unique<CursesGroupValidator>(options));
	m_validators.push_back(std::make_unique<CursesTimeValidator>(options));
}

Status OutputModeValidator::Validate() const
{
	auto mode = m_options.GetOutputMode();

	if (mode == Options::EOutputMode::omLoggable || mode == Options::EOutputMode::omColored ||
		mode == Options::EOutputMode::omNCurses)
	{
		return Status::Ok();
	}

	return Status::Error("'" + std::string(Options::OUTPUTMODE) + "' has an invalid value");
}

Status UpdateIntervalValidator::Validate() const
{
	int v = m_options.GetUpdateInterval();
	if (v == 0) return Status::Ok();
	if (v < 25)
		return Status::Error("'" + std::string(Options::UPDATEINTERVAL) + "' must be >= 25 ms.");
	return Status::Ok();
}

Status CursesNzbNameValidator::Validate() const
{
	if (m_options.GetOutputMode() != Options::EOutputMode::omNCurses &&
		m_options.GetCursesNzbName())
	{
		return Status::Info("'" + std::string(Options::CURSESNZBNAME) +
							"' applies only when OutputMode=curses");
	}
	return Status::Ok();
}

Status CursesGroupValidator::Validate() const
{
	if (m_options.GetOutputMode() != Options::EOutputMode::omNCurses && m_options.GetCursesGroup())
	{
		return Status::Info("'" + std::string(Options::CURSESGROUP) +
							"' applies only when OutputMode=curses");
	}
	return Status::Ok();
}

Status CursesTimeValidator::Validate() const
{
	if (m_options.GetOutputMode() != Options::EOutputMode::omNCurses && m_options.GetCursesTime())
	{
		return Status::Info("'" + std::string(Options::CURSESTIME) +
							"' applies only when OutputMode=curses");
	}
	return Status::Ok();
}

}  // namespace SystemHealth::Display
