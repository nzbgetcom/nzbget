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

#include "CheckAndRepairValidator.h"
#include "Options.h"
#include "Validators.h"
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace SystemHealth::CheckAndRepair
{
CheckAndRepairValidator::CheckAndRepairValidator(const Options& options) : m_options(options)
{
	m_validators.reserve(16);
	m_validators.push_back(std::make_unique<CrcCheckValidator>(options));
	m_validators.push_back(std::make_unique<ParCheckValidator>(options));
	m_validators.push_back(std::make_unique<ParRepairValidator>(options));
	m_validators.push_back(std::make_unique<ParScanValidator>(options));
	m_validators.push_back(std::make_unique<ParQuickValidator>(options));
	m_validators.push_back(std::make_unique<ParBufferValidator>(options));
	m_validators.push_back(std::make_unique<ParThreadsValidator>(options));
	m_validators.push_back(std::make_unique<ParIgnoreExtValidator>(options));
	m_validators.push_back(std::make_unique<ParRenameValidator>(options));
	m_validators.push_back(std::make_unique<RarRenameValidator>(options));
	m_validators.push_back(std::make_unique<DirectRenameValidator>(options));
	m_validators.push_back(std::make_unique<HardLinkingValidator>(options));
	m_validators.push_back(std::make_unique<HardLinkingIgnoreExtValidator>(options));
	m_validators.push_back(std::make_unique<HealthCheckValidator>(options));
	m_validators.push_back(std::make_unique<ParTimeLimitValidator>(options));
	m_validators.push_back(std::make_unique<ParPauseQueueValidator>(options));
}

Status CrcCheckValidator::Validate() const
{
	if (!m_options.GetCrcCheck())
	{
		return Status::Info("Normally, '" + std::string(Options::CRCCHECK) +
							"' should be enabled to better detect download errors and for "
							"quick par-verification");
	}
	return Status::Ok();
}

Status ParCheckValidator::Validate() const
{
	if (!m_options.GetParRepair()) return Status::Ok();

	const auto parCheck = m_options.GetParCheck();
	switch (parCheck)
	{
		case Options::EParCheck::pcAuto:
			return Status::Ok();

		case Options::EParCheck::pcAlways:
			return Status::Info("'" + std::string(Options::PARCHECK) +
								"' is set to 'Always'. "
								"Verification runs on every download, even undamaged ones. "
								"Use 'Auto' to skip unnecessary checks and save CPU");

		case Options::EParCheck::pcForce:
			return Status::Info(
				"'" + std::string(Options::PARCHECK) +
				"' is set to 'Force'. "
				"All PAR2 files are downloaded immediately, wasting bandwidth and CPU. "
				"'Auto' is recommended");

		case Options::EParCheck::pcManual:
			return Status::Warning(
				"Automatic repair is disabled. "
				"Damaged downloads will require manual intervention");
	}
	return Status::Ok();
}

Status ParRepairValidator::Validate() const
{
	if (!m_options.GetParRepair())
	{
		return Status::Warning("'" + std::string(Options::PARREPAIR) +
							   "' is off. Corrupted files won't be automatically repaired");
	}
	return Status::Ok();
}

Status ParScanValidator::Validate() const { return Status::Ok(); }

Status ParQuickValidator::Validate() const
{
	if (!m_options.GetParQuick() && !m_options.GetParRepair()) return Status::Ok();
	if (!m_options.GetParQuick())
	{
		return Status::Info(
			"'" + std::string(Options::PARQUICK) +
			"' is off. Files will be verified by reading from disk, which is slower");
	}

	return Status::Ok();
}

Status ParBufferValidator::Validate() const
{
	if (!m_options.GetParRepair()) return Status::Ok();

	Status s = CheckPositiveNum(Options::PARBUFFER, m_options.GetParBuffer());
	if (!s.IsOk()) return s;

	const int buffer = m_options.GetParBuffer();
	if (buffer < 250)
	{
		return Status::Info(
			"'" + std::string(Options::PARBUFFER) + "' is set to " + std::to_string(buffer) +
			" MB. "
			"Increasing to 250 MB or higher is recommended to speed up verification and repair");
	}

	return Status::Ok();
}

Status ParThreadsValidator::Validate() const
{
	if (!m_options.GetParRepair()) return Status::Ok();

	Status s = CheckPositiveNum(Options::PARTHREADS, m_options.GetParThreads());
	if (!s.IsOk()) return s;

	const size_t threads = static_cast<size_t>(m_options.GetParThreads());
	if (threads == 0) return Status::Ok();

	const size_t hwThreads = std::thread::hardware_concurrency();
	if (hwThreads > 0 && threads > hwThreads)
	{
		return Status::Warning("'" + std::string(Options::PARTHREADS) + "' (" +
							   std::to_string(threads) + ") exceeds the number of CPU cores (" +
							   std::to_string(hwThreads) +
							   "). "
							   "This may slow down repair due to excessive context switching");
	}

	return Status::Ok();
}

Status ParIgnoreExtValidator::Validate() const { return Status::Ok(); }

Status ParRenameValidator::Validate() const
{
	if (!m_options.GetParRename())
	{
		return Status::Info(std::string(Options::PARRENAME) +
							" is off. Original file names won't be restored from par2-files");
	}
	return Status::Ok();
}

Status RarRenameValidator::Validate() const
{
	if (!m_options.GetRarRename())
		return Status::Info("'" + std::string(Options::RARRENAME) +
							"' is off. Original file names won't be restored from rar-files");
	return Status::Ok();
}

Status DirectRenameValidator::Validate() const { return Status::Ok(); }

Status HardLinkingValidator::Validate() const
{
	if (!m_options.GetHardLinking()) return Status::Ok();
	if (!m_options.GetDirectRename())
	{
		return Status::Warning("'" + std::string(Options::HARDLINKING) + "' is enabled, but '" +
							   std::string(Options::DIRECTRENAME) +
							   "' is disabled. Hard linking will not be performed");
	}

	return Status::Ok();
}

Status HardLinkingIgnoreExtValidator::Validate() const { return Status::Ok(); }

Status HealthCheckValidator::Validate() const
{
	const auto healthCheck = m_options.GetHealthCheck();
	switch (healthCheck)
	{
		case Options::EHealthCheck::hcDelete:
		case Options::EHealthCheck::hcPark:
		case Options::EHealthCheck::hcNone:
			return Status::Ok();
		case Options::EHealthCheck::hcPause:
			return Status::Warning("'" + std::string(Options::HEALTHCHECK) +
								   "' is set to 'Pause' you will need to manually move another "
								   "duplicate from history to queue");

		default:
			return Status::Ok();
	}
}

Status ParTimeLimitValidator::Validate() const
{
	if (!m_options.GetParRepair()) return Status::Ok();

	Status s = CheckPositiveNum(Options::PARTIMELIMIT, m_options.GetParTimeLimit());
	if (!s.IsOk()) return s;

	const int limit = m_options.GetParTimeLimit();
	if (limit == 0) return Status::Ok();
	if (limit < 5)
	{
		return Status::Info("'" + std::string(Options::PARTIMELIMIT) + "' is set to " +
							std::to_string(limit) +
							" minutes. "
							"Large repairs often take longer, risking premature cancellation");
	}

	return Status::Ok();
}

Status ParPauseQueueValidator::Validate() const { return Status::Ok(); }

}  // namespace SystemHealth::CheckAndRepair
