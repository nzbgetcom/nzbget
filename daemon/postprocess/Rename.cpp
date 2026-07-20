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

void ObfuscatedRenamer::StartJob(PostInfo* postInfo)
{
	ObfuscatedRenamer* renamer = new ObfuscatedRenamer();
	renamer->m_postInfo = postInfo;
	renamer->SetAutoDestroy(false);

	postInfo->SetPostThread(renamer);
	renamer->Start();
}

void ObfuscatedRenamer::Run()
{
	NzbInfo* nzbInfo = m_postInfo->GetNzbInfo();

	m_renamedCount = RenameFiles(m_postInfo);

	if (IsStopped())
	{
		nzbInfo->PrintMessage(Message::mkWarning, "Obfuscated renaming cancelled for %s", nzbInfo->GetName());
	}
	else if (m_renamedCount > 0)
	{
		nzbInfo->PrintMessage(Message::mkInfo, "Renamed %i obfuscated file(s) for %s", m_renamedCount, nzbInfo->GetName());
		nzbInfo->SetObfuscatedRenameStatus(NzbInfo::RenameStatus::Success);
	}
	else
	{
		nzbInfo->PrintMessage(Message::mkInfo, "No obfuscated files renamed for %s", nzbInfo->GetName());
		nzbInfo->SetObfuscatedRenameStatus(NzbInfo::RenameStatus::Nothing);
	}

	RenameCompleted();
}

void ObfuscatedRenamer::RenameCompleted()
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();
	m_postInfo->SetWorking(false);
}

std::string ObfuscatedRenamer::ResolveSubtitleName(std::string_view metaname, std::string_view stem, std::string_view ext)
{
	if (stem.size() > 4)
	{
		size_t dotPos = stem.rfind('.');
		if (dotPos != std::string_view::npos && dotPos > 0)
		{
			std::string_view langTag = stem.substr(dotPos + 1);
			if (langTag.size() >= 2 && langTag.size() <= 4 &&
				std::all_of(langTag.begin(), langTag.end(), [](unsigned char c) { return std::isalpha(c); }))
			{
				// Language tag found: "a1b2c3.eng.srt" -> "metaname.eng.srt"
				return std::string(metaname) + "." + std::string(langTag) + std::string(ext);
			}
		}
	}
	return std::string(metaname) + std::string(ext);
}

std::string ObfuscatedRenamer::ResolveUniqueName(std::string_view metaname, std::string_view stem, std::string_view ext,
	std::string_view baseName, const std::unordered_set<std::string>& usedNames, const fs::path& destPath)
{
	
	int counter = 0;
	fs::error_code ec;
	std::string candidate(baseName);
	std::string metanameStr(metaname);
	std::string extStr(ext);
	std::string stemStr(stem);
	bool isSub = FileTypes::IsSubtitleExt(ext) && stem.size() > 2;

	while (usedNames.count(candidate) || fs::exists(destPath / candidate, ec))
	{
		++counter;
		if (isSub)
		{
			if (counter == 1)
			{
				candidate = metanameStr + "." + stemStr + extStr;
			}
			else
			{
				candidate = metanameStr + "." + stemStr + "(" + std::to_string(counter - 1) + ")" + extStr;
			}
		}
		else
		{
			candidate = metanameStr + "(" + std::to_string(counter) + ")" + extStr;
		}
	}
	return candidate;
}

void ObfuscatedRenamer::CollectCandidates(const fs::path& dir, std::vector<fs::path>& candidates)
{
	if (IsStopped()) return;

	fs::error_code ec;
	for (const auto& entry : fs::directory_iterator(dir, ec))
	{
		if (ec || IsStopped()) break;

		if (fs::is_directory(entry.status()))
		{
			CollectCandidates(entry.path(), candidates);
			continue;
		}

		if (!fs::is_regular_file(entry.status()))
		{
			continue;
		}

		std::string filename = fs::u8string(entry.path().filename());
		if (!Deobfuscation::IsExcessivelyObfuscated(filename.c_str()))
		{
			continue;
		}

		if (!entry.path().has_extension())
		{
			continue;
		}

		if (Util::MatchFileExt(filename.c_str(), g_Options->GetRenameIgnoreExt(), ","))
		{
			continue;
		}

		candidates.push_back(entry.path());
	}
}

int ObfuscatedRenamer::RenameFiles(PostInfo* postInfo)
{
	if (IsStopped()) return 0;

	NzbInfo* nzbInfo = postInfo->GetNzbInfo();
	const char* destDir = nzbInfo->GetDestDir();
	const char* metaname = nzbInfo->GetMetaName();

	if (Util::EmptyStr(metaname) || Deobfuscation::IsExcessivelyObfuscated(metaname))
	{
		metaname = nzbInfo->GetName();
		if (Util::EmptyStr(metaname) || Deobfuscation::IsExcessivelyObfuscated(metaname))
		{
			return 0;
		}
	}

	fs::path destPath(destDir ? destDir : "");
	if (!fs::is_directory(destPath))
	{
		return 0;
	}

	std::vector<fs::path> candidates;
	CollectCandidates(destPath, candidates);

	std::unordered_set<std::string> usedNames;
	int renamedCount = 0;

	for (const fs::path& fullPath : candidates)
	{
		if (IsStopped()) break;

		std::string filename = fs::u8string(fullPath.filename());
		std::string ext = fs::u8string(fullPath.extension());
		std::string stem = filename.substr(0, filename.size() - ext.size());
		std::string newName(metaname);

		if (FileTypes::IsSubtitleExt(ext))
		{
			newName = ResolveSubtitleName(metaname, stem, ext);
		}
		else if (FileTypes::IsSampleStem(stem))
		{
			newName = std::string(metaname) + "-sample" + ext;
		}
		else
		{
			newName = std::string(metaname) + ext;
		}

		// Ensure unique filename within this batch and on disk
		fs::path parentDir = fullPath.parent_path();
		std::string candidate = ResolveUniqueName(metaname, stem, ext, newName, usedNames, parentDir);
		usedNames.insert(candidate);

		fs::path newPath = parentDir / candidate;
		fs::error_code ec;
		fs::move_file(fullPath, newPath, ec);
		if (ec)
		{
			nzbInfo->PrintMessage(Message::mkWarning,
				"Could not rename obfuscated file %s to %s: %s",
				filename.c_str(), candidate.c_str(), ec.message().c_str());
			continue;
		}

		// Update CompletedFile in-memory (only for direct-download files)
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

		++renamedCount;
	}

	return renamedCount;
}
