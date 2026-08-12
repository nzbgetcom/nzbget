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

#include "PostDownloadRenamer.h"
#include "CollectionAnalyzer.h"
#include "Options.h"
#include "Log.h"
#include "FileSystem.h"
#include "Deobfuscation.h"
#include "FileTypes.h"
#include "Util.h"

namespace PostDownloadRenamer
{

void Controller::StartJob(PostInfo* postInfo)
{
	Controller* renamer = new (std::nothrow) Controller();

	if (!renamer)
	{
		error("Failed to allocate memory for PostDownloadRenamer::Controller");
		return;
	}

	renamer->m_postInfo = postInfo;
	renamer->SetAutoDestroy(false);

	postInfo->SetPostThread(renamer);
	renamer->Start();
}

void Controller::Run()
{
	FinishStage(*this, m_postInfo, RenameFiles(*this, m_postInfo));
}

void FinishStage(Thread& thread, PostInfo* postInfo, const RenameResult& result)
{
	NzbInfo* nzbInfo = postInfo->GetNzbInfo();
	const char* name = nzbInfo->GetName();

	int renamedCount = result.downloadedCount + result.extractedCount;

	if (thread.IsStopped())
	{
		nzbInfo->PrintMessage(Message::mkWarning, "Obfuscated renaming cancelled for %s", name);
	}
	else if (renamedCount > 0)
	{
		nzbInfo->PrintMessage(Message::mkInfo, "Renamed %i obfuscated file(s) for %s", renamedCount, name);
	}
	else
	{
		nzbInfo->PrintMessage(Message::mkInfo, "No obfuscated files renamed for %s", name);
	}

	GuardedDownloadQueue downloadQueue = DownloadQueue::Guard();

	if (thread.IsStopped()) return;

	if (result.extractedRan)
	{
		nzbInfo->SetPostUnpackRenamingStatus(result.extractedFailed
			? NzbInfo::RenamingStatus::Failure
			: (result.extractedCount > 0 ? NzbInfo::RenamingStatus::Success : NzbInfo::RenamingStatus::Nothing));
	}
	if (result.downloadedRan)
	{
		nzbInfo->SetPostRenamingStatus(result.downloadedFailed
			? NzbInfo::RenamingStatus::Failure
			: (result.downloadedCount > 0 ? NzbInfo::RenamingStatus::Success : NzbInfo::RenamingStatus::Nothing));
	}

	postInfo->SetWorking(false);
}

std::string ResolveSubtitleName(std::string_view metaname, std::string_view stem, std::string_view ext)
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

std::string ResolveUniqueName(std::string_view metaname, std::string_view stem, std::string_view ext,
	std::string_view baseName, const std::set<fs::path>& usedPaths, const fs::path& destPath)
{
	int counter = 0;
	std::string candidate(baseName);
	std::string metanameStr(metaname);
	std::string extStr(ext);
	std::string stemStr(stem);
	bool isSub = FileTypes::IsSubtitleExt(ext) && stem.size() >= 2;
	fs::error_code ec;
	fs::path fullPath = destPath / candidate;
	while ((usedPaths.count(fullPath) || fs::exists(fullPath, ec)))
	{
		++counter;
		if (isSub)
		{
			if (counter == 1 && candidate != metanameStr + "." + stemStr + extStr)
			{
				candidate = metanameStr + "." + stemStr + extStr;
			}
			else
			{
				int subIndex = (baseName == metanameStr + "." + stemStr + extStr) ? counter : counter - 1;
				candidate = metanameStr + "." + stemStr + "(" + std::to_string(subIndex) + ")" + extStr;
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

std::vector<PostDownloadRenamer::Candidate> CollectCandidates(Thread& thread, const fs::path& dir)
{
	std::vector<Candidate> candidates;

	fs::error_code ec;
	for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
		it != fs::recursive_directory_iterator();
		it.increment(ec))
	{
		if (thread.IsStopped()) break;
		if (ec) break;
		if (!it->is_regular_file(ec)) continue;

		std::string filename = fs::u8string(it->path().filename());
		if (!Deobfuscation::IsExcessivelyObfuscated(filename)) continue;
		if (!it->path().has_extension()) continue;
		if (Util::MatchFileExt(filename.c_str(), g_Options->GetRenameIgnoreExt(), ",")) continue;

		Candidate candidate;
		candidate.path = it->path();
		candidate.parentDir = candidate.path.parent_path();
		candidate.filename = filename;
		candidate.stem = fs::u8string(candidate.path.stem());
		candidate.ext = fs::u8string(candidate.path.extension());
		candidate.extLower = candidate.ext;
		std::transform(candidate.extLower.begin(), candidate.extLower.end(), candidate.extLower.begin(),
			[](unsigned char c) { return std::tolower(c); });
		candidate.size = it->file_size(ec);
		if (ec) candidate.size = 0;
		candidates.push_back(std::move(candidate));
	}
	return candidates;
}

RenameResult RenameFiles(Thread& thread, PostInfo* postInfo, bool runDownloaded, bool runExtracted)
{
	NzbInfo* nzbInfo = postInfo->GetNzbInfo();

	RenameResult result;

	result.downloadedRan = runDownloaded;
	result.extractedRan = runExtracted;

	if (Util::EmptyStr(nzbInfo->GetDestDir())) return result;

	fs::path destPath = fs::u8path(nzbInfo->GetDestDir());
	if (!fs::is_directory(destPath))
	{
		return result;
	}

	auto IsValidName = [](const std::string& name)
	{
		return !name.empty() && !Deobfuscation::IsExcessivelyObfuscated(name);
	};

	std::string metaname = nzbInfo->GetMetaName();
	if (!IsValidName(metaname))
	{
		metaname = nzbInfo->GetName();
	}

	if (!IsValidName(metaname))
	{
		return result;
	}

	auto ToLower = [](std::string str)
	{
		std::transform(str.begin(), str.end(), str.begin(),
				[](unsigned char c) { return std::tolower(c); });
		return str;
	};

	std::vector<Candidate> downloadedCandidates;
	std::vector<Candidate> extractedCandidates;
	std::vector<Candidate> allCandidates = CollectCandidates(thread, destPath);
	{

		std::unordered_set<std::string> downloadedFilenames;
		GuardedDownloadQueue guard = DownloadQueue::Guard();
		for (FileInfo* fileInfo : nzbInfo->GetFileList())
		{
			if (fileInfo->GetFilename())
			{
				downloadedFilenames.insert(ToLower(FileSystem::BaseFileName(fileInfo->GetFilename())));
			}
		}
		for (CompletedFile& completedFile : *nzbInfo->GetCompletedFiles())
		{
			if (completedFile.GetFilename())
			{
				downloadedFilenames.insert(ToLower(FileSystem::BaseFileName(completedFile.GetFilename())));
			}
		}

		for (Candidate& candidate : allCandidates)
		{
			if (downloadedFilenames.count(ToLower(candidate.filename)))
			{
				if (runDownloaded)
				{
					downloadedCandidates.push_back(std::move(candidate));
				}
			}
			else if (runExtracted)
			{
				extractedCandidates.push_back(std::move(candidate));
			}
		}
	}

	std::set<fs::path> usedPaths;
	if (runExtracted)
	{
		PassResult res = RenameExtractedFiles(thread, nzbInfo, metaname, extractedCandidates, usedPaths);
		result.extractedCount = res.count;
		result.extractedFailed = res.failed;
	}
	if (runDownloaded)
	{
		PassResult res = RenameDownloadedFiles(thread, nzbInfo, metaname, downloadedCandidates, usedPaths);
		result.downloadedCount = res.count;
		result.downloadedFailed = res.failed;
	}

	return result;
}

RenameResult RenameFiles(Thread& thread, PostInfo* postInfo)
{
	NzbInfo* nzbInfo = postInfo->GetNzbInfo();

	bool extractedRan = g_Options->GetRenameAfterUnpack() &&
		nzbInfo->GetPostUnpackRenamingStatus() == NzbInfo::RenamingStatus::None &&
		nzbInfo->GetUnpackStatus() == NzbInfo::usSuccess;
	bool downloadedRan = g_Options->GetDirectRename() &&
		nzbInfo->GetPostRenamingStatus() == NzbInfo::RenamingStatus::None;

	return RenameFiles(thread, postInfo, downloadedRan, extractedRan);
}

PassResult RenameCandidates(Thread& thread, NzbInfo* nzbInfo, const std::string& metaname,
	const std::vector<Candidate>& candidates, bool updateCompletedRecord, std::set<fs::path>& usedPaths)
{
	CollectionAnalyzer analyzer(candidates);
	PassResult passResult;

	for (const Candidate& candidate : candidates)
	{
		if (thread.IsStopped()) break;
		if (analyzer.ShouldSkip(candidate)) continue;

		const std::string& filename = candidate.filename;
		const std::string& stem = candidate.stem;
		const std::string& ext = candidate.ext;
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

		std::string candidateName = ResolveUniqueName(metaname, stem, ext, newName, usedPaths, candidate.parentDir);
		fs::path newPath = candidate.parentDir / candidateName;
		usedPaths.insert(newPath);

		nzbInfo->PrintMessage(Message::mkInfo, "Renaming obfuscated file %s to %s", filename.c_str(), candidateName.c_str());
		fs::error_code moveEc;
		fs::move_file(candidate.path, newPath, moveEc);
		if (moveEc)
		{
			nzbInfo->PrintMessage(Message::mkWarning,
				"Could not rename obfuscated file %s to %s: %s",
				filename.c_str(), candidateName.c_str(), moveEc.message().c_str());
			passResult.failed = true;
			continue;
		}

		if (updateCompletedRecord)
		{
			bool updated;
			{
				GuardedDownloadQueue guard = DownloadQueue::Guard();
				updated = nzbInfo->RenameCompletedFile(filename.c_str(), candidateName.c_str());
			}
			if (!updated)
			{
				nzbInfo->PrintMessage(Message::mkWarning,
					"Could not update completed file record for %s", filename.c_str());
			}
		}

		++passResult.count;
	}

	return passResult;
}

PassResult RenameDownloadedFiles(Thread& thread, NzbInfo* nzbInfo, const std::string& metaname,
	const std::vector<Candidate>& candidates, std::set<fs::path>& usedPaths)
{
	return RenameCandidates(thread, nzbInfo, metaname, candidates, true, usedPaths);
}

PassResult RenameExtractedFiles(Thread& thread, NzbInfo* nzbInfo, const std::string& metaname,
	const std::vector<Candidate>& candidates, std::set<fs::path>& usedPaths)
{
	return RenameCandidates(thread, nzbInfo, metaname, candidates, false, usedPaths);
}

}
