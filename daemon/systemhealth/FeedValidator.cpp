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

#include "FeedValidator.h"
#include "FeedInfo.h"
#include "Options.h"

namespace SystemHealth::Feeds
{

FeedValidator::FeedValidator(const FeedInfo& feed, const Options& options)
	: m_feed(feed), m_options(options), m_name("Feed" + std::to_string(feed.GetId()))
{
	m_validators.reserve(6);
	m_validators.push_back(std::make_unique<NameValidator>(feed));
	m_validators.push_back(std::make_unique<UrlValidator>(feed));
	m_validators.push_back(std::make_unique<IntervalValidator>(feed));
	m_validators.push_back(std::make_unique<FilterValidator>(feed));
	m_validators.push_back(std::make_unique<ScriptsValidator>(feed));
	m_validators.push_back(std::make_unique<CategoryValidator>(feed, options));
}

Status NameValidator::Validate() const
{
	std::string_view name = m_feed.GetName();
	if (name.empty())
	{
		return Status::Info("Name is recommended for clearer logs and troubleshooting");
	}
	return Status::Ok();
}

Status UrlValidator::Validate() const
{
	std::string_view url = m_feed.GetUrl();
	if (url.empty())
	{
		return Status::Warning("URL is required");
	}

	if (url.find("http://") == std::string_view::npos &&
		url.find("https://") == std::string_view::npos)
	{
		return Status::Warning("URL does not start with 'http://' or 'https://'");
	}

	return Status::Ok();
}

Status IntervalValidator::Validate() const
{
	int interval = m_feed.GetInterval();
	const auto check = CheckPositiveNum("Interval", interval);
	if (!check.IsOk()) return check;

	if (interval == 0)
	{
		return Status::Info("Automatic check is disabled");
	}

	return Status::Ok();
}

Status FilterValidator::Validate() const { return Status::Ok(); }

Status ScriptsValidator::Validate() const { return Status::Ok(); }

Status CategoryValidator::Validate() const
{
	return Status::Ok();
}

}  // namespace SystemHealth::Feeds
