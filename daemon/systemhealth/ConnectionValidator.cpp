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
		return Status::Error("'" + std::string(Options::ARTICLERETRIES) +
							 "' value must be between 0 and 99");

	if (val < 3)
		return Status::Warning(
			"Value is very low. Transient network errors may cause download failures");

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

	if (val < 30) return Status::Warning("Value is very low. Valid connections may be dropped");

	return Status::Ok();
}

Status ArticleReadChunkSizeValidator::Validate() const
{
	int val = m_options.GetArticleReadChunkSize();
	Status s = CheckPositiveNum(Options::ARTICLEREADCHUNKSIZE, val);
	if (!s.IsOk()) return s;

	if (val < 4)
		return Status::Warning(
			"Value is very low. This may reduce download speed due to network protocol overhead.");

	if (val > 10240)  // 10 MB
		return Status::Info("Value is very high. Ensure you have enough RAM");

	return Status::Ok();
}

Status UrlRetriesValidator::Validate() const
{
	int val = m_options.GetUrlRetries();
	if (val < 0 || val > 99)
		return Status::Error(std::string(Options::URLRETRIES) + " must be between 0 and 99");

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

	if (val < 5) return Status::Warning("Value is very low. RSS/URL fetches might fail");

	return Status::Ok();
}

Status RemoteTimeoutValidator::Validate() const
{
	return CheckPositiveNum(Options::REMOTETIMEOUT, m_options.GetRemoteTimeout());
}

Status DownloadRateValidator::Validate() const
{
	int val = m_options.GetDownloadRate();
	Status s = CheckPositiveNum(Options::DOWNLOADRATE, val);
	if (!s.IsOk()) return s;

	if (val == 0) return Status::Ok();

	return Status::Warning("Download speed is limited to " + std::to_string(val) + " KB/s");
}

Status UrlConnectionsValidator::Validate() const
{
	int val = m_options.GetUrlConnections();
	if (val < 0 || val > 999)
	{
		return Status::Error("'" + std::string(Options::URLCONNECTIONS) +
							 "' must be between 0 and 999");
	}
	return Status::Ok();
}

Status UrlForceValidator::Validate() const { return Status::Ok(); }

Status MonthlyQuotaValidator::Validate() const
{
	int val = m_options.GetMonthlyQuota();
	Status s = CheckPositiveNum(Options::MONTHLYQUOTA, val);
	if (!s.IsOk()) return s;

	if (val == 0) return Status::Ok(); // no quota

	return Status::Info("'" + std::string(Options::MONTHLYQUOTA) + "' is active (" +
						std::to_string(val) +
						" MB). "
						"Downloads will pause if this limit is reached");
}

Status QuotaStartDayValidator::Validate() const
{
	int day = m_options.GetQuotaStartDay();
	if (day < 1 || day > 31)
		return Status::Error(std::string(Options::QUOTASTARTDAY) + " must be between 1 and 31");

	return Status::Ok();
}

Status DailyQuotaValidator::Validate() const
{
	int val = m_options.GetDailyQuota();
	Status s = CheckPositiveNum(Options::DAILYQUOTA, val);
	if (!s.IsOk()) return s;

	if (val == 0) return Status::Ok();

	return Status::Info("'" + std::string(Options::DAILYQUOTA) + "' is active (" +
						std::to_string(val) +
						" MB). "
						"Downloads will pause if this limit is reached");
}

}  // namespace SystemHealth::Connection
