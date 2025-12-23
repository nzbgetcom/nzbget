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

#include "LoggingValidator.h"
#include "Options.h"
#include "Validators.h"
#include <memory>
#include <string>

namespace SystemHealth::Logging
{
LoggingValidator::LoggingValidator(const Options& options) : m_options(options)
{
	m_validators.reserve(7);
	m_validators.push_back(std::make_unique<WriteLogValidator>(options));
	m_validators.push_back(std::make_unique<RotateLogValidator>(options));
	m_validators.push_back(std::make_unique<LogBufferValidator>(options));
	m_validators.push_back(std::make_unique<NzbLogValidator>(options));
	m_validators.push_back(std::make_unique<CrashTraceValidator>(options));
	m_validators.push_back(std::make_unique<CrashDumpValidator>(options));
	m_validators.push_back(std::make_unique<TimeCorrectionValidator>(options));
}

Status WriteLogValidator::Validate() const
{
	const auto writeLog = m_options.GetWriteLog();
	switch (writeLog)
	{
		case Options::EWriteLog::wlNone:
			return Status::Info(
				"Logging is disabled. Logging is recommended for "
				"effective debugging and troubleshooting");
		case Options::EWriteLog::wlAppend:
			return Status::Warning("'" + std::string(Options::WRITELOG) +
								   "' is set to 'Append'. The log file may grow indefinitely");
		case Options::EWriteLog::wlReset:
		case Options::EWriteLog::wlRotate:
			return Status::Ok();
	}
	return Status::Ok();
}

Status RotateLogValidator::Validate() const
{
	if (m_options.GetWriteLog() != Options::EWriteLog::wlRotate) return Status::Ok();

	Status s = CheckPositiveNum(Options::ROTATELOG, m_options.GetRotateLog());
	if (!s.IsOk()) return s;

	int rotateLog = m_options.GetRotateLog();
	if (rotateLog > 365)
		return Status::Warning("'" + std::string(Options::ROTATELOG) +
							   "' is very high (>365). This may consume significant disk space");

	return Status::Ok();
}
Status LogBufferValidator::Validate() const
{
	Status s = CheckPositiveNum(Options::LOGBUFFER, m_options.GetLogBuffer());
	if (!s.IsOk()) return s;

	if (m_options.GetLogBuffer() < 100)
		return Status::Info("'" + std::string(Options::LOGBUFFER) +
							"' is very low. You might miss recent messages in the web UI");
	return Status::Ok();
}

Status CrashDumpValidator::Validate() const
{
#ifdef __linux__
	if (m_options.GetCrashDump())
		return Status::Info(
			"'" + std::string(Options::CRASHDUMP) +
			"' is enabled. Memory dumps may contain sensitive data (passwords, keys)");
#endif

	return Status::Ok();
}

Status TimeCorrectionValidator::Validate() const
{
	int val = m_options.GetTimeCorrection();

	// Config says: -24..+24 are hours, others are minutes.
	// If someone sets 10000 (minutes), that's ~7 days offset, likely a mistake.
	if (std::abs(val) > 1440 && std::abs(val) < 1000000)  // 1440 mins = 24 hours
		return Status::Warning("'" + std::string(Options::TIMECORRECTION) +
							   "' value is very large (interpreted as minutes)");

	return Status::Ok();
}

}  // namespace SystemHealth::Logging