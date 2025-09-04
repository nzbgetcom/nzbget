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
#include <string>
#include <sstream>

namespace SystemHealth::Feeds
{

FeedValidator::FeedValidator(const FeedInfo& feed)
	: m_feed(feed), m_name("Feed" + std::to_string(feed.GetId()))
{
	m_validators.reserve(6);
	m_validators.push_back(std::make_unique<FeedNameValidator>(feed));
	m_validators.push_back(std::make_unique<FeedUrlValidator>(feed));
	m_validators.push_back(std::make_unique<FeedIntervalValidator>(feed));
	m_validators.push_back(std::make_unique<FeedFilterValidator>(feed));
	m_validators.push_back(std::make_unique<FeedScriptsValidator>(feed));
	m_validators.push_back(std::make_unique<FeedCategoryValidator>(feed));
}

Status FeedNameValidator::Validate() const
{
	std::string_view name = m_feed.GetName();
	if (name.empty())
	{
		return Status::Error("Feed name is required.");
	}
	return Status::Ok();
}

Status FeedUrlValidator::Validate() const
{
	std::string_view url = m_feed.GetUrl();
	if (url.empty())
	{
		return Status::Warning("Feed URL is required.");
	}

	if (url.find("http://") == std::string_view::npos &&
		url.find("https://") == std::string_view::npos)
	{
		return Status::Warning("URL does not start with 'http://' or 'https://'.");
	}

	return Status::Ok();
}

Status FeedIntervalValidator::Validate() const
{
	int interval = m_feed.GetInterval();

	if (interval < 0)
	{
		return Status::Error("Interval cannot be negative.");
	}

	if (interval == 0)
	{
		return Status::Info("Automatic check is disabled (0).");
	}

	return Status::Ok();
}

Status FeedFilterValidator::Validate() const
{
	std::string_view filter = m_feed.GetFilter();
	if (filter.empty())
	{
		return Status::Info("No feed filter specified.");
	}
	if (filter.size() > 1024)
	{
		return Status::Warning("Feed filter is unusually long.");
	}
	return Status::Ok();
}

Status FeedScriptsValidator::Validate() const
{
	std::string_view ext = m_feed.GetExtensions();
	if (ext.empty()) return Status::Ok();

	// Basic sanity checks: token count and token length
	std::istringstream ss(ext.data());
	std::string token;
	int count = 0;
	while (std::getline(ss, token, ','))
	{
		++count;
		if (token.size() > 256)
			return Status::Warning("One of the feed extensions/script names is unusually long.");
	}
	if (count > 20) return Status::Warning("Too many feed extensions/scripts defined.");

	return Status::Ok();
}

Status FeedCategoryValidator::Validate() const
{
	const char* category = m_feed.GetCategory();
	if (!category || *category == '\0')
	{
		return Status::Info("No category assigned to this feed.");
	}

	if (g_Options)
	{
		if (!g_Options->FindCategory(category, true))
			return Status::Warning(std::string("Feed references unknown category: ") + category);
	}

	return Status::Ok();
}

}  // namespace SystemHealth::Feeds
