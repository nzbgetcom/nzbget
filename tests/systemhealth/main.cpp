/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 */

#include "nzbget.h"

#define BOOST_TEST_MODULE SystemHealthTests
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
