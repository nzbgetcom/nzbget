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

#ifndef FEEDS_VALIDATOR_H
#define FEEDS_VALIDATOR_H

#include <vector>
#include "FeedInfo.h"
#include "Options.h"
#include "SectionGroupValidator.h"

namespace SystemHealth::Feeds
{

class FeedsValidator final : public SectionGroupValidator
{
public:
	explicit FeedsValidator(const ::Feeds& feeds, const Options& options);
	std::string_view GetName() const override { return "Feeds"; }

private:
	const ::Feeds& m_feeds;
	const Options& m_options;
	std::vector<std::unique_ptr<SectionValidator>> MakeFeedValidators(const ::Feeds& feeds) const;
};

}  // namespace SystemHealth::Feeds

#endif
