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

#ifndef UNPACK_VALIDATOR_H
#define UNPACK_VALIDATOR_H

#include "SectionValidator.h"
#include "Options.h"

namespace SystemHealth::Unpack
{
class UnpackValidator final : public SectionValidator
{
public:
	explicit UnpackValidator(const Options& options);
	std::string_view GetName() const override { return "Unpack"; }

private:
	const Options& m_options;
};

class UnpackEnabledValidator final : public Validator
{
public:
	explicit UnpackEnabledValidator(const Options& options) : m_options(&options) {}
	std::string_view GetName() const override { return Options::UNPACK; }
	Status Validate() const override;

private:
	const Options* m_options;
};

class DirectUnpackValidator final : public Validator
{
public:
	explicit DirectUnpackValidator(const Options& options) : m_options(&options) {}
	std::string_view GetName() const override { return Options::DIRECTUNPACK; }
	Status Validate() const override;

private:
	const Options* m_options;
};

class UseTempUnpackDirValidator final : public Validator
{
public:
	explicit UseTempUnpackDirValidator(const Options& options) : m_options(&options) {}
	std::string_view GetName() const override { return Options::USETEMPUNPACKDIR; }
	Status Validate() const override;

private:
	const Options* m_options;
};

class UnpackCleanupDiskValidator final : public Validator
{
public:
	explicit UnpackCleanupDiskValidator(const Options& options) : m_options(&options) {}
	std::string_view GetName() const override { return Options::UNPACKCLEANUPDISK; }
	Status Validate() const override;

private:
	const Options* m_options;
};

class UnpackPauseQueueValidator final : public Validator
{
public:
	explicit UnpackPauseQueueValidator(const Options& options) : m_options(&options) {}
	std::string_view GetName() const override { return Options::UNPACKPAUSEQUEUE; }
	Status Validate() const override;

private:
	const Options* m_options;
};

class UnrarCmdValidator final : public Validator
{
public:
	explicit UnrarCmdValidator(const Options& options) : m_options(&options) {}
	std::string_view GetName() const override { return Options::UNRARCMD; }
	Status Validate() const override;

private:
	const Options* m_options;
};

class SevenZipCmdValidator final : public Validator
{
public:
	explicit SevenZipCmdValidator(const Options& options) : m_options(&options) {}
	std::string_view GetName() const override { return Options::SEVENZIPCMD; }
	Status Validate() const override;

private:
	const Options* m_options;
};

class ExtensionCleanupValidator final : public Validator
{
public:
	explicit ExtensionCleanupValidator(const Options& options) : m_options(&options) {}
	std::string_view GetName() const override { return Options::EXTCLEANUPDISK; }
	Status Validate() const override;

private:
	const Options* m_options;
};

class UnpackPassFileValidator final : public Validator
{
public:
	explicit UnpackPassFileValidator(const Options& options) : m_options(&options) {}
	std::string_view GetName() const override { return Options::UNPACKPASSFILE; }
	Status Validate() const override;

private:
	const Options* m_options;
};

class UnpackIgnoreExtValidator final : public Validator
{
public:
	explicit UnpackIgnoreExtValidator(const Options& options) : m_options(&options) {}
	std::string_view GetName() const override { return Options::UNPACKIGNOREEXT; }
	Status Validate() const override;

private:
	const Options* m_options;
};

}  // namespace SystemHealth::Unpack

#endif
