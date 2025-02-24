#include "nzbget.h"

#define BOOST_TEST_MODULE "QueueTests"
#include <boost/test/included/unit_test.hpp>

#include "Log.h"
#include "Options.h"
#include "DiskState.h"

Log* g_Log;
Options* g_Options;
DiskState* g_DiskState;

struct InitGlobals
{
	InitGlobals() 
	{
		g_Log = new Log();
		g_Options = new Options(nullptr, nullptr);
		g_DiskState = new DiskState();
	}

	~InitGlobals() 
	{
		delete g_Log;
		delete g_Options;
		delete g_DiskState;
	}
};

BOOST_GLOBAL_FIXTURE(InitGlobals);
