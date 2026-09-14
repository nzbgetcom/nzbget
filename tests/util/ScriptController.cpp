/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Rick van Hattem <Wolph@wol.ph>
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

#ifdef WIN32

#include <boost/test/unit_test.hpp>
#include <future>
#include <io.h>

#include "ScriptController.h"

BOOST_AUTO_TEST_SUITE(ScriptControllerTest)

namespace
{
class HandleTestScript : public ScriptController
{
public:
	int Run()
	{
		char command[MAX_PATH];
		UINT length = GetSystemDirectoryA(command, MAX_PATH);
		BOOST_REQUIRE(length > 0 && length < MAX_PATH - 14);
		strcat(command, "\\hostname.exe");
		SetArgs({command});
		int input = -1, output = -1;
		StartProcess(&input, &output);
		BOOST_REQUIRE(input != -1);
		char buffer[256];
		while (_read(input, buffer, sizeof(buffer)) > 0) {}
		_close(input);
		return WaitProcess();
	}

	int RunAndTerminate()
	{
		char command[MAX_PATH];
		UINT length = GetSystemDirectoryA(command, MAX_PATH);
		BOOST_REQUIRE(length > 0 && length < MAX_PATH - 14);
		strcat(command, "\\cmd.exe");
		SetArgs({command, "/d"});
		SetNeedWrite(true);
		int input = -1, output = -1;
		StartProcess(&input, &output);
		BOOST_REQUIRE(input != -1 && output != -1);
		// Keep stdin open so cmd waits until explicitly terminated.
		auto result = std::async(std::launch::async, [this]() { return WaitProcess(); });
		bool waiting = result.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout;
		Terminate();
		_close(input);
		_close(output);
		int exitCode = result.get();
		BOOST_REQUIRE(waiting);
		return exitCode;
	}
};

DWORD HandleCount()
{
	DWORD count = 0;
	BOOST_REQUIRE(GetProcessHandleCount(GetCurrentProcess(), &count));
	return count;
}
}

BOOST_AUTO_TEST_CASE(ScriptProcessHandlesReleased)
{
	// Warm up the CRT process/pipe support before measuring kernel handles.
	{ HandleTestScript script; BOOST_REQUIRE_EQUAL(script.Run(), 0); }
	DWORD before = HandleCount();
	{
		HandleTestScript script;
		for (int i = 0; i < 16; ++i)
		{
			BOOST_REQUIRE_EQUAL(script.Run(), 0);
		}
		// A live controller may retain its most recent process for termination.
		BOOST_CHECK_LE(HandleCount(), before + 1);
	}
	BOOST_CHECK_EQUAL(HandleCount(), before);
}

BOOST_AUTO_TEST_CASE(ScriptProcessHandlesReleasedOnResume)
{
	{ HandleTestScript script; BOOST_REQUIRE_EQUAL(script.Run(), 0); }
	DWORD before = HandleCount();
	HandleTestScript script;
	for (int i = 0; i < 16; ++i)
	{
		BOOST_REQUIRE_EQUAL(script.Run(), 0);
		script.Resume();
	}
	BOOST_CHECK_EQUAL(HandleCount(), before);
}

BOOST_AUTO_TEST_CASE(ScriptProcessCanTerminateWhileWaiting)
{
	HandleTestScript script;
	BOOST_CHECK_EQUAL(script.RunAndTerminate(), -1);
}

BOOST_AUTO_TEST_CASE(BuildCommandLineEmptyArgs)
{
	ScriptController ctrl;
	char buf[256] = "initial";
	ctrl.BuildCommandLine(buf, sizeof(buf));
	BOOST_CHECK_EQUAL(buf, "");
}

BOOST_AUTO_TEST_CASE(BuildCommandLineSimpleArgs)
{
	ScriptController ctrl;
	ctrl.SetArgs({"unrar", "x", "-y"});
	char buf[256] = {};
	ctrl.BuildCommandLine(buf, sizeof(buf));
	BOOST_CHECK_EQUAL(buf, "\"unrar\" \"x\" \"-y\"");
}

BOOST_AUTO_TEST_CASE(BuildCommandLinePasswordWithSpace)
{
	ScriptController ctrl;
	ctrl.SetArgs({"unrar", "x", "-psecret pass"});
	char buf[256] = {};
	ctrl.BuildCommandLine(buf, sizeof(buf));
	BOOST_CHECK_EQUAL(buf, "\"unrar\" \"x\" \"-psecret pass\"");
}

BOOST_AUTO_TEST_CASE(BuildCommandLinePasswordWithQuote)
{
	ScriptController ctrl;
	ctrl.SetArgs({"unrar", "x", "-pfoo\"bar"});
	char buf[256] = {};
	ctrl.BuildCommandLine(buf, sizeof(buf));
	BOOST_CHECK_EQUAL(buf, "\"unrar\" \"x\" \"-pfoo\\\"bar\"");
}

BOOST_AUTO_TEST_CASE(BuildCommandLinePathWithTrailingBackslash)
{
	ScriptController ctrl;
	ctrl.SetArgs({"cmd", "/c", "C:\\path\\"});
	char buf[256] = {};
	ctrl.BuildCommandLine(buf, sizeof(buf));
	BOOST_CHECK_EQUAL(buf, "\"cmd\" \"/c\" \"C:\\path\\\\\"");
}

BOOST_AUTO_TEST_CASE(BuildCommandLineBackslashesBeforeQuote)
{
	ScriptController ctrl;
	ctrl.SetArgs({"-p", "a\\\"b"});
	char buf[256] = {};
	ctrl.BuildCommandLine(buf, sizeof(buf));
	BOOST_CHECK_EQUAL(buf, "\"-p\" \"a\\\\\\\"b\"");
}

BOOST_AUTO_TEST_CASE(BuildCommandLineMultipleTrailingBackslashes)
{
	ScriptController ctrl;
	ctrl.SetArgs({"dir\\\\"});
	char buf[256] = {};
	ctrl.BuildCommandLine(buf, sizeof(buf));
	BOOST_CHECK_EQUAL(buf, "\"dir\\\\\\\\\"");
}

BOOST_AUTO_TEST_CASE(BuildCommandLineEmptyStringArg)
{
	ScriptController ctrl;
	ctrl.SetArgs({""});
	char buf[256] = {};
	ctrl.BuildCommandLine(buf, sizeof(buf));
	BOOST_CHECK_EQUAL(buf, "\"\"");
}

BOOST_AUTO_TEST_CASE(BuildCommandLineTruncation)
{
	ScriptController ctrl;
	ctrl.SetArgs({"unrar", "x", "-p", "very_long_password_that_exceeds_buffer"});
	char buf[16] = {};
	ctrl.BuildCommandLine(buf, sizeof(buf));
	BOOST_CHECK(strlen(buf) < sizeof(buf));
	BOOST_CHECK_EQUAL(buf[sizeof(buf) - 1], '\0');
}

BOOST_AUTO_TEST_CASE(BuildCommandLineNullBuffer)
{
	ScriptController ctrl;
	ctrl.SetArgs({"test"});
	ctrl.BuildCommandLine(nullptr, 100);
	ctrl.BuildCommandLine(nullptr, 0);
	char buf[10] = "init";
	ctrl.BuildCommandLine(buf, 0);
	ctrl.BuildCommandLine(buf, -5);
	BOOST_CHECK_EQUAL(buf, "init");
}

BOOST_AUTO_TEST_CASE(BuildCommandLineStringVersion)
{
	ScriptController ctrl;
	ctrl.SetArgs({"unrar", "x", "-pfoo\"bar", "C:\\path\\"});
	BOOST_CHECK_EQUAL(ctrl.BuildCommandLine(), "\"unrar\" \"x\" \"-pfoo\\\"bar\" \"C:\\path\\\\\"");
}

BOOST_AUTO_TEST_CASE(BuildCommandLineStringVersionEmpty)
{
	ScriptController ctrl;
	BOOST_CHECK_EQUAL(ctrl.BuildCommandLine(), "");
}

BOOST_AUTO_TEST_CASE(BuildCommandLineExceedsWindowsLimit)
{
	ScriptController ctrl;
	std::string veryLongArg(33000, 'x');
	ctrl.SetArgs({veryLongArg.c_str()});
	std::string cmdLine = ctrl.BuildCommandLine();
	BOOST_CHECK(cmdLine.size() > 32767);
}

BOOST_AUTO_TEST_SUITE_END()

#endif // WIN32
