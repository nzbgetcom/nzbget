/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "nzbget.h"

#include <boost/test/unit_test.hpp>

#include "FileSystem.h"

#ifdef WIN32
BOOST_AUTO_TEST_CASE(FileSystemTest)
{
	BOOST_CHECK(!strcmp(FileSystem::MakeCanonicalPath("C:\\Program Files\\NZBGet"), "C:\\Program Files\\NZBGet"));
	BOOST_CHECK(!strcmp(FileSystem::MakeCanonicalPath("C:\\Program Files\\NZBGet\\"), "C:\\Program Files\\NZBGet\\"));
	BOOST_CHECK(!strcmp(FileSystem::MakeCanonicalPath("C:\\\\Program Files\\\\NZBGet"), "C:\\Program Files\\NZBGet"));
	BOOST_CHECK(!strcmp(FileSystem::MakeCanonicalPath("C:\\Program Files\\NZBGet\\scripts\\.."), "C:\\Program Files\\NZBGet\\"));
	BOOST_CHECK(!strcmp(FileSystem::MakeCanonicalPath("C:\\Program Files\\NZBGet\\scripts\\email\\..\\.."), "C:\\Program Files\\NZBGet\\"));
	BOOST_CHECK(!strcmp(FileSystem::MakeCanonicalPath("C:\\Program Files\\NZBGet\\scripts\\email\\..\\..\\"), "C:\\Program Files\\NZBGet\\"));
	BOOST_CHECK(!strcmp(FileSystem::MakeCanonicalPath("C:\\Program Files\\NZBGet\\."), "C:\\Program Files\\NZBGet\\"));
	BOOST_CHECK(!strcmp(FileSystem::MakeCanonicalPath("\\\\server\\Program Files\\NZBGet\\scripts\\email\\..\\..\\"), "\\\\server\\Program Files\\NZBGet\\"));
}
#endif

BOOST_AUTO_TEST_CASE(EscapePathForShellTest)
{
	BOOST_CHECK(FileSystem::EscapePathForShell("C:\\Program Files\\NZBGet\\unrar.exe") == "\"C:\\Program Files\\NZBGet\\unrar.exe\"");
	BOOST_CHECK(FileSystem::EscapePathForShell("") == "");
}
