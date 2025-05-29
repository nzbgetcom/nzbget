/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2015-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"

#include <iostream>
#include <cassert>

#include "Util.h"
#include "FileSystem.h"
#include "TestUtil.h"
#include "Log.h"
#include "Options.h"
#include "DiskState.h"

bool TestUtil::m_usedWorkingDir = false;
std::string DataDir;

class NullStreamBuf : public std::streambuf
{
public:
	int sputc ( char c ) { return (int) c; }
} NullStreamBufSuiteInstance;

std::streambuf* oldcoutbuf;

void TestUtil::Init(const char* argv0)
{
	m_usedWorkingDir = false;

	CString filename = FileSystem::GetExeFileName(argv0);
	FileSystem::NormalizePathSeparators(filename);
	char* end = strrchr(filename, PATH_SEPARATOR);
	if (end) *end = '\0';
	DataDir = filename;
	DataDir += "/testdata";
	if (!FileSystem::DirectoryExists(DataDir.c_str()))
	{
		DataDir = filename;
		DataDir += "/tests/testdata";
	}
	if (!FileSystem::DirectoryExists(DataDir.c_str()))
	{
		DataDir = "";
	}
}

void TestUtil::Final()
{
	if (m_usedWorkingDir)
	{
		CleanupWorkingDir();
	}
}

const std::string TestUtil::TestDataDir()
{
	if (DataDir == "")
	{
		printf("ERROR: Directory \"testdata\" not found.\n");
		exit(1);
	}
	return DataDir;
}

const std::string TestUtil::WorkingDir()
{
	return TestDataDir() + "/temp";
}

void TestUtil::PrepareWorkingDir(const std::string templateDir)
{
	m_usedWorkingDir = true;

	std::string workDir = WorkingDir();
	std::string srcDir(TestDataDir() + "/" + templateDir);

	CString errmsg;
	int retries = 20;

	FileSystem::DeleteDirectoryWithContent(workDir.c_str(), errmsg);
	while (FileSystem::DirectoryExists(workDir.c_str()) && retries > 0)
	{
		Util::Sleep(100);
		retries--;
		FileSystem::DeleteDirectoryWithContent(workDir.c_str(), errmsg);
	}
	assert(FileSystem::DirectoryExists(workDir.c_str()) == false);
	FileSystem::CreateDirectory(workDir.c_str());
	assert(FileSystem::DirEmpty(workDir.c_str()));

	CopyAllFiles(workDir, srcDir);
}

void TestUtil::CopyAllFiles(const std::string destDir, const std::string srcDir)
{
	DirBrowser dir(srcDir.c_str());
	while (const char* filename = dir.Next())
	{
		std::string srcFile(srcDir + "/" + filename);
		std::string dstFile(destDir + "/" + filename);

		if (FileSystem::DirectoryExists(srcFile.c_str()))
		{
			FileSystem::CreateDirectory(dstFile.c_str());
			CopyAllFiles(dstFile.c_str(), srcFile.c_str());
			continue;
		}

		FileSystem::CopyFile(srcFile.c_str(), dstFile.c_str());
	}
}

void TestUtil::CleanupWorkingDir()
{
	CString errmsg;
	FileSystem::DeleteDirectoryWithContent(WorkingDir().c_str(), errmsg);
}

void TestUtil::DisableCout()
{
	oldcoutbuf = std::cout.rdbuf(&NullStreamBufSuiteInstance);
}

void TestUtil::EnableCout()
{
	std::cout.rdbuf(oldcoutbuf);
}

