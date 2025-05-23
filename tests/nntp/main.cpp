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

#include "nzbget.h"

#define BOOST_TEST_MODULE "NNTPTest" 
#include <boost/test/included/unit_test.hpp>

#include "Log.h"
#include "Options.h"
#include "WorkState.h"
#include "DiskState.h"
#include "ServerPool.h"
#include "YEncode.h"

Log* g_Log;
WorkState* g_WorkState;
Options* g_Options;
DiskState* g_DiskState;
ServerPool* g_ServerPool;

struct InitGlobals
{
	InitGlobals()
	{
		YEncode::init();
		g_Log = new Log();
		g_WorkState = new WorkState();
		g_Options = new Options(nullptr, nullptr);
		g_DiskState = new DiskState();
		g_ServerPool = new ServerPool();
	}

	~InitGlobals()
	{
		delete g_WorkState;
		delete g_Options;
		delete g_DiskState;
		delete g_ServerPool;
		delete g_Log;
	}
};

BOOST_GLOBAL_FIXTURE(InitGlobals);
