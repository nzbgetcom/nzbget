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

namespace SystemHealth::CheckAndRepair
{
CheckAndRepairValidator::CheckAndRepairValidator(const Options& options) : m_options(options)
{
	m_validators.reserve(14);
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
	m_validators.push_back(std::make_unique<HealthCheckValidator>(options));
	m_validators.push_back(std::make_unique<ParTimeLimitValidator>(options));
	m_validators.push_back(std::make_unique<ParPauseQueueValidator>(options));
}

Status CrcCheckValidator::Validate() const
{
	if (!m_options.GetCrcCheck())
	{
		return Status::Warning(std::string(Options::CRCCHECK) +
							   " is off. File integrity won't be verified during download.");
	}
	return Status::Ok();
}

Status ParCheckValidator::Validate() const
{
	auto parCheck = m_options.GetParCheck();
	switch (parCheck)
	{
		case Options::EParCheck::pcAuto:
			return Status::Ok();
		case Options::EParCheck::pcAlways:
			return Status::Info(
				std::string(Options::PARCHECK) +
				" is set to always. Every download will be checked, even if undamaged.");
		case Options::EParCheck::pcForce:
			return Status::Info(std::string(Options::PARCHECK) +
								" is set to force. All par2-files will be downloaded and checked.");
		case Options::EParCheck::pcManual:
			return Status::Info(std::string(Options::PARCHECK) +
								" is set to manual. Repair must be done manually if needed.");
	}
	return Status::Ok();
}

Status ParRepairValidator::Validate() const
{
	if (!m_options.GetParRepair())
	{
		return Status::Warning(std::string(Options::PARREPAIR) +
							   " is off. Corrupted files won't be automatically repaired.");
	}
	return Status::Ok();
}

Status ParScanValidator::Validate() const
{
	auto parScan = m_options.GetParScan();
	switch (parScan)
	{
		case Options::EParScan::psLimited:
			return Status::Info(std::string(Options::PARSCAN) +
								" is set to limited. Only files in par-set are scanned.");
		case Options::EParScan::psExtended:
			return Status::Ok();
		case Options::EParScan::psFull:
			return Status::Info(std::string(Options::PARSCAN) +
								" is set to full. All files in destination directory are scanned. "
								"Can be time-consuming.");
		case Options::EParScan::psDupe:
			return Status::Info(std::string(Options::PARSCAN) +
								" is set to dupe. Even files from duplicate downloads are scanned. "
								"Can be very time-consuming.");
	}
	return Status::Ok();
}

Status ParQuickValidator::Validate() const
{
	if (!m_options.GetParQuick())
	{
		return Status::Info(
			std::string(Options::PARQUICK) +
			" is off. Files will be verified by reading from disk, which is slower.");
	}
	return Status::Ok();
}

Status ParBufferValidator::Validate() const
{
	Status s = CheckPositiveNum(Options::PARBUFFER, m_options.GetParBuffer());
	if (!s.IsOk()) return s;

	const int buffer = m_options.GetParBuffer();
	if (buffer < 8)
	{
		return Status::Info(std::string(Options::PARBUFFER) + " is " + std::to_string(buffer) +
							" MB. Consider at least 8 MB for better repair performance.");
	}
	if (buffer > 512)
	{
		return Status::Info(std::string(Options::PARBUFFER) + " is " + std::to_string(buffer) +
							" MB. Very large buffers may consume excessive memory.");
	}
	return Status::Ok();
}

Status ParThreadsValidator::Validate() const
{
	Status s = CheckPositiveNum(Options::PARTHREADS, m_options.GetParThreads());
	if (!s.IsOk()) return s;

	const int threads = m_options.GetParThreads();
	if (threads == 0) return Status::Ok();
	if (threads > 16)
	{
		return Status::Info(std::string(Options::PARTHREADS) + " is " + std::to_string(threads) +
							". Very high thread counts may overload the system.");
	}
	return Status::Ok();
}

Status ParIgnoreExtValidator::Validate() const
{
	const char* ignoreExt = m_options.GetParIgnoreExt();
	if (ignoreExt && *ignoreExt != '\0')
	{
		return Status::Info(
			std::string(Options::PARIGNOREEXT) +
			" is set. Files matching the pattern will be ignored during par-check.");
	}
	return Status::Ok();
}

Status ParTimeLimitValidator::Validate() const
{
	Status s = CheckPositiveNum(Options::PARTIMELIMIT, m_options.GetParTimeLimit());
	if (!s.IsOk()) return s;

	const int limit = m_options.GetParTimeLimit();
	if (limit == 0) return Status::Ok();
	if (limit < 5)
	{
		return Status::Info(std::string(Options::PARTIMELIMIT) + " is " + std::to_string(limit) +
							" minutes. Very short limits may cancel repairs prematurely.");
	}
	return Status::Ok();
}

Status ParRenameValidator::Validate() const
{
	if (!m_options.GetParRename())
	{
		return Status::Info(std::string(Options::PARRENAME) +
							" is off. Original file names won't be restored from par2-files.");
	}
	return Status::Ok();
}

Status RarRenameValidator::Validate() const
{
	if (!m_options.GetRarRename())
	{
		return Status::Info(std::string(Options::RARRENAME) +
							" is off. Original file names won't be restored from rar-files.");
	}
	return Status::Ok();
}

Status DirectRenameValidator::Validate() const
{
	if (m_options.GetDirectRename())
	{
		return Status::Info(
			std::string(Options::DIRECTRENAME) +
			" is on. Files will be renamed during download (only works for healthy downloads).");
	}
	return Status::Ok();
}

Status HealthCheckValidator::Validate() const
{
	auto healthCheck = m_options.GetHealthCheck();
	switch (healthCheck)
	{
		case Options::EHealthCheck::hcNone:
			return Status::Ok();
		case Options::EHealthCheck::hcDelete:
			return Status::Info(
				std::string(Options::HEALTHCHECK) +
				" is set to delete. Downloads with critical health will be deleted.");
		case Options::EHealthCheck::hcPark:
			return Status::Ok();
		case Options::EHealthCheck::hcPause:
			return Status::Info(std::string(Options::HEALTHCHECK) +
								" is set to pause. Downloads with critical health will be paused.");
	}
	return Status::Ok();
}

Status ParPauseQueueValidator::Validate() const
{
	if (m_options.GetParPauseQueue())
	{
		return Status::Info(
			std::string(Options::PARPAUSEQUEUE) +
			" is on. Download queue will pause during par-check/repair to give CPU more time.");
	}
	return Status::Ok();
}

}  // namespace SystemHealth::CheckAndRepair
