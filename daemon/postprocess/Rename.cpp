/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2016-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "Options.h"
#include "DiskState.h"
#include "Log.h"
#include "FileSystem.h"
#include "Deobfuscation.h"
#include "FileTypes.h"
#include "Rename.h"

#ifndef DISABLE_PARCHECK
void RenameController::PostParRenamer::PrintMessage(Message::EKind kind, const char* format, ...)
{
	char text[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(text, 1024, format, args);
	va_end(args);
	text[1024-1] = '\0';

	m_owner->m_postInfo->GetNzbInfo()->AddMessage(kind, text);
}
#endif


void RenameController::PostRarRenamer::PrintMessage(Message::EKind kind, const char* format, ...)
{
	char text[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(text, 1024, format, args);
	va_end(args);
	text[1024 - 1] = '\0';

	m_owner->m_postInfo->GetNzbInfo()->AddMessage(kind, text);
}


RenameController::RenameController()
{
	debug("Creating RenameController");

#ifndef DISABLE_PARCHECK
	m_parRenamer.m_owner = this;
#endif

	m_rarRenamer.m_owner = this;
}

void RenameController::StartJob(PostInfo* postInfo, EJobKind kind)
{
	RenameController* renameController = new RenameController();
	renameController->m_postInfo = postInfo;
	renameController->m_kind = kind;
	renameController->SetAutoDestroy(false);

	postInfo->SetPostThread(renameController);

	renameController->Start();
}

void RenameController::Run()
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

	BString<1024> infoName("rename for %s", *nzbName);
	SetInfoName(infoName);

	PrintMessage(Message::mkInfo, "Checking renamed %sfiles for %s",
		m_kind == jkRar ? "archive " : "", *nzbName);

	ExecRename(destDir, finalDir, nzbName);

	if (IsStopped())
	{
		PrintMessage(Message::mkWarning, "Renaming cancelled for %s", *nzbName);
	}
	else if (m_renamedCount > 0)
	{
		PrintMessage(Message::mkInfo, "Successfully renamed %i %sfile(s) for %s",
			m_renamedCount, m_kind == jkRar ? "archive " : "", *nzbName);
	}
	else
	{
		PrintMessage(Message::mkInfo, "No renamed %sfiles found for %s",
			m_kind == jkRar ? "archive " : "", *nzbName);
	}

	RenameCompleted();
}

void RenameController::AddMessage(Message::EKind kind, const char* text)
{
	m_postInfo->GetNzbInfo()->AddMessage(kind, text);
}

void RenameController::ExecRename(const char* destDir, const char* finalDir, const char* nzbName)
{
	if (m_kind == jkPar)
	{
#ifndef DISABLE_PARCHECK
		m_parRenamer.SetDestDir(m_postInfo->GetNzbInfo()->GetUnpackStatus() == NzbInfo::usSuccess &&
			!Util::EmptyStr(finalDir) ? finalDir : destDir);
		m_parRenamer.SetInfoName(nzbName);
		m_parRenamer.SetDetectMissing(m_postInfo->GetNzbInfo()->GetUnpackStatus() == NzbInfo::usNone);
		m_parRenamer.Execute();
#endif
	}
	else if (m_kind == jkRar)
	{
		m_rarRenamer.SetDestDir(destDir);
		m_rarRenamer.SetInfoName(nzbName);
		m_rarRenamer.SetIgnoreExt(g_Options->GetUnpackIgnoreExt());

		NzbParameter* parameter = m_postInfo->GetNzbInfo()->GetParameters()->Find("*Unpack:Password");
		if (parameter)
		{
			m_rarRenamer.SetPassword(parameter->GetValue());
		}

		m_rarRenamer.Execute();
	}
}

void RenameController::RenameCompleted()
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	if (m_kind == jkPar)
	{
		m_postInfo->GetNzbInfo()->SetParRenameStatus(m_renamedCount > 0 ? NzbInfo::rsSuccess : NzbInfo::rsNothing);
#ifndef DISABLE_PARCHECK
		// request another par2-file if the renaming has failed due to damaged par2-files
		if (m_renamedCount == 0 && m_parRenamer.HasDamagedParFiles() &&
			m_postInfo->GetNzbInfo()->GetRemainingParCount() > 0)
		{
			m_parRenamer.PrintMessage(Message::mkInfo, "Requesting extra par2-files for %s to perform par-rename", m_parRenamer.GetInfoName());
			downloadQueue->EditEntry(m_postInfo->GetNzbInfo()->GetId(), DownloadQueue::eaGroupResume, nullptr);
			downloadQueue->EditEntry(m_postInfo->GetNzbInfo()->GetId(), DownloadQueue::eaGroupPauseExtraPars, nullptr);
			if (m_postInfo->GetNzbInfo()->GetRemainingSize() > 0)
			{
				// reset rename status to execute renamer again, after the new par2-file is downloaded
				m_postInfo->GetNzbInfo()->SetParRenameStatus(NzbInfo::rsNone);
			}
		}
#endif
	}
	else if (m_kind == jkRar)
	{
		m_postInfo->GetNzbInfo()->SetRarRenameStatus(m_renamedCount > 0 ? NzbInfo::rsSuccess : NzbInfo::rsNothing);
	}

#ifndef DISABLE_PARCHECK
	if (m_parRenamer.HasMissedFiles() && m_postInfo->GetNzbInfo()->GetParStatus() <= NzbInfo::psSkipped)
	{
		m_parRenamer.PrintMessage(Message::mkInfo, "Requesting par-check/repair for %s to restore missing files ", m_parRenamer.GetInfoName());
		m_postInfo->SetRequestParCheck(true);
	}
#endif

	m_postInfo->SetWorking(false);
}

#ifndef DISABLE_PARCHECK
void RenameController::UpdateParRenameProgress()
{
	GuardedDownloadQueue guard = DownloadQueue::Guard();

	m_postInfo->SetProgressLabel(m_parRenamer.GetProgressLabel());
	m_postInfo->SetStageProgress(m_parRenamer.GetStageProgress());
}
#endif

void RenameController::UpdateRarRenameProgress()
{
	GuardedDownloadQueue guard = DownloadQueue::Guard();

	m_postInfo->SetProgressLabel(m_rarRenamer.GetProgressLabel());
	m_postInfo->SetStageProgress(m_rarRenamer.GetStageProgress());
}

/**
*  Update file name in the CompletedFiles-list of NZBInfo
*/
void RenameController::RegisterRenamedFile(const char* oldFilename, const char* newFilename)
{
	for (CompletedFile& completedFile : m_postInfo->GetNzbInfo()->GetCompletedFiles())
	{
		if (!strcasecmp(completedFile.GetFilename(), oldFilename))
		{
			if (Util::EmptyStr(completedFile.GetOrigname()))
			{
				completedFile.SetOrigname(completedFile.GetFilename());
			}
			completedFile.SetFilename(newFilename);
			break;
		}
	}
	m_renamedCount++;
}

namespace
{
	bool IsSampleStem(std::string_view stem)
	{
		return Util::EndsWith(stem.data(), "-sample", false) ||
			   Util::EndsWith(stem.data(), ".sample", false) ||
			   Util::EndsWith(stem.data(), "_sample", false);
	}
}

/**
 * @brief Renames excessively obfuscated directly-downloaded files.
 * 
 * Runs as a post-processing stage after ParRename and RarRename. It:
 * 1. Iterates over files in the download destination directory.
 * 2. Skips archives, parity files, and extensions in RenameIgnoreExt.
 * 3. Renames obfuscated files to the NZB metaname.
 * 4. Preserves subtitle language tags (e.g., .eng.srt) and sample suffixes.
 * 5. Handles filename collisions by appending a counter or the original stem.
 * 6. Renames corresponding stale hardlinks in finalDir if InterDir is set.
 */
int RenameObfuscatedFiles(PostInfo* postInfo)
{
	NzbInfo* nzbInfo = postInfo->GetNzbInfo();
	const char* destDir = nzbInfo->GetDestDir();
	const char* metaname = nzbInfo->GetMetaName();

	if (Util::EmptyStr(metaname) || Deobfuscation::IsExcessivelyObfuscated(metaname))
	{
		return 0;
	}

	int renamedCount = 0;
	std::unordered_set<std::string> usedNames;

	fs::path destPath(destDir ? destDir : "");
	if (!fs::is_directory(destPath))
	{
		return 0;
	}

	for (const auto& entry : fs::directory_iterator(destPath))
	{
		if (!fs::is_regular_file(entry.status()))
		{
			continue;
		}

		std::string filename = entry.path().filename().string();
		fs::path fullPath = entry.path();

		if (!Deobfuscation::IsExcessivelyObfuscated(filename.c_str()))
		{
			continue;
		}

		std::string ext = entry.path().extension().string();
		if (ext.empty() ||
			FileTypes::IsArchiveExt(ext) ||
			FileTypes::IsDiscStructureExt(ext) ||
			FileTypes::IsParityExt(ext))
		{
			continue;
		}

		if (Util::MatchFileExt(filename.c_str(), g_Options->GetRenameIgnoreExt(), ","))
		{
			continue;
		}

		std::string stem = filename.substr(0, filename.size() - ext.size());
		std::string newName(metaname);

		if (FileTypes::IsSubtitleExt(ext))
		{
			// Check for two-part extension like ".eng.srt", ".dut.sub"
			if (stem.size() > 4)
			{
				size_t dotPos = stem.rfind('.');
				if (dotPos != std::string::npos && dotPos > 0)
				{
					std::string langTag = stem.substr(dotPos + 1);
				if (langTag.size() >= 2 && langTag.size() <= 4 &&
					std::all_of(langTag.begin(), langTag.end(),
						[](unsigned char c) { return std::isalpha(c); }))
					{
						// Language tag found: "a1b2c3.eng.srt" → "metaname.eng.srt"
						newName = std::string(metaname) + "." + langTag + ext;
					}
					else
					{
						newName = std::string(metaname) + ext;
					}
				}
				else
				{
					newName = std::string(metaname) + ext;
				}
			}
			else
			{
				newName = std::string(metaname) + ext;
			}
		}
		else if (IsSampleStem(stem))
		{
			newName = std::string(metaname) + "-sample" + ext;
		}
		else
		{
			newName = std::string(metaname) + ext;
		}

		// Ensure unique filename within this batch and on disk
		std::string candidate = newName;
		int counter = 0;
		while (usedNames.count(candidate) || fs::exists(destPath / candidate))
		{
			++counter;
			if (FileTypes::IsSubtitleExt(ext) && stem.size() > 2)
			{
				candidate = std::string(metaname) + "." + stem + ext;
			}
			else
			{
				candidate = std::string(metaname) + "(" + std::to_string(counter) + ")" + ext;
			}
		}
		usedNames.insert(candidate);

		fs::path newPath = destPath / candidate;
		fs::error_code ec;
		fs::move_file(fullPath, newPath, ec);
		if (ec)
		{
			continue;
		}

		// Update CompletedFile in-memory
		for (CompletedFile& cf : *nzbInfo->GetCompletedFiles())
		{
			if (!strcasecmp(cf.GetFilename(), filename.c_str()))
			{
				if (Util::EmptyStr(cf.GetOrigname()))
				{
					cf.SetOrigname(cf.GetFilename());
				}
				cf.SetFilename(candidate.c_str());
				break;
			}
		}

		// If InterDir is set, DirectRenamer already hardlinked this file
		// to finalDir with the obfuscated name. Rename that hardlink too.
		if (g_Options->GetHardLinking() && !g_Options->GetInterDirPath().empty())
		{
			fs::path finalDirPath(nzbInfo->BuildFinalDirName().Str());
			fs::path oldFinalPath = finalDirPath / filename;
			fs::error_code ignore;
			if (fs::exists(oldFinalPath, ignore))
			{
				fs::path newFinalPath = finalDirPath / candidate;
				fs::move_file(oldFinalPath, newFinalPath, ec);
			}
		}

		++renamedCount;
	}

	return renamedCount;
}
