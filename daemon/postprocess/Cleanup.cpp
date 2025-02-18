/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2013-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "Cleanup.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"
#include "ParParser.h"
#include "Options.h"

void MoveController::StartJob(PostInfo* postInfo)
{
	MoveController* moveController = new MoveController();
	moveController->m_postInfo = postInfo;
	moveController->SetAutoDestroy(false);

	postInfo->SetPostThread(moveController);

	moveController->Start();
}

void MoveController::Run()
{
	std::string nzbName;
	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();
		nzbName = m_postInfo->GetNzbInfo()->GetName();
		m_interDir = m_postInfo->GetNzbInfo()->GetDestDir();
		m_destDir = m_postInfo->GetNzbInfo()->GetFinalDir();
	}

	std::string infoName = "move for " + nzbName;
	SetInfoName(infoName.c_str());

	if (m_destDir.empty())
	{
		m_destDir = m_postInfo->GetNzbInfo()->BuildFinalDirName();
	}

	PrintMessage(Message::mkInfo, "Moving completed files for %s", nzbName.c_str());

	bool ok = MoveFiles();

	infoName[0] = 'M'; // uppercase

	if (ok)
	{
		PrintMessage(Message::mkInfo, "%s successful", infoName.c_str());
		// save new dest dir
		GuardedDownloadQueue guard = DownloadQueue::Guard();
		m_postInfo->GetNzbInfo()->SetDestDir(m_destDir.c_str());
		m_postInfo->GetNzbInfo()->SetFinalDir("");
		m_postInfo->GetNzbInfo()->SetMoveStatus(NzbInfo::msSuccess);
	}
	else
	{
		PrintMessage(Message::mkError, "%s failed", infoName.c_str());
		m_postInfo->GetNzbInfo()->SetMoveStatus(NzbInfo::msFailure);
	}

	m_postInfo->SetWorking(false);
}

bool MoveController::MoveFiles()
{
	if (m_interDir == m_destDir)
		return true;

	bool ok = true;
	MoveFiles(m_interDir, m_destDir, ok);
	
	CString errmsg;
	if (ok && !FileSystem::DeleteDirectoryWithContent(m_interDir.c_str(), errmsg))
	{
		PrintMessage(Message::mkWarning, "Could not delete intermediate directory %s: %s", m_interDir.c_str(), *errmsg);
	}

	return ok;
}

void MoveController::MoveFiles(const std::string& src, const std::string& dest, bool& isOk)
{
	DirBrowser dir(src.c_str());
	while (const char* filename = dir.Next())
	{
		const std::string srcFile = src + PATH_SEPARATOR + filename;
		const std::string dstFile = dest + PATH_SEPARATOR + filename;
		if (FileSystem::DirectoryExists(srcFile.c_str()))
		{
			CString errmsg;
			if (FileSystem::ForceDirectories(dstFile.c_str(), errmsg))
			{
				MoveFiles(srcFile, dstFile, isOk);
			}
			else
			{
				isOk = false;
				PrintMessage(Message::mkError, "Could not create directory %s: %s", dest.c_str(), *errmsg);
			}
		}
		else
		{
			bool hiddenFile = srcFile[0] == '.';
			if (hiddenFile)
				continue;

			PrintMessage(Message::mkInfo, "Moving file %s to %s", filename, dest.c_str());

			if (!FileSystem::MoveFile(srcFile.c_str(), dstFile.c_str()))
			{
				isOk = false;
				PrintMessage(Message::mkError, "Could not move file %s to %s: %s",
					srcFile.c_str(), dstFile.c_str(), *FileSystem::GetLastErrorMessage());
			}
		}
	}
}

void MoveController::AddMessage(Message::EKind kind, const char* text)
{
	m_postInfo->GetNzbInfo()->AddMessage(kind, text);
}

void CleanupController::StartJob(PostInfo* postInfo)
{
	CleanupController* cleanupController = new CleanupController();
	cleanupController->m_postInfo = postInfo;
	cleanupController->SetAutoDestroy(false);

	postInfo->SetPostThread(cleanupController);

	cleanupController->Start();
}

void CleanupController::Run()
{
	BString<1024> nzbName;
	CString destDir;
	CString finalDir;
	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();
		nzbName = m_postInfo->GetNzbInfo()->GetName();
		destDir = m_postInfo->GetNzbInfo()->GetDestDir();
		finalDir = m_postInfo->GetNzbInfo()->GetFinalDir();
	}

	BString<1024> infoName("cleanup for %s", *nzbName);
	SetInfoName(infoName);

	PrintMessage(Message::mkInfo, "Cleaning up %s", *nzbName);

	bool deleted = false;
	bool ok = Cleanup(destDir, &deleted);

	if (ok && !finalDir.Empty())
	{
		bool deleted2 = false;
		ok = Cleanup(finalDir, &deleted2);
		deleted = deleted || deleted2;
	}

	infoName[0] = 'C'; // uppercase

	if (ok && deleted)
	{
		PrintMessage(Message::mkInfo, "%s successful", *infoName);
		m_postInfo->GetNzbInfo()->SetCleanupStatus(NzbInfo::csSuccess);
	}
	else if (ok)
	{
		PrintMessage(Message::mkInfo, "Nothing to cleanup for %s", *nzbName);
		m_postInfo->GetNzbInfo()->SetCleanupStatus(NzbInfo::csSuccess);
	}
	else
	{
		PrintMessage(Message::mkError, "%s failed", *infoName);
		m_postInfo->GetNzbInfo()->SetCleanupStatus(NzbInfo::csFailure);
	}

	m_postInfo->SetWorking(false);
}

bool CleanupController::Cleanup(const char* destDir, bool *deleted)
{
	*deleted = false;
	bool ok = true;

	DirBrowser dir(destDir);
	while (const char* filename = dir.Next())
	{
		BString<1024> fullFilename("%s%c%s", destDir, PATH_SEPARATOR, filename);

		bool isDir = FileSystem::DirectoryExists(fullFilename);

		if (isDir)
		{
			ok &= Cleanup(fullFilename, deleted);
		}

		// check file extension
		bool deleteIt = Util::MatchFileExt(filename, g_Options->GetExtCleanupDisk(), ",;") && !isDir;

		if (deleteIt)
		{
			PrintMessage(Message::mkInfo, "Deleting file %s", filename);
			if (!FileSystem::DeleteFile(fullFilename))
			{
				PrintMessage(Message::mkError, "Could not delete file %s: %s", *fullFilename,
					*FileSystem::GetLastErrorMessage());
				ok = false;
			}

			*deleted = true;
		}
	}

	return ok;
}

void CleanupController::AddMessage(Message::EKind kind, const char* text)
{
	m_postInfo->GetNzbInfo()->AddMessage(kind, text);
}
