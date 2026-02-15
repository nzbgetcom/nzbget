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
#include "Options.h"
#include "Status.h"
#include "Validators.h"
#include "PathsValidator.h"

namespace SystemHealth::Paths
{
PathsValidator::PathsValidator(const Options& options) : m_options(options)
{
	m_validators.reserve(13);
	m_validators.push_back(std::make_unique<MainDirValidator>(options));
	m_validators.push_back(std::make_unique<DestDirValidator>(options));
	m_validators.push_back(std::make_unique<InterDirValidator>(options));
	m_validators.push_back(std::make_unique<NzbDirValidator>(options));
	m_validators.push_back(std::make_unique<QueueDirValidator>(options));
	m_validators.push_back(std::make_unique<WebDirValidator>(options));
	m_validators.push_back(std::make_unique<TempDirValidator>(options));
	m_validators.push_back(std::make_unique<ScriptDirValidator>(options));
	m_validators.push_back(std::make_unique<ConfigTemplateValidator>(options));
	m_validators.push_back(std::make_unique<LogFileValidator>(options));
	m_validators.push_back(std::make_unique<CertStoreValidator>(options));
	m_validators.push_back(std::make_unique<RequiredDirValidator>(options));
#ifndef _WIN32
	m_validators.push_back(std::make_unique<LockFileValidator>(options));
#endif
}

Status MainDirValidator::Validate() const { return Validate(m_options.GetMainDirPath()); }

Status MainDirValidator::Validate(const boost::filesystem::path& path)
{
	return RequiredPathOption(Options::MAINDIR, path)
		.And(&Directory::Exists, path)
		.And(&Directory::Writable, path);
}

Status DestDirValidator::Validate() const
{
	return Validate(m_options.GetDestDirPath())
		.And(
			[&]()
			{
				return UniquePath(GetName(), m_options.GetDestDirPath(),
								  {{Options::MAINDIR, m_options.GetMainDirPath()}});
			});
}

Status DestDirValidator::Validate(const boost::filesystem::path& path)
{
	return RequiredPathOption(Options::DESTDIR, path)
		.And(&Directory::Exists, path)
		.And(&Directory::Writable, path);
}

Status InterDirValidator::Validate() const
{
	return Validate(m_options.GetInterDirPath())
		.And(
			[&]()
			{
				return UniquePath(GetName(), m_options.GetInterDirPath(),
								  {{Options::MAINDIR, m_options.GetMainDirPath()},
								   {Options::DESTDIR, m_options.GetDestDirPath()}});
			});
}

Status InterDirValidator::Validate(const boost::filesystem::path& path)
{
	if (path.empty())
		return Status::Warning(
			"'" + std::string(Options::INTERDIR) +
			"' is set to empty which is not recommended for optimal unpack performance");

	return Directory::Exists(path).And(&Directory::Writable, path);
}

Status NzbDirValidator::Validate() const
{
	return Validate(m_options.GetNzbDirPath())
		.And(
			[&]()
			{
				return UniquePath(GetName(), m_options.GetNzbDirPath(),
								  {{Options::MAINDIR, m_options.GetMainDirPath()},
								   {Options::DESTDIR, m_options.GetDestDirPath()},
								   {Options::INTERDIR, m_options.GetInterDirPath()}});
			});
}

Status NzbDirValidator::Validate(const boost::filesystem::path& path)
{
	return RequiredPathOption(Options::NZBDIR, path)
		.And(&Directory::Exists, path)
		.And(&Directory::Writable, path);
}

Status QueueDirValidator::Validate() const
{
	return Validate(m_options.GetQueueDirPath())
		.And(
			[&]()
			{
				return UniquePath(GetName(), m_options.GetQueueDirPath(),
								  {{Options::MAINDIR, m_options.GetMainDirPath()},
								   {Options::DESTDIR, m_options.GetDestDirPath()},
								   {Options::INTERDIR, m_options.GetInterDirPath()},
								   {Options::NZBDIR, m_options.GetNzbDirPath()}});
			});
}

Status QueueDirValidator::Validate(const boost::filesystem::path& path)
{
	return RequiredPathOption(Options::QUEUEDIR, path)
		.And(&Directory::Exists, path)
		.And(&Directory::Writable, path);
}

Status WebDirValidator::Validate() const
{
	const auto& path = m_options.GetWebDirPath();
	return Validate(path).And(
		[&]()
		{
			if (path.empty()) return Status::Ok();
			return UniquePath(GetName(), m_options.GetWebDirPath(),
							  {{Options::MAINDIR, m_options.GetMainDirPath()},
							   {Options::DESTDIR, m_options.GetDestDirPath()},
							   {Options::INTERDIR, m_options.GetInterDirPath()},
							   {Options::NZBDIR, m_options.GetNzbDirPath()},
							   {Options::QUEUEDIR, m_options.GetQueueDirPath()}});
		});
}

Status WebDirValidator::Validate(const boost::filesystem::path& path)
{
	if (path.empty()) return Status::Ok();
	return Directory::Exists(path).And(&Directory::Readable, path);
}

Status TempDirValidator::Validate() const
{
	return Validate(m_options.GetTempDirPath())
		.And(
			[&]()
			{
				return UniquePath(GetName(), m_options.GetTempDirPath(),
								  {{Options::MAINDIR, m_options.GetMainDirPath()},
								   {Options::DESTDIR, m_options.GetDestDirPath()},
								   {Options::INTERDIR, m_options.GetInterDirPath()},
								   {Options::NZBDIR, m_options.GetNzbDirPath()},
								   {Options::QUEUEDIR, m_options.GetQueueDirPath()},
								   {Options::WEBDIR, m_options.GetWebDirPath()}});
			});
}

Status TempDirValidator::Validate(const boost::filesystem::path& path)
{
	return RequiredPathOption(Options::TEMPDIR, path)
		.And(&Directory::Exists, path)
		.And(&Directory::Writable, path);
}

Status ScriptDirValidator::Validate() const
{
	const auto& paths = m_options.GetScriptDirPaths();
	if (paths.empty())
	{
		return Status::Error("'" + std::string(Options::SCRIPTDIR) +
							 "' is required and cannot be empty");
	}

	for (const auto& dir : paths)
	{
		auto status = Validate(dir).And(
			[&]()
			{
				return UniquePath(GetName(), dir,
								  {{Options::MAINDIR, m_options.GetMainDirPath()},
								   {Options::DESTDIR, m_options.GetDestDirPath()},
								   {Options::INTERDIR, m_options.GetInterDirPath()},
								   {Options::NZBDIR, m_options.GetNzbDirPath()},
								   {Options::QUEUEDIR, m_options.GetQueueDirPath()},
								   {Options::WEBDIR, m_options.GetWebDirPath()},
								   {Options::TEMPDIR, m_options.GetTempDirPath()}});
			});
		if (!status.IsOk())
		{
			return status;
		}
	}

	return Status::Ok();
}

Status ScriptDirValidator::Validate(const boost::filesystem::path& path)
{
	return RequiredPathOption(Options::SCRIPTDIR, path)
		.And(&Directory::Exists, path)
		.And(&Directory::Writable, path);
}

Status ConfigTemplateValidator::Validate() const { return Status::Ok(); }

Status LogFileValidator::Validate() const
{
	return Validate(m_options.GetLogFilePath(), m_options.GetWriteLog())
		.And(
			[&]()
			{
				return UniquePath(GetName(), m_options.GetLogFilePath(),
								  {{Options::CONFIGTEMPLATE, m_options.GetConfigTemplatePath()},
								   {Options::CONFIGFILE, m_options.GetConfigFilePath()}});
			});
}

Status LogFileValidator::Validate(const boost::filesystem::path& path, Options::EWriteLog writeLog)
{
	if (writeLog == Options::EWriteLog::wlNone) return Status::Ok();

	if (path.empty())
	{
		return Status::Error("Logging is enabled, but '" + std::string(Options::LOGFILE) +
							 "' is set to empty");
	}

	if (writeLog == Options::EWriteLog::wlRotate && path.has_parent_path())
	{
		const auto&& parent = path.parent_path();
		return Directory::Exists(parent).And(&Directory::Writable, parent);
	}

	return File::Exists(path).And(&File::Writable, path);
}

Status CertStoreValidator::Validate() const
{
	return Validate(m_options.GetCertStore(), m_options.GetCertCheck())
		.And(
			[&]()
			{
				return UniquePath(GetName(), m_options.GetCertStorePath(),
								  {{Options::MAINDIR, m_options.GetMainDirPath()},
								   {Options::DESTDIR, m_options.GetDestDirPath()},
								   {Options::INTERDIR, m_options.GetInterDirPath()},
								   {Options::NZBDIR, m_options.GetNzbDirPath()},
								   {Options::QUEUEDIR, m_options.GetQueueDirPath()},
								   {Options::WEBDIR, m_options.GetWebDirPath()},
								   {Options::TEMPDIR, m_options.GetTempDirPath()},
								   {Options::CONFIGTEMPLATE, m_options.GetConfigTemplatePath()},
								   {Options::LOGFILE, m_options.GetLogFilePath()},
								   {Options::CONFIGFILE, m_options.GetConfigFilePath()}});
			});
}

Status CertStoreValidator::Validate(const boost::filesystem::path& path, bool certCheck)
{
	if (path.empty() && !certCheck) return Status::Ok();

	if (path.empty() && certCheck)
		return Status::Error("'" + std::string(Options::CERTCHECK) +
							 "' requires proper configuration of option '" +
							 std::string(Options::CERTSTORE) + "'");

	const auto file = File::Exists(path);
	if (!file.IsError()) return File::Readable(path);

	const auto dir = Directory::Exists(path);
	if (!dir.IsError()) return Directory::Readable(path);

	return Status::Error("'" + std::string(Options::CERTSTORE) + "' must be a file or a directory");
}

Status RequiredDirValidator::Validate() const { return Status::Ok(); }

#ifndef _WIN32
Status LockFileValidator::Validate() const
{
	return Validate(m_options.GetLockFilePath(), m_options.GetDaemonMode())
		.And(
			[&]()
			{
				return UniquePath(GetName(), m_options.GetLockFilePath(),
								  {{Options::MAINDIR, m_options.GetMainDirPath()},
								   {Options::CONFIGTEMPLATE, m_options.GetConfigTemplatePath()},
								   {Options::LOGFILE, m_options.GetLogFilePath()},
								   {Options::CERTSTORE, m_options.GetCertStorePath()},
								   {Options::CONFIGFILE, m_options.GetConfigFilePath()}});
			});
}

Status LockFileValidator::Validate(const boost::filesystem::path& path, bool daemonMode)
{
	if (path.empty() && daemonMode)
	{
		return Status::Warning(
			"'" + std::string(Options::LOCKFILE) +
			"' is set to empty. The check for another running instance is disabled");
	}

	if (daemonMode)
	{
		return File::Exists(path);
	}

	return Status::Ok();
}
#endif
}  // namespace SystemHealth::Paths
