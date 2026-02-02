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

#ifndef CONNECTION_VALIDATOR_H
#define CONNECTION_VALIDATOR_H

#include "SectionValidator.h"
#include "Options.h"

namespace SystemHealth::Connection
{
class ConnectionValidator final : public SectionValidator
{
public:
	explicit ConnectionValidator(const Options& options);
	std::string_view GetName() const override { return "Connection"; }

private:
	const Options& m_options;
};

class ArticleRetriesValidator : public Validator
{
public:
	explicit ArticleRetriesValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::ARTICLERETRIES; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ArticleIntervalValidator : public Validator
{
public:
	explicit ArticleIntervalValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::ARTICLEINTERVAL; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ArticleTimeoutValidator : public Validator
{
public:
	explicit ArticleTimeoutValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::ARTICLETIMEOUT; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ArticleReadChunkSizeValidator : public Validator
{
public:
	explicit ArticleReadChunkSizeValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::ARTICLEREADCHUNKSIZE; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class UrlRetriesValidator : public Validator
{
public:
	explicit UrlRetriesValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::URLRETRIES; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class UrlIntervalValidator : public Validator
{
public:
	explicit UrlIntervalValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::URLINTERVAL; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class UrlTimeoutValidator : public Validator
{
public:
	explicit UrlTimeoutValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::URLTIMEOUT; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class RemoteTimeoutValidator : public Validator
{
public:
	explicit RemoteTimeoutValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::REMOTETIMEOUT; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class DownloadRateValidator : public Validator
{
public:
	explicit DownloadRateValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::DOWNLOADRATE; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class UrlConnectionsValidator : public Validator
{
public:
	explicit UrlConnectionsValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::URLCONNECTIONS; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class UrlForceValidator : public Validator
{
public:
	explicit UrlForceValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::URLFORCE; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class MonthlyQuotaValidator : public Validator
{
public:
	explicit MonthlyQuotaValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::MONTHLYQUOTA; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class QuotaStartDayValidator : public Validator
{
public:
	explicit QuotaStartDayValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::QUOTASTARTDAY; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class DailyQuotaValidator : public Validator
{
public:
	explicit DailyQuotaValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::DAILYQUOTA; }
	Status Validate() const override;

private:
	const Options& m_options;
};

}  // namespace SystemHealth::Connection

#endif
