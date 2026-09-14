/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Rick van Hattem <Wolph@wol.ph>
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

BOOST_AUTO_TEST_SUITE_END()
