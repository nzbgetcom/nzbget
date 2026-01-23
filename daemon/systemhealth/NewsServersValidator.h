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

#ifndef NEWS_SERVERS_VALIDATOR_H
#define NEWS_SERVERS_VALIDATOR_H

#include "Options.h"
#include "NewsServerValidator.h"
#include "Connection.h"
#include "SectionGroupValidator.h"
#include "SectionValidator.h"
#include "Validators.h"

namespace SystemHealth::NewsServers
{

class NewsServersValidator final : public SectionGroupValidator
{
public:
	explicit NewsServersValidator(const Servers& servers);
	std::string_view GetName() const override { return "NewsServers"; }

private:
	const Servers& m_servers;
	std::vector<std::unique_ptr<SectionValidator>> MakeServerValidators(
		const Servers& servers) const;
};

class ServersConfiguredValidator final : public Validator
{
public:
	explicit ServersConfiguredValidator(const Servers& servers) : m_servers(servers) {}

	std::string_view GetName() const override { return ""; }
	Status Validate() const override;

private:
	const Servers& m_servers;
	static constexpr std::string_view DEFAULT_SERVER_HOST = "my.newsserver.com";
};

class AnyServerActiveValidator final : public Validator
{
public:
	explicit AnyServerActiveValidator(const Servers& servers) : m_servers(servers) {}

	std::string_view GetName() const override { return ""; }
	Status Validate() const override;

private:
	const Servers& m_servers;
};

class AnyPrimaryServerExistsValidator final : public Validator
{
public:
	explicit AnyPrimaryServerExistsValidator(const Servers& servers) : m_servers(servers) {}

	std::string_view GetName() const override { return ""; }
	Status Validate() const override;

private:
	const Servers& m_servers;
};

}  // namespace SystemHealth::NewsServers

#endif
