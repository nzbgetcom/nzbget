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

#ifndef LOGGING_VALIDATOR_H
#define LOGGING_VALIDATOR_H

#include "SectionValidator.h"
#include "Options.h"

namespace SystemHealth::Logging
{

class LoggingValidator final : public SectionValidator
{
public:
	explicit LoggingValidator(const Options& options);
	std::string_view GetName() const override { return "Logging"; }

private:
	const Options& m_options;
};

class WriteLogValidator final : public Validator
{
public:
	explicit WriteLogValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::WRITELOG; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class RotateLogValidator final : public Validator
{
public:
	explicit RotateLogValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::ROTATELOG; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class LogBufferValidator final : public Validator
{
public:
	explicit LogBufferValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::LOGBUFFER; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class NzbLogValidator final : public Validator
{
public:
	explicit NzbLogValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::NZBLOG; }
	Status Validate() const override { return Status::Ok(); }

private:
	const Options& m_options;
};

class CrashTraceValidator final : public Validator
{
public:
	explicit CrashTraceValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::CRASHTRACE; }
	Status Validate() const override { return Status::Ok(); }

private:
	const Options& m_options;
};

class CrashDumpValidator final : public Validator
{
public:
	explicit CrashDumpValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::CRASHDUMP; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class TimeCorrectionValidator final : public Validator
{
public:
	explicit TimeCorrectionValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::TIMECORRECTION; }
	Status Validate() const override;

private:
	const Options& m_options;
};

}  // namespace SystemHealth::Logging

#endif
