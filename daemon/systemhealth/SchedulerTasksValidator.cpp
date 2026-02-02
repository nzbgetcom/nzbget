/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 */

#include "nzbget.h"

#include "SchedulerTasksValidator.h"
#include "SchedulerTaskValidator.h"

namespace SystemHealth::Scheduler
{
SchedulerTasksValidator::SchedulerTasksValidator(const ::Scheduler::TaskList& tasks)
	: SectionGroupValidator(MakeTaskValidators(tasks)), m_tasks(tasks)
{
}

std::vector<std::unique_ptr<SectionValidator>> SchedulerTasksValidator::MakeTaskValidators(
	const ::Scheduler::TaskList& tasks) const
{
	std::vector<std::unique_ptr<SectionValidator>> validators;
	validators.reserve(tasks.size());
	for (const auto& task : tasks)
	{
		validators.push_back(std::make_unique<SchedulerTaskValidator>(*task));
	}
	return validators;
}

}  // namespace SystemHealth::SchedulerTask
