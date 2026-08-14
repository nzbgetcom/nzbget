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
#include "LoggingValidator.h"
#include "Options.h"

BOOST_AUTO_TEST_SUITE(SystemHealthTest)

using namespace SystemHealth;
using namespace SystemHealth::Logging;

BOOST_AUTO_TEST_SUITE(LoggingValidatorsSuite)

BOOST_AUTO_TEST_CASE(TestWriteLogValidator)
{
	Options::CmdOptList writeLogNone;
	writeLogNone.push_back("WriteLog=none");
	Options noneOptions(&writeLogNone, nullptr);
	Status noneStatus = WriteLogValidator(noneOptions).Validate();
	BOOST_CHECK(noneStatus.IsOk());
	BOOST_CHECK(noneStatus.GetMessage().empty());

	Options::CmdOptList writeLogAppend;
	writeLogAppend.push_back("WriteLog=append");
	Options appendOptions(&writeLogAppend, nullptr);
	Status appendStatus = WriteLogValidator(appendOptions).Validate();
	BOOST_CHECK(appendStatus.IsWarning());
	BOOST_CHECK(appendStatus.GetMessage().find("may grow indefinitely") != std::string::npos);

#ifdef _WIN32
	Options::CmdOptList writeLogAppendSpecial;
	writeLogAppendSpecial.push_back("WriteLog=append");
	writeLogAppendSpecial.push_back("LogFile=NUL");
	Options appendSpecialOptions(&writeLogAppendSpecial, nullptr);
	Status appendSpecialStatus = WriteLogValidator(appendSpecialOptions).Validate();
	BOOST_CHECK(appendSpecialStatus.IsOk());
	BOOST_CHECK(appendSpecialStatus.GetMessage().empty());
#else
	Options::CmdOptList writeLogAppendSpecial;
	writeLogAppendSpecial.push_back("WriteLog=append");
	writeLogAppendSpecial.push_back("LogFile=/dev/null");
	Options appendSpecialOptions(&writeLogAppendSpecial, nullptr);
	Status appendSpecialStatus = WriteLogValidator(appendSpecialOptions).Validate();
	BOOST_CHECK(appendSpecialStatus.IsOk());
	BOOST_CHECK(appendSpecialStatus.GetMessage().empty());

	Options::CmdOptList writeLogRotateSpecial;
	writeLogRotateSpecial.push_back("WriteLog=rotate");
	writeLogRotateSpecial.push_back("LogFile=/dev/null");
	Options rotateSpecialOptions(&writeLogRotateSpecial, nullptr);
	Status rotateSpecialStatus = WriteLogValidator(rotateSpecialOptions).Validate();
	BOOST_CHECK(rotateSpecialStatus.IsOk());

	Options::CmdOptList writeLogResetSpecial;
	writeLogResetSpecial.push_back("WriteLog=reset");
	writeLogResetSpecial.push_back("LogFile=/dev/null");
	Options resetSpecialOptions(&writeLogResetSpecial, nullptr);
	Status resetSpecialStatus = WriteLogValidator(resetSpecialOptions).Validate();
	BOOST_CHECK(resetSpecialStatus.IsOk());
#endif
}

BOOST_AUTO_TEST_CASE(TestRotateLogValidator)
{
	Options::CmdOptList rotateLogHigh;
	rotateLogHigh.push_back("WriteLog=rotate");
	rotateLogHigh.push_back("RotateLog=366");
	Options options(&rotateLogHigh, nullptr);

	Status status = RotateLogValidator(options).Validate();
	BOOST_CHECK(status.IsWarning());
	BOOST_CHECK(status.GetMessage().find("'RotateLog' is very high") != std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
