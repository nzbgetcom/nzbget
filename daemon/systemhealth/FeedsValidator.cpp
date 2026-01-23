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

#include "FeedsValidator.h"
#include "FeedValidator.h"

namespace SystemHealth::Feeds
{
FeedsValidator::FeedsValidator(const ::Feeds& feeds, const Options& options)
	: SectionGroupValidator(MakeFeedValidators(feeds)), m_feeds(feeds), m_options(options)
{
	m_validators.reserve(1);
	m_validators.push_back(std::make_unique<DuplicateFeedsValidator>(feeds));
}
std::vector<std::unique_ptr<SectionValidator>> FeedsValidator::MakeFeedValidators(
	const ::Feeds& feeds) const
{
	std::vector<std::unique_ptr<SectionValidator>> validators;
	validators.reserve(feeds.size());
	for (const auto& feed : feeds)
	{
		validators.push_back(std::make_unique<FeedValidator>(*feed, m_options));
	}
	return validators;
}

Status DuplicateFeedsValidator::Validate() const
{
	if (m_feeds.empty()) return Status::Ok();

	std::map<std::string_view, std::string_view> seenUrls;

	for (const auto& feed : m_feeds)
	{
		std::string_view url = feed->GetUrl();
		if (url.empty()) continue;

		auto it = seenUrls.find(url);
		if (it != seenUrls.end())
		{
			std::string_view firstFeedName = it->second;
			std::string_view currentFeedName = feed->GetName();

			return Status::Warning("Feeds '" + std::string(firstFeedName) + "' and '" +
								   std::string(currentFeedName) + "' have the same URL");
		}

		seenUrls.emplace(url, feed->GetName());
	}

	return Status::Ok();
}
}  // namespace SystemHealth::Feeds
