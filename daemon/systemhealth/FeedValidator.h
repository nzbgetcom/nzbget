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

#ifndef FEED_VALIDATOR_H
#define FEED_VALIDATOR_H

#include "SectionValidator.h"
#include "FeedInfo.h"
#include "Validators.h"

namespace SystemHealth::Feeds
{

class FeedValidator final : public SectionValidator
{
public:
	FeedValidator(const FeedInfo& feed);
	std::string_view GetName() const override { return m_name; }

private:
	const FeedInfo& m_feed;
	const std::string m_name;
};

class FeedNameValidator final : public Validator
{
public:
	explicit FeedNameValidator(const FeedInfo& feed) : m_feed(feed) {}
	std::string_view GetName() const override { return "Name"; }
	Status Validate() const override;

private:
	const FeedInfo& m_feed;
};

class FeedUrlValidator final : public Validator
{
public:
	explicit FeedUrlValidator(const FeedInfo& feed) : m_feed(feed) {}
	std::string_view GetName() const override { return "URL"; }
	Status Validate() const override;

private:
	const FeedInfo& m_feed;
};

class FeedIntervalValidator final : public Validator
{
public:
	explicit FeedIntervalValidator(const FeedInfo& feed) : m_feed(feed) {}
	std::string_view GetName() const override { return "Interval"; }
	Status Validate() const override;

private:
	const FeedInfo& m_feed;
};

class FeedFilterValidator final : public Validator
{
public:
	explicit FeedFilterValidator(const FeedInfo& feed) : m_feed(feed) {}
	std::string_view GetName() const override { return "Filter"; }
	Status Validate() const override;

private:
	const FeedInfo& m_feed;
};

class FeedScriptsValidator final : public Validator
{
public:
	FeedScriptsValidator(const FeedInfo& feed) : m_feed(feed) {}

	std::string_view GetName() const override { return "Extensions"; }
	Status Validate() const override;

private:
	const FeedInfo& m_feed;
};

class FeedCategoryValidator final : public Validator
{
public:
	FeedCategoryValidator(const FeedInfo& feed) : m_feed(feed) {}

	std::string_view GetName() const override { return "Category"; }
	Status Validate() const override;

private:
	const FeedInfo& m_feed;
};

}  // namespace SystemHealth::Feeds

#endif
