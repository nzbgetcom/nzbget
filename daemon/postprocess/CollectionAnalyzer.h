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


#ifndef COLLECTION_ANALYZER_H
#define COLLECTION_ANALYZER_H

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include "FileSystem.h"

namespace CollectionAnalyzer
{
	struct FileEntry
	{
		fs::path path;
		std::string filename;
		std::string stem;
		std::string ext;
		uintmax_t size = 0;
	};

	struct AnalysisResult
	{
		FileEntry mainVideo;
		FileEntry sampleVideo;
		std::vector<FileEntry> subtitles;
		std::vector<FileEntry> nfos;
		std::vector<FileEntry> otherFiles;
		bool isAmbiguousCollection = false;
		bool isDiscStructure = false;
		bool hasAudio = false;

		bool CanRename() const
		{
			return !mainVideo.filename.empty() && !isAmbiguousCollection && !isDiscStructure;
		}
	};

	struct RenameAction
	{
		fs::path srcPath;
		fs::path dstPath;
		std::string oldFilename;
		std::string newFilename;
	};

	struct RenamePlan
	{
		std::vector<RenameAction> actions;
		bool isAmbiguousCollection = false;
		bool isDiscStructure = false;
		bool canRename = false;
	};

	AnalysisResult Analyze(const std::vector<FileEntry>& files);
	AnalysisResult AnalyzeDirectory(const fs::path& dir);

	RenamePlan BuildPlan(const fs::path& dir, std::string_view targetName, const char* ignoreExt);

	std::string ResolveSubtitleName(std::string_view baseName, std::string_view subStem, std::string_view subExt);
	std::string ResolveSampleName(std::string_view baseName, std::string_view sampleExt);
}

#endif
