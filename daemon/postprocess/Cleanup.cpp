/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2013-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2025-2026 Denis <denis@nzbget.com>
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
#include "DownloadInfo.h"
#include "Cleanup.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"
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
	BString<1024> nzbName;
	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();
		nzbName = m_postInfo->GetNzbInfo()->GetName();
		m_interDir = fs::u8path(m_postInfo->GetNzbInfo()->GetDestDir());
		m_destDir = fs::u8path(m_postInfo->GetNzbInfo()->GetFinalDir());
		if (m_destDir.empty())
		{
			m_destDir = fs::u8path(m_postInfo->GetNzbInfo()->BuildFinalDirName().Str());
		}
	}

	BString<1024> infoName("move for %s", *nzbName);
	SetInfoName(*infoName);
	PrintMessage(Message::mkInfo, "Moving completed files for %s", *nzbName);

	bool ok = MoveFiles();

	infoName[0] = 'M'; // uppercase

	if (ok)
	{
		RemoveStaleHardlinks(*m_postInfo->GetNzbInfo(), m_destDir);
	}

	{
		GuardedDownloadQueue guard = DownloadQueue::Guard();
		if (m_postInfo && m_postInfo->GetNzbInfo())
		{
			if (ok)
			{
				PrintMessage(Message::mkInfo, "%s successful", *infoName);
				m_postInfo->GetNzbInfo()->SetDestDir(fs::u8string(m_destDir).c_str());
				m_postInfo->GetNzbInfo()->SetFinalDir("");
				m_postInfo->GetNzbInfo()->SetMoveStatus(NzbInfo::msSuccess);
			}
			else
			{
				PrintMessage(Message::mkError, "%s failed", *infoName);
				m_postInfo->GetNzbInfo()->SetMoveStatus(NzbInfo::msFailure);
			}
			m_postInfo->SetWorking(false);
		}
	}
}

bool MoveController::MoveFiles()
{
	if (m_interDir == m_destDir)
		return true;

	fs::error_code ec;
	if (fs::exists(m_interDir, ec) && fs::exists(m_destDir, ec))
	{
		if (fs::equivalent(m_interDir, m_destDir, ec))
			return true;
	}

	ec.clear();

	fs::create_directories(m_destDir, ec);
	if (ec)
	{
		PrintMessage(Message::mkError, "Could not create directory %s: %s",
			fs::u8string(m_destDir).c_str(), ec.message().c_str());
		return false;
	}

	if (!MoveFiles(m_interDir, m_destDir))
		return false;

	if (fs::exists(m_interDir, ec) && !ec)
	{
		fs::remove_all(m_interDir, ec);
		if (ec)
		{
			PrintMessage(Message::mkWarning, "Could not delete intermediate directory %s: %s",
				fs::u8string(m_interDir).c_str(), ec.message().c_str());
		}
	}

	return true;
}

bool MoveController::MoveFiles(const fs::path& src, const fs::path& dest)
{
	fs::error_code ec;
	auto it = fs::recursive_directory_iterator(src, fs::directory_options::skip_permission_denied, ec);
	auto end = fs::recursive_directory_iterator();

	if (ec)
	{
		PrintMessage(Message::mkError, "Could not open directory %s: %s",
			fs::u8string(src).c_str(), ec.message().c_str());
		return false;
	}

	while (it != end)
	{
		if (IsStopped() || ec) return false;

		const auto& entry = *it;
		auto relPath = entry.path().lexically_relative(src);
		fs::path dstPath = dest / relPath;

		if (entry.is_directory(ec))
		{
			fs::create_directories(dstPath, ec);
			if (ec) return false;
		}
		else 
		{
			std::string filename = fs::u8string(entry.path().filename());
			bool isDotFile = !filename.empty() && filename[0] == '.';

			if (!isDotFile)
			{
				PrintMessage(Message::mkInfo, "Moving file %s to %s",
					filename.c_str(), fs::u8string(dstPath.filename()).c_str());
			}

			fs::move_file(entry.path(), dstPath, ec);
			if (ec && ec == std::errc::file_exists)
			{
				ec.clear();

				if (entry.path() == dstPath)
				{
					it.increment(ec);
					continue;
				}

				if (fs::equivalent(entry.path(), dstPath, ec))
				{
					fs::remove(entry.path(), ec);
					it.increment(ec);
					continue;
				}
				ec.clear();

				dstPath = fs::make_unique_filename(dstPath);
				if (!isDotFile)
				{
					PrintMessage(Message::mkWarning, "File %s already exists, renaming to %s",
						filename.c_str(), fs::u8string(dstPath.filename()).c_str());
				}

				fs::move_file(entry.path(), dstPath, ec);
				if (ec) return false;
			}
			else if (ec)
			{
				PrintMessage(Message::mkError, "Could not move file %s: %s",
					filename.c_str(), ec.message().c_str());
				return false;
			}

			if (dstPath.filename() != entry.path().filename())
			{
				std::string newName = fs::u8string(dstPath.lexically_relative(dest));
				std::string oldRelName = fs::u8string(relPath);
				
				GuardedDownloadQueue guard = DownloadQueue::Guard();
				if (m_postInfo && m_postInfo->GetNzbInfo())
				{
					m_postInfo->GetNzbInfo()->RenameCompletedFile(oldRelName.c_str(), newName.c_str());
				}
			}
		}

		it.increment(ec);
	}

	if (ec)
	{
		PrintMessage(Message::mkError, "Could not read directory %s: %s",
			fs::u8string(src).c_str(), ec.message().c_str());
		return false;
	}

	if (IsStopped())
	{
		return false;
	}

	return true;
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
	SetInfoName(*infoName);

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

void MoveController::RemoveStaleHardlinks(NzbInfo& nzbInfo, const fs::path& destDir)
{
	const auto& hardLinkPath = nzbInfo.GetHardLinkPath();
	if (hardLinkPath.empty()) return;

	const auto path = fs::u8path(hardLinkPath);
	if (path == destDir)
		return;

	fs::error_code ec;
	if (fs::exists(path, ec) && fs::exists(destDir, ec))
	{
		if (fs::equivalent(path, destDir, ec))
			return;
	}

	ec.clear();

	fs::remove_all(path, ec);
	if (ec)
	{
		PrintMessage(Message::mkError, "Could not remove old hardlink directory: %s", ec.message().c_str());
	}
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
