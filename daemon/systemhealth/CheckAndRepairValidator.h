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

#ifndef CHECK_AND_REPAIR_VALIDATOR_H
#define CHECK_AND_REPAIR_VALIDATOR_H

#include "SectionValidator.h"
#include "Options.h"

namespace SystemHealth::CheckAndRepair
{
class CheckAndRepairValidator final : public SectionValidator
{
public:
	explicit CheckAndRepairValidator(const Options& options);
	std::string_view GetName() const override { return "CheckAndRepair"; }

private:
	const Options& m_options;
};

// Individual validators
class CrcCheckValidator : public Validator
{
public:
	explicit CrcCheckValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::CRCCHECK; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ParCheckValidator : public Validator
{
public:
	explicit ParCheckValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::PARCHECK; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ParRepairValidator : public Validator
{
public:
	explicit ParRepairValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::PARREPAIR; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ParScanValidator : public Validator
{
public:
	explicit ParScanValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::PARSCAN; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ParQuickValidator : public Validator
{
public:
	explicit ParQuickValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::PARQUICK; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ParBufferValidator : public Validator
{
public:
	explicit ParBufferValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::PARBUFFER; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ParThreadsValidator : public Validator
{
public:
	explicit ParThreadsValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::PARTHREADS; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ParIgnoreExtValidator : public Validator
{
public:
	explicit ParIgnoreExtValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::PARIGNOREEXT; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ParTimeLimitValidator : public Validator
{
public:
	explicit ParTimeLimitValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::PARTIMELIMIT; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ParRenameValidator : public Validator
{
public:
	explicit ParRenameValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::PARRENAME; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class RarRenameValidator : public Validator
{
public:
	explicit RarRenameValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::RARRENAME; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class DirectRenameValidator : public Validator
{
public:
	explicit DirectRenameValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::DIRECTRENAME; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class HardLinkingValidator : public Validator
{
public:
	explicit HardLinkingValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::HARDLINKING; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class HardLinkingIgnoreExtValidator : public Validator
{
public:
	explicit HardLinkingIgnoreExtValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::HARDLINKINGIGNOREEXT; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class HealthCheckValidator : public Validator
{
public:
	explicit HealthCheckValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::HEALTHCHECK; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ParPauseQueueValidator : public Validator
{
public:
	explicit ParPauseQueueValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::PARPAUSEQUEUE; }
	Status Validate() const override;

private:
	const Options& m_options;
};

}  // namespace SystemHealth::CheckAndRepair

#endif
