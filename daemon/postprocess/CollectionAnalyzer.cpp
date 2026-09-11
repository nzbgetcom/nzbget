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
#include "CollectionAnalyzer.h"
#include "FileTypes.h"
#include "Deobfuscation.h"
#include "Util.h"
#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace CollectionAnalyzer
{
	static constexpr uintmax_t AMBIGUOUS_COLLECTION_RATIO = 3;

	AnalysisResult Analyze(const std::vector<FileEntry>& files)
	{
		AnalysisResult result;
		uintmax_t largestSize = 0;
		uintmax_t secondSize = 0;
		int videoCount = 0;
		int audioCount = 0;

		for (const auto& file : files)
		{
			if (FileTypes::IsDiscStructureExt(file.ext) || FileTypes::IsDiscDescriptorExt(file.ext))
			{
				result.isDiscStructure = true;
			}

			if (FileTypes::IsAudioExt(file.ext))
			{
				result.hasAudio = true;
				audioCount++;
				result.otherFiles.push_back(file);
			}
			else if (FileTypes::IsVideoExt(file.ext))
			{
				if (FileTypes::IsSampleStem(file.stem))
				{
					result.sampleVideo = file;
				}
				else
				{
					videoCount++;
					if (file.size > largestSize)
					{
						secondSize = largestSize;
						largestSize = file.size;
						result.mainVideo = file;
					}
					else if (file.size > secondSize)
					{
						secondSize = file.size;
					}
				}
			}
			else if (FileTypes::IsSubtitleExt(file.ext))
			{
				result.subtitles.push_back(file);
			}
			else if (FileTypes::IsNfoExt(file.ext))
			{
				result.nfos.push_back(file);
			}
			else
			{
				result.otherFiles.push_back(file);
			}
		}

		// The 3:1 Dominance Rule:
		// Multiple same-type videos with comparable size (e.g. season pack, multi-part episode)
		// have no single dominant feature. Renaming them to the release title would overwrite files.
		if (videoCount > 1 && largestSize <= secondSize * AMBIGUOUS_COLLECTION_RATIO)
		{
			result.isAmbiguousCollection = true;
		}

		// Music albums without video must not have individual tracks folded into one release title
		if (audioCount > 1 && videoCount == 0)
		{
			result.isAmbiguousCollection = true;
		}

		return result;
	}

	AnalysisResult AnalyzeDirectory(const fs::path& dir)
	{
		std::vector<FileEntry> files;
		bool discFound = false;

		fs::error_code ec;
		if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec))
		{
			return {};
		}

		for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
			it != fs::recursive_directory_iterator();
			it.increment(ec))
		{
			if (ec) break;

			if (it->is_directory(ec))
			{
				std::string dirname = fs::u8string(it->path().filename());
				if (FileTypes::IsClutterDir(dirname))
				{
					it.disable_recursion_pending();
					continue;
				}
				if (FileTypes::IsDiscStructureDir(dirname))
				{
					discFound = true;
					it.disable_recursion_pending();
				}
				continue;
			}

			if (it->is_regular_file(ec))
			{
				const fs::path& entryPath = it->path();
				std::string filename = fs::u8string(entryPath.filename());
				if (FileTypes::IsClutterFile(filename))
				{
					continue;
				}
				fs::error_code sizeEc;
				uintmax_t size = it->file_size(sizeEc);
				if (sizeEc)
				{
					continue;
				}
				FileEntry fe;
				fe.path = entryPath;
				fe.filename = std::move(filename);
				fe.stem = fs::u8string(entryPath.stem());
				fe.ext = fs::u8string(entryPath.extension());
				fe.size = size;
				files.push_back(std::move(fe));
			}
		}

		AnalysisResult res = Analyze(files);
		if (discFound)
		{
			res.isDiscStructure = true;
		}
		return res;
	}

	RenamePlan BuildPlan(const fs::path& dir, std::string_view targetName, const char* ignoreExt)
	{
		RenamePlan plan;

		AnalysisResult analysis = AnalyzeDirectory(dir);
		plan.isDiscStructure = analysis.isDiscStructure;
		plan.isAmbiguousCollection = analysis.isAmbiguousCollection;
		plan.canRename = analysis.CanRename();

		if (!plan.canRename)
		{
			return plan;
		}

		std::unordered_set<std::string> usedPaths;

		auto PlanFileRename = [&](const FileEntry& entry, const std::string& newBasename) -> std::string {
			std::string cleanBasename = FileSystem::SanitizePathSegment(newBasename);
			if (cleanBasename.empty())
			{
				return fs::u8string(entry.path.stem());
			}
			fs::path targetDstPath = entry.path.parent_path() / fs::u8path(cleanBasename);
			std::string srcStr = fs::u8string(entry.path);
			std::string targetDst = fs::u8string(targetDstPath);

			if (srcStr == targetDst)
			{
				usedPaths.insert(targetDst);
				return fs::u8string(entry.path.stem());
			}

			if (ignoreExt && Util::MatchFileExt(targetDst.c_str(), ignoreExt, ","))
			{
				return fs::u8string(entry.path.stem());
			}

			if (FileSystem::FileExists(targetDst.c_str()) || usedPaths.count(targetDst))
			{
				std::string dirStr = fs::u8string(entry.path.parent_path());
				targetDst = FileSystem::MakeUniqueFilename(dirStr.c_str(), cleanBasename.c_str()).Str();
				targetDstPath = fs::u8path(targetDst);
			}
			usedPaths.insert(targetDst);

			const char* actualFinalBasename = FileSystem::BaseFileName(targetDst.c_str());
			plan.actions.push_back({entry.path, targetDstPath, entry.filename, actualFinalBasename});
			return fs::u8string(targetDstPath.stem());
		};

		std::string cleanTargetName = FileSystem::SanitizePathSegment(targetName);
		bool canUseTargetName = !cleanTargetName.empty() && cleanTargetName != "nzb" &&
			!Deobfuscation::IsExcessivelyObfuscated(cleanTargetName);

		std::string videoBaseName = cleanTargetName;
		if (Deobfuscation::IsExcessivelyObfuscated(analysis.mainVideo.filename))
		{
			if (canUseTargetName)
			{
				std::string newVideoName = videoBaseName + analysis.mainVideo.ext;
				videoBaseName = PlanFileRename(analysis.mainVideo, newVideoName);
			}
			else
			{
				plan.targetNameObfuscated = true;
				videoBaseName = analysis.mainVideo.stem;
			}
		}
		else
		{
			videoBaseName = analysis.mainVideo.stem;
		}

		plan.effectiveBaseName = videoBaseName;

		if (!videoBaseName.empty() && !Deobfuscation::IsExcessivelyObfuscated(videoBaseName))
		{
			// 2. Plan sample rename if present
			if (!analysis.sampleVideo.filename.empty())
			{
				std::string sampleDstName = ResolveSampleName(videoBaseName, analysis.sampleVideo.ext);
				PlanFileRename(analysis.sampleVideo, sampleDstName);
			}

			// 3. Plan accompanying subtitles rename
			for (const auto& sub : analysis.subtitles)
			{
				std::string subDstName = ResolveSubtitleName(videoBaseName, sub.stem, sub.ext);
				PlanFileRename(sub, subDstName);
			}
		}

		return plan;
	}

	std::string ResolveSubtitleName(std::string_view baseName, std::string_view subStem, std::string_view subExt)
	{
		constexpr size_t MIN_LANG_TAG_LEN = 2; // ISO 639-1 codes
		constexpr size_t MAX_LANG_TAG_LEN = 4; // ISO 639-2 codes plus margin

		// Detect language tag at end of stem: e.g. "12345.en" -> ".en", "sub.eng" -> ".eng"
		size_t dotPos = subStem.rfind('.');
		if (dotPos != std::string_view::npos && dotPos > 0)
		{
			std::string_view tag = subStem.substr(dotPos + 1);
			if (tag.size() >= MIN_LANG_TAG_LEN && tag.size() <= MAX_LANG_TAG_LEN &&
				std::all_of(tag.begin(), tag.end(), [](unsigned char c) { return std::isalpha(c); }))
			{
				return std::string(baseName) + "." + std::string(tag) + std::string(subExt);
			}
		}
		return std::string(baseName) + std::string(subExt);
	}

	std::string ResolveSampleName(std::string_view baseName, std::string_view sampleExt)
	{
		return std::string(baseName) + "-sample" + std::string(sampleExt);
	}
}
