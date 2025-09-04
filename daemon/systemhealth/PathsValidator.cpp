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

#include "Validators.h"
#include "PathsValidator.h"
#include "Util.h"

namespace SystemHealth::Paths
{
PathsValidator::PathsValidator(const Options& options) : m_options(options)
{
	m_validators.reserve(14);
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
	return RequiredDir(Options::MAINDIR, path).And([&]() { return Directory::Writable(path); });
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
	return RequiredDir(Options::DESTDIR, path).And([&]() { return Directory::Writable(path); });
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
	const std::string name(Options::INTERDIR);

	if (path.empty())
		return Status::Warning(name + " is set to empty. Using " + name +
							   " is recommended for optimal unpack performance.");

	return RequiredDir(name, path).And([&]() { return Directory::Writable(path); });
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
	return RequiredDir(Options::NZBDIR, path).And([&]() { return Directory::Writable(path); });
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
	return RequiredDir(Options::QUEUEDIR, path).And([&]() { return Directory::Writable(path); });
}

Status WebDirValidator::Validate() const
{
	return Validate(m_options.GetWebDirPath())
		.And(
			[&]()
			{
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
	return RequiredDir(Options::WEBDIR, path).And([&]() { return Directory::Readable(path); });
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
	return RequiredDir(Options::TEMPDIR, path).And([&]() { return Directory::Writable(path); });
}

Status ScriptDirValidator::Validate() const
{
	return Validate(m_options.GetScriptDirPath())
		.And(
			[&]()
			{
				return UniquePath(GetName(), m_options.GetScriptDirPath(),
								  {{Options::MAINDIR, m_options.GetMainDirPath()},
								   {Options::DESTDIR, m_options.GetDestDirPath()},
								   {Options::INTERDIR, m_options.GetInterDirPath()},
								   {Options::NZBDIR, m_options.GetNzbDirPath()},
								   {Options::QUEUEDIR, m_options.GetQueueDirPath()},
								   {Options::WEBDIR, m_options.GetWebDirPath()},
								   {Options::TEMPDIR, m_options.GetTempDirPath()}});
			});
}

Status ScriptDirValidator::Validate(const boost::filesystem::path& path)
{
	return RequiredDir(Options::SCRIPTDIR, path);
}

Status ConfigTemplateValidator::Validate() const
{
	return Validate(m_options.GetConfigTemplatePath())
		.And(
			[&]()
			{
				return UniquePath(GetName(), m_options.GetConfigTemplatePath(),
								  {{Options::MAINDIR, m_options.GetMainDirPath()},
								   {Options::DESTDIR, m_options.GetDestDirPath()},
								   {Options::INTERDIR, m_options.GetInterDirPath()},
								   {Options::NZBDIR, m_options.GetNzbDirPath()},
								   {Options::QUEUEDIR, m_options.GetQueueDirPath()},
								   {Options::WEBDIR, m_options.GetWebDirPath()},
								   {Options::TEMPDIR, m_options.GetTempDirPath()},
								   {Options::SCRIPTDIR, m_options.GetScriptDirPath()}});
			});
}

Status ConfigTemplateValidator::Validate(const boost::filesystem::path& path)
{
	return RequiredFile(Options::CONFIGTEMPLATE, path);
}

Status LogFileValidator::Validate() const
{
	return Validate(m_options.GetLogFilePath(), m_options.GetWriteLog())
		.And(
			[&]()
			{
				return UniquePath(GetName(), m_options.GetLogFilePath(),
								  {{Options::MAINDIR, m_options.GetMainDirPath()},
								   {Options::DESTDIR, m_options.GetDestDirPath()},
								   {Options::INTERDIR, m_options.GetInterDirPath()},
								   {Options::NZBDIR, m_options.GetNzbDirPath()},
								   {Options::QUEUEDIR, m_options.GetQueueDirPath()},
								   {Options::WEBDIR, m_options.GetWebDirPath()},
								   {Options::TEMPDIR, m_options.GetTempDirPath()},
								   {Options::SCRIPTDIR, m_options.GetScriptDirPath()},
								   {Options::CONFIGTEMPLATE, m_options.GetConfigTemplatePath()}});
			});
}

Status LogFileValidator::Validate(const boost::filesystem::path& path, Options::EWriteLog writeLog)
{
	if (path.empty() && writeLog != Options::EWriteLog::wlNone)
	{
		return Status::Error("Logging is enabled, but " + std::string(Options::LOGFILE) +
							 " is set to empty.");
	}
	if (path.empty() && writeLog == Options::EWriteLog::wlNone)
	{
		return Status::Info(
			"Logging is disabled. Logging is recommended for "
			"effective debugging and troubleshooting.");
	}

	return RequiredFile(Options::LOGFILE, path);
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
								   {Options::SCRIPTDIR, m_options.GetScriptDirPath()},
								   {Options::CONFIGTEMPLATE, m_options.GetConfigTemplatePath()},
								   {Options::LOGFILE, m_options.GetLogFilePath()}});
			});
}

Status CertStoreValidator::Validate(const boost::filesystem::path& path, bool certCheck)
{
	if (path.empty() && !certCheck) return Status::Ok();

	if (path.empty() && certCheck)
		return Status::Error(std::string(Options::CERTCHECK) +
							 " requires proper configuration of option " +
							 std::string(Options::CERTSTORE) + ".");

	const auto file = File::Exists(path);
	if (!file.IsError()) return File::Readable(path);

	const auto dir = Directory::Exists(path);
	if (!dir.IsError()) return Directory::Readable(path);

	return Status::Error(std::string(Options::CERTSTORE) + " must be a file or a directory.");
}

Status RequiredDirValidator::Validate() const
{
	std::string_view req = m_options.GetRequiredDir();
	if (req.empty()) return Status::Ok();

	// Split list of required dirs and ensure they reference only DestDir/InterDir
	// (global or per-category) and that the entries exist.
	const auto tokens = Util::SplitStr(req.data(), ",;");
	const char* globalDest = m_options.GetDestDir();
	const char* globalInter = m_options.GetInterDir();
	const auto* cats = m_options.GetCategories();

	for (const auto& t : tokens)
	{
		std::string token = *t;
		if (token.empty())
			return Status::Error(std::string(Options::REQUIREDDIR) + " contains an empty entry.");

		bool allowed = false;
		if (!token.empty() && globalDest && token == globalDest) allowed = true;
		if (!token.empty() && globalInter && token == globalInter) allowed = true;

		if (!allowed && cats)
		{
			for (const auto& cat : *cats)
			{
				if (token == cat.GetDestDir())
				{
					allowed = true;
					break;
				}
			}
		}

		if (!allowed)
		{
			return Status::Error(
				std::string(Options::REQUIREDDIR) + " may only include directories used by " +
				std::string(Options::DESTDIR) + " or " + std::string(Options::INTERDIR) +
				" (global or per-category). Invalid entry: '" + token + "'.");
		}

		// If allowed, ensure the path exists (file or directory)
		const auto path = boost::filesystem::path(token);
		const auto fileStatus = File::Exists(path);
		if (fileStatus.IsOk()) continue;
		const auto dirStatus = Directory::Exists(path);
		if (dirStatus.IsOk()) continue;

		return Status::Warning(std::string(Options::REQUIREDDIR) + " contains missing entry '" +
							   token + "'.");
	}

	return Status::Ok();
}

#ifndef _WIN32
Status LockFileValidator::Validate() const
{
	return Validate(m_options.GetLockFilePath(), m_options.GetDaemonMode())
		.And(
			[&]()
			{
				return UniquePath(GetName(), m_options.GetLockFilePath(),
								  {{Options::MAINDIR, m_options.GetMainDirPath()},
								   {Options::DESTDIR, m_options.GetDestDirPath()},
								   {Options::INTERDIR, m_options.GetInterDirPath()},
								   {Options::NZBDIR, m_options.GetNzbDirPath()},
								   {Options::QUEUEDIR, m_options.GetQueueDirPath()},
								   {Options::WEBDIR, m_options.GetWebDirPath()},
								   {Options::TEMPDIR, m_options.GetTempDirPath()},
								   {Options::SCRIPTDIR, m_options.GetScriptDirPath()},
								   {Options::CONFIGTEMPLATE, m_options.GetConfigTemplatePath()},
								   {Options::LOGFILE, m_options.GetLogFilePath()},
								   {Options::CERTSTORE, m_options.GetCertStorePath()}});
			});
}

Status LockFileValidator::Validate(const boost::filesystem::path& path, bool daemonMode)
{
	if (path.empty() && daemonMode)
	{
		return Status::Warning(
			std::string(Options::LOCKFILE) +
			" value is empty. The check for another running instance is disabled.");
	}

	return Status::Ok();
}
#endif
}  // namespace SystemHealth::Paths
