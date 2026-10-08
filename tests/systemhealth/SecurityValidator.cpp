/*
 *  This file is part of nzbget. See <https://nzbget.com>.
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
#include "SecurityValidator.h"

BOOST_AUTO_TEST_SUITE(SystemHealthTest)

namespace
{
	struct SecurityOptions
	{
		explicit SecurityOptions(std::vector<std::string> opts) : m_opts(std::move(opts))
		{
			for (const std::string& opt : m_opts) m_cmdOpts.push_back(opt.c_str());
			m_options = std::make_unique<Options>(&m_cmdOpts, nullptr);
		}
		std::vector<std::string> m_opts;
		Options::CmdOptList m_cmdOpts;
		std::unique_ptr<Options> m_options;
	};
}

BOOST_AUTO_TEST_CASE(ControlPasswordCheckedWithoutUsername)
{
	// an empty username takes any name: the password still gates access
	SecurityOptions empty({"ControlUsername=", "ControlPassword="});
	BOOST_CHECK(!SystemHealth::Security::ControlPasswordValidator(*empty.m_options).Validate().IsOk());

	SecurityOptions defaultPassword({"ControlUsername=", "ControlPassword=tegbzn6789"});
	BOOST_CHECK(!SystemHealth::Security::ControlPasswordValidator(*defaultPassword.m_options).Validate().IsOk());

	SecurityOptions good({"ControlUsername=", "ControlPassword=a-long-password"});
	BOOST_CHECK(SystemHealth::Security::ControlPasswordValidator(*good.m_options).Validate().IsOk());
}

BOOST_AUTO_TEST_CASE(FormAuthWithControlUserOnly)
{
	// the control user logs in by form: no warning without add or restricted users
	SecurityOptions control({"FormAuth=yes", "SecureControl=yes", "ControlUsername=admin",
		"ControlPassword=a-long-password", "AddUsername=", "RestrictedUsername="});
	BOOST_CHECK(SystemHealth::Security::FormAuthValidator(*control.m_options).Validate().IsOk());

	SecurityOptions none({"FormAuth=yes", "SecureControl=yes", "ControlUsername=",
		"ControlPassword=", "AddUsername=", "RestrictedUsername="});
	BOOST_CHECK(SystemHealth::Security::FormAuthValidator(*none.m_options).Validate().IsWarning());
}

BOOST_AUTO_TEST_SUITE_END()
