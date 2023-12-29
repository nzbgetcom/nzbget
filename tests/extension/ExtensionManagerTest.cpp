/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2023 Denis <denis@nzbget.com>
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

#include <filesystem>
#include <locale>
#include <iostream>
#include <string>
#include <codecvt>
#include <cstdint>

#include "ExtensionManager.h"
#include "Options.h"
#include "Log.h"
#include "DiskState.h"

Log* g_Log;
Options* g_Options;
DiskState* g_DiskState;

static std::string currentPath = std::filesystem::current_path().string();
static std::string testDir = currentPath + "/scripts";

class MockOptions : public IOptions {
public:
	const char* GetScriptDir() const override {
		return testDir.c_str();
	}

	const char* GetScriptOrder() const override {
		return "Extension2;Extension1;email;";
	}

	const char* GetExtensions() const override {
		return "Extension2;Extension3;Extension1;email;";
	}
};

BOOST_AUTO_TEST_CASE(ExtensionManagerTest)
{
	MockOptions options;

	std::vector<std::string> order = { "Extension2", "Extension1", "email" };
	ExtensionManager::Manager manager;

	BOOST_CHECK(manager.LoadExtensions(options) == true);
	BOOST_CHECK(manager.GetExtensions().size() == 4);

	for (size_t i = 0; i < manager.GetExtensions().size(); ++i)
	{
		if (i < order.size())
		{
			BOOST_CHECK(order[i] == manager.GetExtensions()[i].GetName());
		}
	}
}
