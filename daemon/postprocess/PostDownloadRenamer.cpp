/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2026 Denis <denis@nzbget.com>
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
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */


#include "nzbget.h"

#include "PostDownloadRenamer.h"
#include "CollectionAnalyzer.h"
#include "FileSystem.h"
#include "Deobfuscation.h"
#include "Util.h"
#include "Options.h"

namespace PostDownloadRenamer
{
	void Controller::StartJob(PostInfo* postInfo)
	{
		Controller* controller = new (std::nothrow) Controller();

		if (!controller)
		{
			error("Failed to allocate memory for PostDownloadRenamer::Controller");
			return;
		}

		controller->m_postInfo = postInfo;
		controller->SetAutoDestroy(false);

		postInfo->SetPostThread(controller);

		controller->Start();
	}

	void Controller::Run()
	{
		std::string targetName;
		std::string dstDir;
		bool fromMetadata = false;
		{
			GuardedDownloadQueue guard = DownloadQueue::Guard();
			NzbInfo* nzbInfo = m_postInfo->GetNzbInfo();
			targetName = nzbInfo->GetMetaName();
			if (!targetName.empty())
			{
				fromMetadata = true;
			}
			else
			{
				targetName = nzbInfo->GetName() ? nzbInfo->GetName() : "";
			}
			dstDir = nzbInfo->GetDestDir() ? nzbInfo->GetDestDir() : "";
		}

		if (dstDir.empty())
		{
			m_postInfo->GetNzbInfo()->SetPostDownloadRenamingStatus(NzbInfo::PostDownloadRenamingStatus::Skipped);
			m_postInfo->SetWorking(false);
			return;
		}

		BString<1024> infoName("Post-download renaming for %s", targetName.c_str());
		SetInfoName(*infoName);

		CollectionAnalyzer::RenamePlan plan = CollectionAnalyzer::BuildPlan(
			fs::u8path(dstDir), targetName, g_Options->GetRenameIgnoreExt());

		if (plan.isDiscStructure)
		{
			PrintMessage(Message::mkInfo, "Skipping Post-download renaming: disc structure detected");
			m_postInfo->GetNzbInfo()->SetPostDownloadRenamingStatus(NzbInfo::PostDownloadRenamingStatus::Skipped);
			m_postInfo->SetWorking(false);
			return;
		}

		if (plan.isAmbiguousCollection)
		{
			PrintMessage(Message::mkInfo, "Skipping Post-download renaming: ambiguous multi-file collection detected");
			m_postInfo->GetNzbInfo()->SetPostDownloadRenamingStatus(NzbInfo::PostDownloadRenamingStatus::Skipped);
			m_postInfo->SetWorking(false);
			return;
		}

		if (!plan.canRename)
		{
			PrintMessage(Message::mkInfo, "No qualifying media file found for Post-download renaming");
			m_postInfo->GetNzbInfo()->SetPostDownloadRenamingStatus(NzbInfo::PostDownloadRenamingStatus::Skipped);
			m_postInfo->SetWorking(false);
			return;
		}

		if (plan.actions.empty())
		{
			PrintMessage(Message::mkInfo, "No files needed renaming for %s", targetName.c_str());
			m_postInfo->GetNzbInfo()->SetPostDownloadRenamingStatus(NzbInfo::PostDownloadRenamingStatus::Skipped);
			m_postInfo->SetWorking(false);
			return;
		}

		if (targetName.empty() || targetName == "nzb" || (!fromMetadata && Deobfuscation::IsExcessivelyObfuscated(targetName)))
		{
			PrintMessage(Message::mkWarning,
				"Skipping Post-download renaming. Obfuscated files found, but NZB filename %s is also obfuscated and no clean metadata was provided.",
				targetName.c_str());
			m_postInfo->GetNzbInfo()->SetPostDownloadRenamingStatus(NzbInfo::PostDownloadRenamingStatus::Skipped);
			m_postInfo->SetWorking(false);
			return;
		}

		bool anyRenamed = false;
		bool anyFailed = false;
		NzbInfo* nzbInfo = m_postInfo->GetNzbInfo();

		for (const auto& action : plan.actions)
		{
			if (IsStopped()) break;

			std::string srcStr = fs::u8string(action.srcPath);
			std::string dstStr = fs::u8string(action.dstPath);

			if (FileSystem::MoveFile(srcStr.c_str(), dstStr.c_str()))
			{
				PrintMessage(Message::mkInfo, "Renamed %s to %s", action.oldFilename.c_str(), action.newFilename.c_str());
				{
					GuardedDownloadQueue guard = DownloadQueue::Guard();
					nzbInfo->RenameCompletedFile(action.oldFilename.c_str(), action.newFilename.c_str());
				}
				anyRenamed = true;
			}
			else
			{
				PrintMessage(Message::mkError, "Could not rename file %s to %s: %s",
					action.oldFilename.c_str(), action.newFilename.c_str(), *FileSystem::GetLastErrorMessage());
				anyFailed = true;
			}
		}

		if (IsStopped())
		{
			PrintMessage(Message::mkWarning, "%s cancelled", *infoName);
			m_postInfo->GetNzbInfo()->SetPostDownloadRenamingStatus(NzbInfo::PostDownloadRenamingStatus::Skipped);
			m_postInfo->SetWorking(false);
			return;
		}

		GuardedDownloadQueue guard = DownloadQueue::Guard();
		if (anyFailed)
		{
			PrintMessage(Message::mkError, "%s finished with errors", *infoName);
			m_postInfo->GetNzbInfo()->SetPostDownloadRenamingStatus(NzbInfo::PostDownloadRenamingStatus::Failure);
		}
		else if (anyRenamed)
		{
			PrintMessage(Message::mkInfo, "%s successful", *infoName);
			m_postInfo->GetNzbInfo()->SetPostDownloadRenamingStatus(NzbInfo::PostDownloadRenamingStatus::Success);
		}
		else
		{
			PrintMessage(Message::mkInfo, "No files needed renaming for %s", *infoName);
			m_postInfo->GetNzbInfo()->SetPostDownloadRenamingStatus(NzbInfo::PostDownloadRenamingStatus::Skipped);
		}

		m_postInfo->SetWorking(false);
	}

	void Controller::AddMessage(Message::EKind kind, const char* text)
	{
		m_postInfo->GetNzbInfo()->AddMessage(kind, text);
	}
}
