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
#include "Options.h"

namespace SystemHealth::Feeds
{

class FeedValidator final : public SectionValidator
{
public:
	FeedValidator(const FeedInfo& feed, const Options& options);
	std::string_view GetName() const override { return m_name; }

private:
	const FeedInfo& m_feed;
	const Options& m_options;
	const std::string m_name;
};

class NameValidator final : public Validator
{
public:
	explicit NameValidator(const FeedInfo& feed) : m_feed(feed) {}
	std::string_view GetName() const override { return "Name"; }
	Status Validate() const override;

private:
	const FeedInfo& m_feed;
};

class UrlValidator final : public Validator
{
public:
	explicit UrlValidator(const FeedInfo& feed) : m_feed(feed) {}
	std::string_view GetName() const override { return "URL"; }
	Status Validate() const override;

private:
	const FeedInfo& m_feed;
};

class IntervalValidator final : public Validator
{
public:
	explicit IntervalValidator(const FeedInfo& feed) : m_feed(feed) {}
	std::string_view GetName() const override { return "Interval"; }
	Status Validate() const override;

private:
	const FeedInfo& m_feed;
};

class FilterValidator final : public Validator
{
public:
	explicit FilterValidator(const FeedInfo& feed) : m_feed(feed) {}
	std::string_view GetName() const override { return "Filter"; }
	Status Validate() const override;

private:
	const FeedInfo& m_feed;
};

class ScriptsValidator final : public Validator
{
public:
	ScriptsValidator(const FeedInfo& feed) : m_feed(feed) {}

	std::string_view GetName() const override { return "Extensions"; }
	Status Validate() const override;

private:
	const FeedInfo& m_feed;
};

class CategoryValidator final : public Validator
{
public:
	CategoryValidator(const FeedInfo& feed, const Options& options)
		: m_feed(feed), m_options(options)
	{
	}

	std::string_view GetName() const override { return "Category"; }
	Status Validate() const override;

private:
	const FeedInfo& m_feed;
	const Options& m_options;
};

}  // namespace SystemHealth::Feeds

#endif
