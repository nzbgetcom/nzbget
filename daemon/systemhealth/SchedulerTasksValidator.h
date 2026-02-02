/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 */

#ifndef SCHEDULER_TASKS_VALIDATOR_H
#define SCHEDULER_TASKS_VALIDATOR_H

#include "SectionGroupValidator.h"
#include "SectionValidator.h"
#include "Scheduler.h"

namespace SystemHealth::Scheduler
{
class SchedulerTasksValidator final : public SectionGroupValidator
{
public:
	explicit SchedulerTasksValidator(const ::Scheduler::TaskList& tasks);
	std::string_view GetName() const override { return "Scheduler"; }

private:
	const ::Scheduler::TaskList& m_tasks;

	std::vector<std::unique_ptr<SectionValidator>> MakeTaskValidators(
		const ::Scheduler::TaskList& tasks) const;
};

}  // namespace SystemHealth::SchedulerTask

#endif
