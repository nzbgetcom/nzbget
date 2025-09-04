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

#include <cerrno>
#include <cstdlib>

#include "SchedulerTaskValidator.h"

namespace SystemHealth::Scheduler
{
SchedulerTaskValidator::SchedulerTaskValidator(const ::Scheduler::Task& task)
	: m_task(task), m_name("Task" + std::to_string(m_task.GetId()))
{
	m_validators.reserve(4);
	m_validators.push_back(std::make_unique<TimeValidator>(task));
	m_validators.push_back(std::make_unique<WeekDaysValidator>(task));
	m_validators.push_back(std::make_unique<ParamValidator>(task));
	m_validators.push_back(std::make_unique<CommandValidator>(task));
}

Status TimeValidator::Validate() const
{
	int h = m_task.GetHours();
	int m = m_task.GetMinutes();

	// Assumption: -1 might represent a wildcard '*' in your internal logic
	// If not, remove the (< -1) check.
	if (h < -1 || h > 23)
	{
		return Status::Error("Invalid hour: " + std::to_string(h));
	}

	if (m < 0 || m > 59)
	{
		return Status::Error("Invalid minute: " + std::to_string(m));
	}

	return Status::Ok();
}

Status WeekDaysValidator::Validate() const
{
	int bits = m_task.GetWeeDaysBits();	 // Using the spelling from your snippet

	if (bits == 0)
	{
		return Status::Warning("Task is not scheduled for any days.");
	}

	if (bits < 0 || bits > 127)	 // Bits 1-7 (1<<0 to 1<<6) = max 127
	{
		return Status::Error("Invalid weekdays configuration.");
	}

	return Status::Ok();
}

// ParamValidator: validate that required params exist and are well-formed
Status ParamValidator::Validate() const
{
	const std::string param = m_task.GetParam();
	const auto cmd = m_task.GetCommand();

	switch (cmd)
	{
		case ::Scheduler::scDownloadRate:
		{
			if (param.empty())
				return Status::Warning(
					"" + std::string("No parameter for download rate; task will be ignored."));

			char* endptr = nullptr;
			errno = 0;
			long val = strtol(param.c_str(), &endptr, 10);
			if (endptr == param.c_str() || *endptr != '\0' || errno != 0)
			{
				return Status::Error("Invalid numeric parameter for download rate: " + param);
			}
			if (val < 0)
			{
				return Status::Error("Download rate must be >= 0: " + param);
			}
			return Status::Ok();
		}

		case ::Scheduler::scExtensions:
		case ::Scheduler::scProcess:
		case ::Scheduler::scFetchFeed:
		case ::Scheduler::scActivateServer:
		case ::Scheduler::scDeactivateServer:
		{
			if (param.empty())
			{
				return Status::Error(
					"Parameter required for this scheduled command but it's empty.");
			}
			return Status::Ok();
		}

		default:
			return Status::Ok();
	}
}

Status CommandValidator::Validate() const
{
	const auto cmd = m_task.GetCommand();

	// Validate that command is within the known enum range.
	switch (cmd)
	{
		case ::Scheduler::scPauseDownload:
		case ::Scheduler::scUnpauseDownload:
		case ::Scheduler::scPausePostProcess:
		case ::Scheduler::scUnpausePostProcess:
		case ::Scheduler::scDownloadRate:
		case ::Scheduler::scExtensions:
		case ::Scheduler::scProcess:
		case ::Scheduler::scPauseScan:
		case ::Scheduler::scUnpauseScan:
		case ::Scheduler::scActivateServer:
		case ::Scheduler::scDeactivateServer:
		case ::Scheduler::scFetchFeed:
			return Status::Ok();
		default:
			return Status::Error("Unknown scheduler command: " +
								 std::to_string(static_cast<int>(cmd)));
	}
}
}  // namespace SystemHealth::Scheduler
