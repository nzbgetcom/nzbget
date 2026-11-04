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
#include "ConnectionValidator.h"
#include "Options.h"

BOOST_AUTO_TEST_SUITE(SystemHealthTest)

using namespace SystemHealth;
using namespace SystemHealth::Connection;

BOOST_AUTO_TEST_SUITE(ConnectionValidatorsSuite)

static Status ValidateChunkSize(const char* value)
{
	const std::string option = std::string("ArticleReadChunkSize=") + value;
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back(option.c_str());
	Options options(&cmdOpts, nullptr);
	return ArticleReadChunkSizeValidator(options).Validate();
}

BOOST_AUTO_TEST_CASE(TestArticleReadChunkSizeValidator)
{
	// 0 selects the size automatically.
	BOOST_CHECK(ValidateChunkSize("0").IsOk());

	// The option is given in KB; thresholds must be compared in KB too.
	BOOST_CHECK(ValidateChunkSize("16").IsOk());
	BOOST_CHECK(ValidateChunkSize("256").IsOk());
	BOOST_CHECK(ValidateChunkSize("10240").IsOk());

	Status small = ValidateChunkSize("4");
	BOOST_CHECK(small.IsWarning());
	BOOST_CHECK(small.GetMessage().find("4 KB") != std::string::npos);

	Status large = ValidateChunkSize("20480");
	BOOST_CHECK(large.IsInfo());
	BOOST_CHECK(large.GetMessage().find("20480 KB") != std::string::npos);

	BOOST_CHECK(ValidateChunkSize("-1").IsError());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
