/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2025-2026 Denis <denis@nzbget.com>
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
#include "ScanScript.h"
#include "Options.h"

BOOST_AUTO_TEST_SUITE(ExtensionTest)

class TestScanScriptController : public ScanScriptController
{
public:
	TestScanScriptController(
		std::string& nzbName,
		std::string& category,
		std::string& dupeKey)
		: ScanScriptController(nzbName, category, dupeKey) {
	}

	void AddMessage(Message::EKind kind, const char* text) override
	{
		ScanScriptController::AddMessage(kind, text);
	}

	void SetPriority(int* priority)
	{
		ScanScriptController::SetPriority(priority);
	}

	void SetDupeScore(int* dupeScore)
	{
		ScanScriptController::SetDupeScore(dupeScore);
	}

	void SetDupeMode(EDupeMode* dupeMode)
	{
		ScanScriptController::SetDupeMode(dupeMode);
	}

	void SetAddTop(bool* addTop)
	{
		ScanScriptController::SetAddTop(addTop);
	}

	void SetAddPaused(bool* addPaused)
	{
		ScanScriptController::SetAddPaused(addPaused);
	}

	void SetParameters(NzbParameterList* parameters)
	{
		ScanScriptController::SetParameters(parameters);
	}
};

BOOST_AUTO_TEST_CASE(ScanScriptCommandTest)
{
	Options::CmdOptList cmdOpts;
	cmdOpts.push_back("NzbLog=no");
	Options options(&cmdOpts, nullptr);

	const char* nzbFilename = "NzbFilename";
	NzbInfo nzbInfo;
	const char* directory = "NzbDir";
	std::string nzbName = "NzbDir";
	std::string category = "NzbCategory";
	int priority = 0;
	NzbParameterList parameters;
	bool addTop = false;
	bool addPaused = false;
	std::string dupeKey = "DupeKey";
	int dupeScore = 0;
	EDupeMode dupeMode = EDupeMode::dmAll;

	TestScanScriptController ctrl(nzbName, category, dupeKey);
	ctrl.SetPriority(&priority);
	ctrl.SetDupeScore(&dupeScore);
	ctrl.SetDupeMode(&dupeMode);
	ctrl.SetAddTop(&addTop);
	ctrl.SetAddPaused(&addPaused);
	ctrl.SetParameters(&parameters);

	ctrl.AddMessage(Message::mkInfo, "[NZB] NZBNAME=my download");
	BOOST_CHECK_EQUAL(nzbName, "my download");

	ctrl.AddMessage(Message::mkInfo, "[NZB] CATEGORY=my category");
	BOOST_CHECK_EQUAL(category, "my category");

	ctrl.AddMessage(Message::mkInfo, "[NZB] DUPEKEY=tv show s01e02");
	BOOST_CHECK_EQUAL(dupeKey, "tv show s01e02");

	ctrl.AddMessage(Message::mkInfo, "[NZB] PRIORITY=50");
	BOOST_CHECK_EQUAL(priority, 50);

	ctrl.AddMessage(Message::mkInfo, "[NZB] DUPESCORE=10");
	BOOST_CHECK_EQUAL(dupeScore, 10);

	ctrl.AddMessage(Message::mkInfo, "[NZB] TOP=1");
	BOOST_CHECK_EQUAL(addTop, true);

	ctrl.AddMessage(Message::mkInfo, "[NZB] PAUSED=1");
	BOOST_CHECK_EQUAL(addPaused, true);

	ctrl.AddMessage(Message::mkInfo, "[NZB] DUPEMODE=FORCE");
	BOOST_CHECK_EQUAL(dupeMode, EDupeMode::dmForce);

	ctrl.AddMessage(Message::mkInfo, "[NZB] NZBPR_myvar=my value");
	BOOST_CHECK_EQUAL(parameters.back().GetName(), "myvar");
	BOOST_CHECK_EQUAL(parameters.back().GetValue(), "my value");
}

BOOST_AUTO_TEST_SUITE_END()
