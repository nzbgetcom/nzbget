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

#include "PostUnpack.h"
#include "FileSystem.h"
#include "Deobfuscation.h"

namespace PostUnpack
{
	void Controller::StartJob(PostInfo* postInfo)
	{
		Controller* controller = new Controller();

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

		bool ok = RenameFiles(m_dstDir, m_name);

		if (ok)
		{
			PrintMessage(Message::mkInfo, "%s successful", infoName.c_str());
			GuardedDownloadQueue guard = DownloadQueue::Guard();
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

	bool Controller::RenameFiles(const std::string& dir, const std::string& nameToRename)
	{
		DirBrowser dirBrowser(dir.c_str());
		while (const char* fileOrDir = dirBrowser.Next())
		{
			std::string srcFileOrDir = dir + PATH_SEPARATOR + fileOrDir;

			if (FileSystem::DirectoryExists(srcFileOrDir.c_str()))
			{
				RenameFiles(fileOrDir, nameToRename);
				continue;
			}

			if (!Deobfuscation::IsStronglyObfuscated(fileOrDir))
			{
				PrintMessage(Message::mkInfo,
					"%s doesn't need to be renamed.",
					srcFileOrDir.c_str()
				);
				continue;
			}

			std::string dstFile = dir + PATH_SEPARATOR + nameToRename;
			size_t lastindex = srcFileOrDir.find_last_of(".");
			std::string ext = srcFileOrDir.substr(lastindex);
			dstFile += ext;

			bool ignore = Util::MatchFileExt(dstFile.c_str(), g_Options->GetIgnoreExtsAndDirsToRename(), ",;");
			if (ignore)
			{
				continue;
			}

			if (FileSystem::MoveFile(srcFileOrDir.c_str(), dstFile.c_str()))
			{
				PrintMessage(Message::mkInfo,
					"%s renamed to %s",
					srcFileOrDir.c_str(),
					dstFile.c_str()
				);
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
