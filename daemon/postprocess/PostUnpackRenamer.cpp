/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2024-2026 Denis <denis@nzbget.com>
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

#include "PostUnpackRenamer.h"
#include "FileSystem.h"
#include "Deobfuscation.h"
#include "Util.h"
#include "Options.h"

namespace PostUnpackRenamer
{
	void Controller::StartJob(PostInfo* postInfo)
	{
		Controller* controller = new (std::nothrow) Controller();
		if (!controller)
		{
			error("Failed to allocate memory for PostUnpackRenamer::Controller");
			return;
		}

		controller->m_postInfo = postInfo;
		controller->SetAutoDestroy(false);

		postInfo->SetPostThread(controller);

		controller->Start();
	}

	void Controller::Run()
	{
		std::string renameTargetName;
		{
			GuardedDownloadQueue guard = DownloadQueue::Guard();

			m_name = m_postInfo->GetNzbInfo()->GetName();
			m_dstDir = m_postInfo->GetNzbInfo()->GetDestDir();
			renameTargetName = m_postInfo->GetNzbInfo()->GetMetaName();
		}

		std::string infoName = "Post-unpack renaming for " + renameTargetName;
		SetInfoName(infoName.c_str());

		if (Deobfuscation::IsExcessivelyObfuscated(renameTargetName))
		{
			PrintMessage(Message::mkWarning,
				"Skipping Post-unpack renaming. Name %s is excessively obfuscated which makes renaming unreliable",
				renameTargetName.c_str()
			);
			m_postInfo->GetNzbInfo()->SetPostUnpackRenamingStatus(
				NzbInfo::PostUnpackRenamingStatus::Skipped
			);
			m_postInfo->SetWorking(false);
			return;
		}

		bool ok = RenameFiles(m_dstDir, renameTargetName);

		GuardedDownloadQueue guard = DownloadQueue::Guard();
		if (ok)
		{
			PrintMessage(Message::mkInfo, "%s successful", infoName.c_str());
			m_postInfo->GetNzbInfo()->SetPostUnpackRenamingStatus(
				NzbInfo::PostUnpackRenamingStatus::Success
			);
		}
		else
		{
			PrintMessage(Message::mkError, "%s failed", infoName.c_str());
			m_postInfo->GetNzbInfo()->SetPostUnpackRenamingStatus(
				NzbInfo::PostUnpackRenamingStatus::Failure
			);
		}

		m_postInfo->SetWorking(false);
	}

	bool Controller::RenameFiles(const std::string& dir, const std::string& newName)
	{
		bool success = true;
		DirBrowser dirBrowser(dir.c_str());
		while (const char* fileOrDir = dirBrowser.Next())
		{
			std::string srcFileOrDir = dir + PATH_SEPARATOR + fileOrDir;

			if (FileSystem::DirectoryExists(srcFileOrDir.c_str()))
			{
				if (!RenameFiles(srcFileOrDir, newName))
				{
					success = false;
				}
				continue;
			}

			if (IsDirectDownload(dir, srcFileOrDir))
			{
				continue;
			}

			if (!Deobfuscation::IsExcessivelyObfuscated(fileOrDir))
			{
				PrintMessage(Message::mkInfo,
					"Filename %s is not excessively obfuscated, no renaming needed",
					fileOrDir
				);
				continue;
			}

			std::string ext = FileSystem::GetFileExtension(srcFileOrDir).value_or("");
			std::string dstBasename = newName + ext;
			std::string dstFile = FileSystem::MakeUniqueFilename(dir.c_str(), dstBasename.c_str()).Str();

			if (Util::MatchFileExt(dstFile.c_str(), g_Options->GetRenameIgnoreExt(), ","))
			{
				continue;
			}

			if (FileSystem::MoveFile(srcFileOrDir.c_str(), dstFile.c_str()))
			{
				PrintMessage(Message::mkInfo, "%s renamed to %s", fileOrDir, dstBasename.c_str());
			}
			else
			{
				PrintMessage(Message::mkError,
					"Could not rename file %s to %s: %s",
					fileOrDir,
					dstBasename.c_str(),
					*FileSystem::GetLastErrorMessage()
				);
				success = false;
			}
		}

		return success;
	}

	/**
	 * @brief Checks if a file was directly downloaded (not extracted from an archive).
	 * 
	 * Compares the file path against the list of CompletedFiles in NzbInfo.
	 * Used to prevent PostUnpackRenamer from double-renaming files that were
	 * already processed by the pre-unpack RenameObfuscatedFiles stage.
	 */
	bool Controller::IsDirectDownload(const std::string& dir, const std::string& srcFileOrDir)
	{
		if (dir != m_dstDir)
		{
			return false;
		}

		GuardedDownloadQueue guard = DownloadQueue::Guard();
		NzbInfo* nzbInfo = m_postInfo->GetNzbInfo();
		for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
		{
			std::string completedFilePath = m_dstDir + PATH_SEPARATOR + completedFile.GetFilename();
			if (completedFilePath == srcFileOrDir)
			{
				return true;
			}
		}

		return false;
	}

	void Controller::AddMessage(Message::EKind kind, const char* text)
	{
		m_postInfo->GetNzbInfo()->AddMessage(kind, text);
	}
}
