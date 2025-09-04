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

#include "ConnectionValidator.h"
#include "Validators.h"

namespace SystemHealth::Connection
{
ConnectionValidator::ConnectionValidator(const Options& options) : m_options(options)
{
	m_validators.reserve(14);
	m_validators.push_back(std::make_unique<ArticleRetriesValidator>(options));
	m_validators.push_back(std::make_unique<ArticleIntervalValidator>(options));
	m_validators.push_back(std::make_unique<ArticleTimeoutValidator>(options));
	m_validators.push_back(std::make_unique<ArticleReadChunkSizeValidator>(options));
	m_validators.push_back(std::make_unique<UrlRetriesValidator>(options));
	m_validators.push_back(std::make_unique<UrlIntervalValidator>(options));
	m_validators.push_back(std::make_unique<UrlTimeoutValidator>(options));
	m_validators.push_back(std::make_unique<UrlConnectionsValidator>(options));
	m_validators.push_back(std::make_unique<UrlForceValidator>(options));
	m_validators.push_back(std::make_unique<RemoteTimeoutValidator>(options));
	m_validators.push_back(std::make_unique<DownloadRateValidator>(options));
	m_validators.push_back(std::make_unique<MonthlyQuotaValidator>(options));
	m_validators.push_back(std::make_unique<QuotaStartDayValidator>(options));
	m_validators.push_back(std::make_unique<DailyQuotaValidator>(options));
}

Status ArticleRetriesValidator::Validate() const
{
	int val = m_options.GetArticleRetries();
	if (val < 0 || val > 99)
	{
		return Status::Error(std::string(Options::ARTICLERETRIES) + " must be between 0 and 99.");
	}
	return Status::Ok();
}
Status ArticleIntervalValidator::Validate() const
{
	return CheckPositiveNum(Options::ARTICLEINTERVAL, m_options.GetArticleInterval());
}

Status ArticleTimeoutValidator::Validate() const
{
	int val = m_options.GetArticleTimeout();
	Status s = CheckPositiveNum(Options::ARTICLETIMEOUT, val);
	if (!s.IsOk()) return s;

	if (val < 5)
	{
		return Status::Warning(
			std::string(Options::ARTICLETIMEOUT) +
			" is very low (< 5s). This may cause downloads to fail on slower connections.");
	}
	return Status::Ok();
}

Status ArticleReadChunkSizeValidator::Validate() const
{
	int val = m_options.GetArticleReadChunkSize();
	if (val <= 0)
	{
		return Status::Error(std::string(Options::ARTICLEREADCHUNKSIZE) +
							 " must be greater than 0.");
	}
	// Very small chunks might be inefficient
	if (val < 4)
	{
		// Just a thought, not necessarily an error, but <4KB is tiny for modern pipes.
		// Leaving as OK for now unless you want strict enforcement.
	}
	return Status::Ok();
}

Status UrlRetriesValidator::Validate() const
{
	int val = m_options.GetUrlRetries();
	if (val < 0 || val > 99)
	{
		return Status::Error(std::string(Options::URLRETRIES) + " must be between 0 and 99.");
	}
	return Status::Ok();
}

Status UrlIntervalValidator::Validate() const
{
	return CheckPositiveNum(Options::URLINTERVAL, m_options.GetUrlInterval());
}

Status UrlTimeoutValidator::Validate() const
{
	int val = m_options.GetUrlTimeout();
	Status s = CheckPositiveNum(Options::URLTIMEOUT, val);
	if (!s.IsOk()) return s;

	if (val < 5)
	{
		return Status::Warning(std::string(Options::URLTIMEOUT) +
							   " is very low (< 5s). RSS/URL fetches might fail.");
	}
	return Status::Ok();
}

Status RemoteTimeoutValidator::Validate() const
{
	return CheckPositiveNum(Options::REMOTETIMEOUT, m_options.GetRemoteTimeout());
}

Status DownloadRateValidator::Validate() const
{
	// 0 is valid (unlimited)
	if (m_options.GetDownloadRate() < 0)
	{
		return Status::Error(std::string(Options::DOWNLOADRATE) + " cannot be negative.");
	}
	return Status::Ok();
}

Status UrlConnectionsValidator::Validate() const
{
	int val = m_options.GetUrlConnections();
	if (val < 0 || val > 999)
	{
		return Status::Error(std::string(Options::URLCONNECTIONS) + " must be between 0 and 999.");
	}
	return Status::Ok();
}

Status UrlForceValidator::Validate() const
{
	if (m_options.GetUrlForce() && m_options.GetUrlConnections() <= 0)
	{
		return Status::Warning(std::string(Options::URLFORCE) + " is enabled but " +
							   std::string(Options::URLCONNECTIONS) +
							   " is 0; forced URLs may not be fetched.");
	}
	return Status::Ok();
}

Status MonthlyQuotaValidator::Validate() const
{
	if (m_options.GetMonthlyQuota() < 0)
	{
		return Status::Error(std::string(Options::MONTHLYQUOTA) + " cannot be negative.");
	}
	return Status::Ok();
}

Status QuotaStartDayValidator::Validate() const
{
	int day = m_options.GetQuotaStartDay();
	// Config docs say 1-31
	if (day < 1 || day > 31)
	{
		return Status::Error(std::string(Options::QUOTASTARTDAY) + " must be between 1 and 31.");
	}
	return Status::Ok();
}

Status DailyQuotaValidator::Validate() const
{
	if (m_options.GetDailyQuota() < 0)
	{
		return Status::Error(std::string(Options::DAILYQUOTA) + " cannot be negative.");
	}
	return Status::Ok();
}

}  // namespace SystemHealth::Connection
