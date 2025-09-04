/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2024 Denis <denis@nzbget.com>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nzbget.h"

#define BOOST_TEST_MODULE HealthCheckTests
#include <boost/test/included/unit_test.hpp>

#include "Log.h"
#include "Options.h"

Log* g_Log;
Options* g_Options;

struct InitGlobals
{
	InitGlobals()
	{
		g_Log = new Log();
		g_Options = new Options(nullptr, nullptr);
	}

	~InitGlobals()
	{
		delete g_Log;
		delete g_Options;
	}
};

BOOST_GLOBAL_FIXTURE(InitGlobals);
