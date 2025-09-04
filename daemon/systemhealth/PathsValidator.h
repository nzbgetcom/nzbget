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

#ifndef PATHS_VALIDATOR_H
#define PATHS_VALIDATOR_H

#include "SectionValidator.h"
#include "Options.h"

namespace SystemHealth::Paths
{

class PathsValidator final : public SectionValidator
{
public:
	explicit PathsValidator(const Options& options);
	std::string_view GetName() const override { return "Paths"; }

private:
	const Options& m_options;
};

class MainDirValidator final : public Validator
{
public:
	explicit MainDirValidator(const Options& options) : m_options(options) {}

	std::string_view GetName() const override { return Options::MAINDIR; }
	Status Validate() const override;
	static Status Validate(const boost::filesystem::path& path);

private:
	const Options& m_options;
};

class DestDirValidator final : public Validator
{
public:
	explicit DestDirValidator(const Options& options) : m_options(options) {}

	std::string_view GetName() const override { return Options::DESTDIR; }
	Status Validate() const override;
	static Status Validate(const boost::filesystem::path& path);

private:
	const Options& m_options;
};

class InterDirValidator final : public Validator
{
public:
	explicit InterDirValidator(const Options& options) : m_options(options) {}

	std::string_view GetName() const override { return Options::INTERDIR; }
	Status Validate() const override;
	static Status Validate(const boost::filesystem::path& path);

private:
	const Options& m_options;
};

class NzbDirValidator final : public Validator
{
public:
	explicit NzbDirValidator(const Options& options) : m_options(options) {}

	std::string_view GetName() const override { return Options::NZBDIR; }
	Status Validate() const override;
	static Status Validate(const boost::filesystem::path& path);

private:
	const Options& m_options;
};

class QueueDirValidator final : public Validator
{
public:
	explicit QueueDirValidator(const Options& options) : m_options(options) {}

	std::string_view GetName() const override { return Options::QUEUEDIR; }
	Status Validate() const override;
	static Status Validate(const boost::filesystem::path& path);

private:
	const Options& m_options;
};

class WebDirValidator final : public Validator
{
public:
	explicit WebDirValidator(const Options& options) : m_options(options) {}

	std::string_view GetName() const override { return Options::WEBDIR; }
	Status Validate() const override;
	static Status Validate(const boost::filesystem::path& path);

private:
	const Options& m_options;
};

class TempDirValidator final : public Validator
{
public:
	explicit TempDirValidator(const Options& options) : m_options(options) {}

	std::string_view GetName() const override { return Options::TEMPDIR; }
	Status Validate() const override;
	static Status Validate(const boost::filesystem::path& path);

private:
	const Options& m_options;
};

class ScriptDirValidator final : public Validator
{
public:
	explicit ScriptDirValidator(const Options& options) : m_options(options) {}

	std::string_view GetName() const override { return Options::SCRIPTDIR; }
	Status Validate() const override;
	static Status Validate(const boost::filesystem::path& path);

private:
	const Options& m_options;
};

class ConfigTemplateValidator final : public Validator
{
public:
	explicit ConfigTemplateValidator(const Options& options) : m_options(options) {}

	std::string_view GetName() const override { return Options::CONFIGTEMPLATE; }
	Status Validate() const override;
	static Status Validate(const boost::filesystem::path& path);

private:
	const Options& m_options;
};

class LogFileValidator final : public Validator
{
public:
	explicit LogFileValidator(const Options& options) : m_options(options) {}

	std::string_view GetName() const override { return Options::LOGFILE; }
	Status Validate() const override;
	static Status Validate(const boost::filesystem::path& path, Options::EWriteLog writeLog);

private:
	const Options& m_options;
};

class CertStoreValidator final : public Validator
{
public:
	explicit CertStoreValidator(const Options& options) : m_options(options) {}

	std::string_view GetName() const override { return Options::CERTSTORE; }
	Status Validate() const override;
	static Status Validate(const boost::filesystem::path& path, bool certCheck);

private:
	const Options& m_options;
};

class RequiredDirValidator final : public Validator
{
public:
	explicit RequiredDirValidator(const Options& options) : m_options(options) {}

	std::string_view GetName() const override { return Options::REQUIREDDIR; }
	Status Validate() const override;

private:
	const Options& m_options;
};
#ifndef _WIN32
class LockFileValidator final : public Validator
{
public:
	explicit LockFileValidator(const Options& options) : m_options(options) {}

	std::string_view GetName() const override { return Options::LOCKFILE; }
	Status Validate() const override;
	static Status Validate(const boost::filesystem::path& path, bool daemonMode);

private:
	const Options& m_options;
};
#endif
}  // namespace SystemHealth::Paths

#endif
