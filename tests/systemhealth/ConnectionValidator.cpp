/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Denis <denis@nzbget.com>
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

#include <boost/test/unit_test.hpp>
#include "Options.h"
#include "ConnectionValidator.h"

BOOST_AUTO_TEST_SUITE(SystemHealthTest)

namespace
{

SystemHealth::Status RetriesStatus(std::vector<const char*> settings)
{
	Options::CmdOptList cmdOpts;
	for (const char* setting : settings)
	{
		cmdOpts.push_back(setting);
	}
	Options options(&cmdOpts, nullptr);
	return SystemHealth::Connection::ArticleRetriesValidator(options).Validate();
}

}

BOOST_AUTO_TEST_CASE(ArticleRetriesWithDupeDecisions)
{
	BOOST_CHECK(RetriesStatus({ "ArticleRetries=3" }).IsOk());

	// without health decisions, a low value is the usual warning
	SystemHealth::Status plain = RetriesStatus({ "ArticleRetries=0", "HealthCheck=park" });
	BOOST_CHECK(plain.IsWarning());
	BOOST_CHECK(plain.GetMessage().find("HealthCheck") == std::string::npos);

	// HealthCheck=dupe and DupeSearch decide by article results: the warning says so
	SystemHealth::Status dupe = RetriesStatus({ "ArticleRetries=0", "HealthCheck=dupe" });
	BOOST_CHECK(dupe.IsWarning());
	BOOST_CHECK(dupe.GetMessage().find("HealthCheck=dupe") != std::string::npos);

	SystemHealth::Status search = RetriesStatus({ "ArticleRetries=1", "DupeSearch=yes",
		"DupeSearchUrl=http://127.0.0.1:1/api" });
	BOOST_CHECK(search.IsWarning());
	BOOST_CHECK(search.GetMessage().find("DupeSearch") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
