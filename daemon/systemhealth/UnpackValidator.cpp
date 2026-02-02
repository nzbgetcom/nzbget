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

#include "SectionValidator.h"
#include "UnpackValidator.h"
#include "Options.h"
#include "Validators.h"

namespace SystemHealth::Unpack
{

UnpackValidator::UnpackValidator(const Options& options) : m_options(options)
{
	m_validators.reserve(10);
	m_validators.push_back(std::make_unique<UnpackEnabledValidator>(options));
	m_validators.push_back(std::make_unique<DirectUnpackValidator>(options));
	m_validators.push_back(std::make_unique<UseTempUnpackDirValidator>(options));
	m_validators.push_back(std::make_unique<UnpackCleanupDiskValidator>(options));
	m_validators.push_back(std::make_unique<UnpackPauseQueueValidator>(options));
	m_validators.push_back(std::make_unique<UnrarCmdValidator>(options));
	m_validators.push_back(std::make_unique<SevenZipCmdValidator>(options));
	m_validators.push_back(std::make_unique<ExtensionCleanupValidator>(options));
	m_validators.push_back(std::make_unique<UnpackPassFileValidator>(options));
	m_validators.push_back(std::make_unique<UnpackIgnoreExtValidator>(options));
}

Status UnpackEnabledValidator::Validate() const
{
	if (!m_options->GetUnpack())
	{
		return Status::Warning("Unpacking is disabled - archives will not be extracted");
	}
	return Status::Ok();
}

Status DirectUnpackValidator::Validate() const
{
	bool directUnpack = m_options->GetDirectUnpack();
	bool unpack = m_options->GetUnpack();

	if (directUnpack && !unpack)
	{
		return Status::Warning("'" + std::string(Options::DIRECTUNPACK) + "' is enabled, but '" +
							   std::string(Options::UNPACK) +
							   "' is disabled. Direct unpacking will not work");
	}

	return Status::Ok();
}

Status UseTempUnpackDirValidator::Validate() const { return Status::Ok(); }

Status UnpackCleanupDiskValidator::Validate() const
{
	if (!m_options->GetUnpack())
	{
		return Status::Info(
			"'" + std::string(Options::UNPACKCLEANUPDISK) +
			"' is configured but global Unpack is disabled; cleanup will not occur");
	}
	return Status::Ok();
}

Status UnpackPauseQueueValidator::Validate() const
{
	if (m_options->GetUnpackPauseQueue() && !m_options->GetUnpack())
	{
		return Status::Info("'" + std::string(Options::UNPACKPAUSEQUEUE) +
							"' has no effect because Unpack is disabled");
	}
	return Status::Ok();
}

Status UnrarCmdValidator::Validate() const
{
	if (!m_options->GetUnpack()) return Status::Ok();

	if (m_options->GetUnrarPath().empty())
	{
		return Status::Warning("'" + std::string(Options::UNRARCMD) +
							   "' is not configured. RAR archives cannot be unpacked");
	}

	const auto exists = File::Exists(m_options->GetUnrarPath());
	if (!exists.IsOk()) return exists;

	const auto exe = File::Executable(m_options->GetUnrarPath());
	if (!exe.IsOk()) return exe;

	return Status::Ok();
}

Status SevenZipCmdValidator::Validate() const
{
	if (!m_options->GetUnpack()) return Status::Ok();
	if (m_options->GetSevenZipPath().empty())
	{
		return Status::Warning("'" + std::string(Options::SEVENZIPCMD) +
							   "' is not configured. "
							   "This prevents unpacking 7z archives and installing extensions");
	}

	const auto exists = File::Exists(m_options->GetSevenZipPath());
	if (!exists.IsOk()) return exists;

	const auto exe = File::Executable(m_options->GetSevenZipPath());
	if (!exe.IsOk()) return exe;

	return Status::Ok();
}

Status ExtensionCleanupValidator::Validate() const
{
	std::string_view ext = m_options->GetExtCleanupDisk();
	if (ext.empty()) return Status::Ok();
	if (ext.size() > 512)
	{
		return Status::Warning("'" + std::string(Options::EXTCLEANUPDISK) +
							   "' value is unusually long");
	}
	return Status::Ok();
}

Status UnpackPassFileValidator::Validate() const
{
	// Note: We check this even if Unpack is disabled, because the user might
	// enable Unpack later and forget the file path is invalid.
	std::string_view unpackPassFile = m_options->GetUnpackPassFile();
	if (!unpackPassFile.empty())
	{
		return File::Exists(unpackPassFile);
	}

	return Status::Ok();
}

Status UnpackIgnoreExtValidator::Validate() const
{
	std::string_view val = m_options->GetUnpackIgnoreExt();
	if (val.empty()) return Status::Ok();
	if (val.size() > 512)
	{
		return Status::Warning("'" + std::string(Options::UNPACKIGNOREEXT) + "' is unusually long");
	}
	return Status::Ok();
}

}  // namespace SystemHealth::Unpack
