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
		{
			GuardedDownloadQueue guard = DownloadQueue::Guard();
			NzbInfo* nzbInfo = m_postInfo->GetNzbInfo();
			targetName = nzbInfo->GetMetaName();
			if (targetName.empty())
			{
				targetName = nzbInfo->GetName() ? nzbInfo->GetName() : "";
			}
			dstDir = nzbInfo->GetDestDir() ? nzbInfo->GetDestDir() : "";
		}

		NzbInfo::PostDownloadRenamingStatus finalStatus = NzbInfo::PostDownloadRenamingStatus::Skipped;

		auto Finish = [&]() {
			GuardedDownloadQueue guard = DownloadQueue::Guard();
			m_postInfo->GetNzbInfo()->SetPostDownloadRenamingStatus(finalStatus);
			m_postInfo->SetWorking(false);
		};

		if (dstDir.empty())
		{
			Finish();
			return;
		}

		BString<1024> infoName("Post-download renaming for %s", targetName.c_str());
		SetInfoName(*infoName);

		CollectionAnalyzer::RenamePlan plan = CollectionAnalyzer::BuildPlan(
			fs::u8path(dstDir), targetName, g_Options->GetRenameIgnoreExt());

		if (plan.isDiscStructure)
		{
			PrintMessage(Message::mkInfo, "Skipping Post-download renaming: disc structure detected");
			Finish();
			return;
		}

		if (plan.isAmbiguousCollection)
		{
			PrintMessage(Message::mkInfo, "Skipping Post-download renaming: ambiguous multi-file collection detected");
			Finish();
			return;
		}

		if (!plan.canRename)
		{
			PrintMessage(Message::mkInfo, "No qualifying media file found for Post-download renaming");
			Finish();
			return;
		}

		if (plan.actions.empty())
		{
			if (plan.targetNameObfuscated)
			{
				PrintMessage(Message::mkWarning,
					"Skipping Post-download renaming. Obfuscated files found, but NZB filename %s is also obfuscated and no clean metadata was provided.",
					targetName.c_str());
			}
			else
			{
				PrintMessage(Message::mkInfo, "No files needed renaming for %s", targetName.c_str());
			}
			Finish();
			return;
		}

		bool anyRenamed = false;
		bool anyFailed = false;
		NzbInfo* nzbInfo = m_postInfo->GetNzbInfo();

		for (const auto& action : plan.actions)
		{
			if (IsStopped()) break;

			fs::error_code ec;
			fs::move_file(action.srcPath, action.dstPath, ec);
			if (!ec)
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
					action.oldFilename.c_str(), action.newFilename.c_str(), ec.message().c_str());
				anyFailed = true;
			}
		}

		if (IsStopped())
		{
			PrintMessage(Message::mkWarning, "%s cancelled", *infoName);
			Finish();
			return;
		}

		if (anyFailed)
		{
			PrintMessage(Message::mkError, "%s finished with errors", *infoName);
			finalStatus = NzbInfo::PostDownloadRenamingStatus::Failure;
		}
		else if (anyRenamed)
		{
			PrintMessage(Message::mkInfo, "%s successful", *infoName);
			finalStatus = NzbInfo::PostDownloadRenamingStatus::Success;
		}
		else
		{
			PrintMessage(Message::mkInfo, "No files needed renaming for %s", *infoName);
			finalStatus = NzbInfo::PostDownloadRenamingStatus::Skipped;
		}

		Finish();
	}

	void Controller::AddMessage(Message::EKind kind, const char* text)
	{
		m_postInfo->GetNzbInfo()->AddMessage(kind, text);
	}
}
