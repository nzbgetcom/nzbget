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

#ifndef SCHEDULER_TASK_VALIDATOR_H
#define SCHEDULER_TASK_VALIDATOR_H

#include "SectionValidator.h"
#include "Scheduler.h"
#include "Validators.h"

namespace SystemHealth::Scheduler
{
class SchedulerTaskValidator final : public SectionValidator
{
public:
	explicit SchedulerTaskValidator(const ::Scheduler::Task& task);
	std::string_view GetName() const override { return m_name; }

private:
	const ::Scheduler::Task& m_task;
	const std::string m_name;
};

class TimeValidator final : public Validator
{
public:
	explicit TimeValidator(const ::Scheduler::Task& task) : m_task(task) {}
	std::string_view GetName() const override { return "Time"; }
	Status Validate() const override;

private:
	const ::Scheduler::Task& m_task;
};

class WeekDaysValidator final : public Validator
{
public:
	explicit WeekDaysValidator(const ::Scheduler::Task& task) : m_task(task) {}
	std::string_view GetName() const override { return "WeekDays"; }
	Status Validate() const override;

private:
	const ::Scheduler::Task& m_task;
};

class ParamValidator final : public Validator
{
public:
	explicit ParamValidator(const ::Scheduler::Task& task) : m_task(task) {}
	std::string_view GetName() const override { return "Param"; }
	Status Validate() const override;

private:
	const ::Scheduler::Task& m_task;
};

class CommandValidator final : public Validator
{
public:
	explicit CommandValidator(const ::Scheduler::Task& task) : m_task(task) {}
	std::string_view GetName() const override { return "Command"; }
	Status Validate() const override;

private:
	const ::Scheduler::Task& m_task;
};

}  // namespace SystemHealth::Scheduler

#endif
