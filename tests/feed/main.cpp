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

#define BOOST_TEST_MODULE "FeedTest"
#include <boost/test/included/unit_test.hpp>

#include "Log.h"
#include "Options.h"
#include "WorkState.h"
#include "DiskState.h"
#include "ArticleWriter.h"
#include "DupeCoordinator.h"
#include "ExtensionManager.h"
#include "HistoryCoordinator.h"
#include "QueueCoordinator.h"
#include "UrlCoordinator.h"
#include "QueueScript.h"
#include "PrePostProcessor.h"
#include "Scanner.h"
#include "StatMeter.h"
#include "Service.h"
#include "ServerPool.h"

char* (*g_EnvironmentVariables)[];
Log* g_Log;
WorkState* g_WorkState;
Options* g_Options;
DiskState* g_DiskState;
ArticleCache* g_ArticleCache;
DupeCoordinator* g_DupeCoordinator;
HistoryCoordinator* g_HistoryCoordinator;
PrePostProcessor* g_PrePostProcessor;
QueueScriptCoordinator* g_QueueScriptCoordinator;
QueueCoordinator* g_QueueCoordinator;
UrlCoordinator* g_UrlCoordinator;
ServiceCoordinator* g_ServiceCoordinator;
Scanner* g_Scanner;
StatMeter* g_StatMeter;
ServerPool* g_ServerPool;
ExtensionManager::Manager* g_ExtensionManager;

struct InitGlobals
{
	InitGlobals()
	{
		g_EnvironmentVariables = nullptr;
		g_Log = new Log();
		g_WorkState = new WorkState();
		g_DiskState = new DiskState();
	}

	~InitGlobals()
	{
		delete g_Log;
		delete g_WorkState;
		delete g_DiskState;
	}
};

BOOST_GLOBAL_FIXTURE(InitGlobals);
