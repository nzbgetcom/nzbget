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
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "nzbget.h"
#include "PostUnpackRenamer.h"
#include "Options.h"
#include "Log.h"
#include "FileSystem.h"
#include "Deobfuscation.h"
#include "FileTypes.h"

namespace PostUnpackRenamer
{

void Controller::StartJob(PostInfo* postInfo)
{
	Controller* renamer = new (std::nothrow) Controller();

	if (!renamer)
	{
		error("Failed to allocate memory for PostUnpackRenamer::Controller");
		return;
	}

	renamer->m_postInfo = postInfo;
	renamer->SetAutoDestroy(false);

	postInfo->SetPostThread(renamer);
	renamer->Start();
}

void Controller::Run()
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
	}
	else
	{
		nzbInfo->PrintMessage(Message::mkInfo, "No obfuscated files renamed for %s", nzbInfo->GetName());
	}

	RenameCompleted();
}

void Controller::RenameCompleted()
{
	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	if (!IsStopped())
	{
		m_postInfo->GetNzbInfo()->SetPostUnpackRenamingStatus(
			m_renamedCount > 0 ? NzbInfo::PostUnpackRenamingStatus::Success : NzbInfo::PostUnpackRenamingStatus::Nothing);
	}

	m_postInfo->SetWorking(false);
}

std::string Controller::ResolveSubtitleName(std::string_view metaname, std::string_view stem, std::string_view ext)
{
	if (stem.size() >= 4)
	{
		size_t dotPos = stem.rfind('.');
		if (dotPos != std::string_view::npos && dotPos > 0)
		{
			std::string_view langTag = stem.substr(dotPos + 1);
			if (langTag.size() >= 2 && langTag.size() <= 4 &&
				std::all_of(langTag.begin(), langTag.end(), [](unsigned char c) { return std::isalpha(c); }))
			{
				return std::string(metaname) + "." + std::string(langTag) + std::string(ext);
			}
		}
	}
	return std::string(metaname) + std::string(ext);
}

std::string Controller::ResolveUniqueName(std::string_view metaname, std::string_view stem, std::string_view ext,
	std::string_view baseName, const std::set<fs::path>& usedPaths, const fs::path& destPath)
{
	int counter = 0;
	std::string candidate(baseName);
	std::string metanameStr(metaname);
	std::string extStr(ext);
	std::string stemStr(stem);
	bool isSub = FileTypes::IsSubtitleExt(ext) && stem.size() > 2;
	fs::error_code ec;
	fs::path fullPath = destPath / candidate;
	while ((usedPaths.count(fullPath) || fs::exists(fullPath, ec)))
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
		fullPath = destPath / candidate;
	}
	return candidate;
}

std::vector<fs::path> Controller::CollectCandidates(const fs::path& dir)
{
	std::vector<fs::path> candidates;

	fs::error_code ec;
	for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
		it != fs::recursive_directory_iterator();
		it.increment(ec))
	{
		if (IsStopped()) break;
		if (ec) break;
		if (!it->is_regular_file()) continue;

		std::string filename = fs::u8string(it->path().filename());
		if (!Deobfuscation::IsExcessivelyObfuscated(filename.c_str())) continue;
		if (!it->path().has_extension()) continue;

		if (Util::MatchFileExt(filename.c_str(), g_Options->GetRenameIgnoreExt(), ",")) continue;

		candidates.push_back(it->path());
	}
	return candidates;
}

int Controller::RenameFiles(PostInfo* postInfo)
{
	NzbInfo* nzbInfo = postInfo->GetNzbInfo();

	if (Util::EmptyStr(nzbInfo->GetDestDir())) return 0;

	fs::path destPath = fs::u8path(nzbInfo->GetDestDir());
	if (!fs::is_directory(destPath))
	{
		return 0;
	}

	std::string metaname = nzbInfo->GetMetaName();
	if (metaname.empty() || Deobfuscation::IsExcessivelyObfuscated(metaname))
	{
		metaname = nzbInfo->GetName();
	}

	if (metaname.empty() || Deobfuscation::IsExcessivelyObfuscated(metaname))
	{
		return 0;
	}

	auto candidates = CollectCandidates(destPath);

	std::set<fs::path> usedPaths;
	int renamedCount = 0;

	for (const fs::path& fullPath : candidates)
	{
		if (IsStopped()) break;

		std::string ext = fs::u8string(fullPath.extension());
		std::string stem = fs::u8string(fullPath.stem());
		std::string filename = fs::u8string(fullPath.filename());
		std::string newName;

		if (FileTypes::IsSubtitleExt(ext))
		{
			newName = ResolveSubtitleName(metaname, stem, ext);
		}
		else if (FileTypes::IsSampleStem(stem))
		{
			newName = metaname + "-sample" + ext;
		}
		else
		{
			newName = metaname + ext;
		}

		fs::path parentDir = fullPath.parent_path();
		std::string candidate = ResolveUniqueName(metaname, stem, ext, newName, usedPaths, parentDir);
		fs::path newPath = parentDir / candidate;
		usedPaths.insert(newPath);

		nzbInfo->PrintMessage(Message::mkInfo, "Renaming obfuscated file %s to %s", filename.c_str(), candidate.c_str());
		fs::error_code moveEc;
		fs::move_file(fullPath, newPath, moveEc);
		if (moveEc)
		{
			nzbInfo->PrintMessage(Message::mkWarning,
				"Could not rename obfuscated file %s to %s: %s",
				filename.c_str(), candidate.c_str(), moveEc.message().c_str());
			continue;
		}

		if (!nzbInfo->RenameCompletedFile(filename.c_str(), candidate.c_str()))
		{
			nzbInfo->PrintMessage(Message::mkWarning,
				"Could not update completed file record for %s", filename.c_str());
		}

		++renamedCount;
	}

	return renamedCount;
}

}
