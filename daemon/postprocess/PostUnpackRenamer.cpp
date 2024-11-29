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
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "nzbget.h"

#include "PostUnpackRenamer.h"
#include "FileSystem.h"
#include "Deobfuscation.h"

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
		{
			GuardedDownloadQueue guard = DownloadQueue::Guard();

			m_name = m_postInfo->GetNzbInfo()->GetName();
			m_dstDir = m_postInfo->GetNzbInfo()->GetDestDir();
		}

		std::string infoName = "Post-unpack renaming for " + m_name;
		SetInfoName(infoName.c_str());

		if (Deobfuscation::IsExcessivelyObfuscated(m_name))
		{
			PrintMessage(Message::mkWarning,
				"Skipping Post-unpack renaming. NZB filename %s is excessively obfuscated which makes renaming unreliable.",
				m_name.c_str()
			);
			m_postInfo->GetNzbInfo()->SetPostUnpackRenamingStatus(
				NzbInfo::PostUnpackRenamingStatus::Skipped
			);
			m_postInfo->SetWorking(false);
			return;
		}

		bool ok = RenameFiles(m_dstDir, m_name);

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
		DirBrowser dirBrowser(dir.c_str());
		while (const char* fileOrDir = dirBrowser.Next())
		{
			std::string srcFileOrDir = dir + PATH_SEPARATOR + fileOrDir;

			if (FileSystem::DirectoryExists(srcFileOrDir.c_str()))
			{
				RenameFiles(srcFileOrDir, newName);
				continue;
			}

			if (!Deobfuscation::IsExcessivelyObfuscated(fileOrDir))
			{
				PrintMessage(Message::mkInfo,
					"Filename %s is not excessively obfuscated, no renaming needed.",
					fileOrDir
				);
				continue;
			}

			std::string dstFile = dir + PATH_SEPARATOR + newName;
			dstFile += FileSystem::GetFileExtension(srcFileOrDir).value_or("");

			if (Util::MatchFileExt(dstFile.c_str(), g_Options->GetRenameIgnoreExt(), ","))
			{
				continue;
			}

			if (FileSystem::MoveFile(srcFileOrDir.c_str(), dstFile.c_str()))
			{
				PrintMessage(Message::mkInfo, "%s renamed to %s", srcFileOrDir.c_str(), dstFile.c_str());
			}
			else
			{
				PrintMessage(Message::mkError,
					"Could not rename file %s to %s: %s",
					srcFileOrDir.c_str(),
					dstFile.c_str(),
					*FileSystem::GetLastErrorMessage()
				);
			}
		}

		return true;
	}

	void Controller::AddMessage(Message::EKind kind, const char* text)
	{
		m_postInfo->GetNzbInfo()->AddMessage(kind, text);
	}
}
