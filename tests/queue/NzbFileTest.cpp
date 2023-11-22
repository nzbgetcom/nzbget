/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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


#include <nzbget.h>

#define BOOST_TEST_MODULE "NzbFileTest" 
#include <boost/test/included/unit_test.hpp>

#include <filesystem>
#include <NzbFile.h>
#include <Log.h>
#include <Options.h>
#include <DiskState.h>

Log* g_Log;
Options* g_Options;
DiskState* g_DiskState;

void TestNzb(std::string testFilename)
{
	BOOST_TEST_MESSAGE(std::string("Filename: ") + testFilename);

	std::string path = std::filesystem::current_path().string();

	std::string nzbFilename(path + "/nzbfile/" + testFilename + ".nzb");
	std::string infoFilename(path + "/nzbfile/" + testFilename + ".txt");

	NzbFile nzbFile(nzbFilename.c_str(), "");
	bool parsedOK = nzbFile.Parse();
	BOOST_CHECK(parsedOK == true);

	FILE* infofile = fopen(infoFilename.c_str(), FOPEN_RB);
	BOOST_CHECK(infofile != nullptr);
	char buffer[1024];

	while (fgets(buffer, sizeof(buffer), infofile) && *buffer == '#') ;
	BOOST_CHECK(*buffer);

	int fileCount = atoi(buffer);
	std::unique_ptr<NzbInfo> nzbInfo = nzbFile.DetachNzbInfo();
	BOOST_CHECK(nzbInfo->GetFileCount() == fileCount);

	for (int i = 0; i < fileCount; i++)
	{
		while (fgets(buffer, sizeof(buffer), infofile) && *buffer == '#') ;
		BOOST_CHECK(*buffer);
		FileInfo* fileInfo = nzbInfo->GetFileList()->at(i).get();
		BOOST_CHECK(fileInfo != nullptr);
		Util::TrimRight(buffer);
		BOOST_CHECK(std::string(fileInfo->GetFilename()) == std::string(buffer));
	}

	fclose(infofile);
}

BOOST_AUTO_TEST_CASE(NZBParser)
{
	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);

	TestNzb("dotless");
	TestNzb("plain");
}
