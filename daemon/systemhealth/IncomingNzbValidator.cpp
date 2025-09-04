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

#include "IncomingNzbValidator.h"

namespace SystemHealth::IncomingNzb
{
IncomingNzbValidator::IncomingNzbValidator(const Options& options) : m_options(options)
{
	m_validators.reserve(4);
	m_validators.push_back(std::make_unique<AppendCategoryDirValidator>(options));
	m_validators.push_back(std::make_unique<NzbDirIntervalValidator>(options));
	m_validators.push_back(std::make_unique<NzbDirFileAgeValidator>(options));
	m_validators.push_back(std::make_unique<DupeCheckValidator>(options));
}

Status AppendCategoryDirValidator::Validate() const
{
	bool append = m_options.GetAppendCategoryDir();
	if (append)
	{
		return Status::Info("Category subdirectories will be created automatically.");
	}
	return Status::Ok();
}

Status NzbDirIntervalValidator::Validate() const
{
	int interval = m_options.GetNzbDirInterval();
	if (interval < 0)
	{
		return Status::Error("Interval cannot be negative.");
	}

	if (interval == 0)
	{
		return Status::Info("Automatic NZB directory scanning is disabled.");
	}

	if (interval < 3)
	{
		std::stringstream ss;
		ss << "Interval is set to " << interval << " seconds. "
		   << "Very frequent scanning may cause high CPU/Disk usage.";
		return Status::Warning(ss.str());
	}

	return Status::Ok();
}

Status NzbDirFileAgeValidator::Validate() const
{
	int age = m_options.GetNzbDirFileAge();
	if (age < 0)
	{
		return Status::Error("File age cannot be negative.");
	}

	if (age > 3600)	 // 1 Hour
	{
		std::stringstream ss;
		ss << "File age is set to " << age << " seconds (> 1 hour). "
		   << "New NZB files will not be picked up until they are at least this old.";
		return Status::Info(ss.str());
	}

	return Status::Ok();
}

Status DupeCheckValidator::Validate() const
{
	bool dupeCheck = m_options.GetDupeCheck();
	auto healthCheckMode = m_options.GetHealthCheck();
	if (!dupeCheck)
	{
		return Status::Ok();
	}

	if (healthCheckMode == Options::EHealthCheck::hcPause)
	{
		return Status::Info(
			"DupeCheck is enabled while HealthCheck is set to 'Pause'. "
			"This configuration is not recommended as it requires manual intervention "
			"to unpause backup downloads if the primary one fails. "
			"Consider using 'Delete', 'Park', or 'None' for HealthCheck.");
	}

	return Status::Ok();
}
}  // namespace SystemHealth::IncomingNzb
