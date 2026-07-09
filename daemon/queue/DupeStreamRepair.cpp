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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"

#include <algorithm>
#include <cctype>
#include "DupeStreamRepair.h"
#include "DupeArticleFallback.h"
#include "FileSystem.h"
#include "Options.h"
#include "Log.h"
#include "Util.h"

bool DupeStreamRepair::IsStreamEligible(const char* filename)
{
	if (Util::EmptyStr(filename))
	{
		return false;
	}

	const char* extension = strrchr(filename, '.');
	if (!extension)
	{
		return false;
	}

	// directly posted media containers: the posted bytes are the playable
	// file itself, so two same-content postings share identical decoded bytes
	static const char* mediaExtensions[] = { ".mkv", ".mp4", ".m4v", ".avi", ".ts",
		".mov", ".wmv", ".mpg", ".mpeg", ".webm" };

	for (const char* mediaExtension : mediaExtensions)
	{
		if (!strcasecmp(extension, mediaExtension))
		{
			return true;
		}
	}

	return false;
}

StreamRangeList DupeStreamRepair::ComputeHoles(FileInfo* fileInfo)
{
	int64 fileSize = fileInfo->GetDecodedFileSize();
	if (fileSize <= 0)
	{
		return StreamRangeList();
	}

	// ranges of successfully decoded articles; failed articles carry no
	// offset/size (their yEnc headers never arrived) and default to 0/0
	StreamRangeList covered;
	for (ArticleInfo* article : fileInfo->GetArticles())
	{
		if (article->GetStatus() == ArticleInfo::aiFinished && article->GetSegmentSize() > 0)
		{
			covered.push_back({article->GetSegmentOffset(), article->GetSegmentSize()});
		}
	}

	std::sort(covered.begin(), covered.end(),
		[](const StreamRange& range1, const StreamRange& range2)
		{
			return range1.Offset < range2.Offset;
		});

	StreamRangeList holes;
	int64 pos = 0;
	for (const StreamRange& range : covered)
	{
		if (range.Offset > pos)
		{
			holes.push_back({pos, range.Offset - pos});
		}
		pos = std::max(pos, range.End());
	}
	if (pos < fileSize)
	{
		holes.push_back({pos, fileSize - pos});
	}

	return holes;
}

int64 DupeStreamRepair::TotalSize(const StreamRangeList& ranges)
{
	int64 total = 0;
	for (const StreamRange& range : ranges)
	{
		total += range.Size;
	}
	return total;
}

bool DupeStreamRepair::IntersectsAny(const StreamRange& range, const StreamRangeList& ranges)
{
	for (const StreamRange& other : ranges)
	{
		if (RangesIntersect(range, other))
		{
			return true;
		}
	}
	return false;
}

StreamRangeList DupeStreamRepair::EstimateDonorRanges(FileInfo* donorFile, int64 decodedFileSize)
{
	StreamRangeList ranges;

	int64 totalEncoded = 0;
	for (ArticleInfo* article : donorFile->GetArticles())
	{
		totalEncoded += article->GetSize();
	}

	if (totalEncoded <= 0 || decodedFileSize <= 0)
	{
		return ranges;
	}

	// scale cumulative encoded offsets into the decoded stream; double
	// arithmetic to avoid int64 overflow on multi-gigabyte files (the
	// ~1e-6 relative error is far below the patch margin of whole parts)
	int64 cumulative = 0;
	int64 previousEnd = 0;
	for (ArticleInfo* article : donorFile->GetArticles())
	{
		cumulative += article->GetSize();
		int64 end = (int64)((double)cumulative / (double)totalEncoded * (double)decodedFileSize);
		end = std::min(end, decodedFileSize);
		end = std::max(end, previousEnd);
		ranges.push_back({previousEnd, end - previousEnd});
		previousEnd = end;
	}

	// the last range always ends exactly at the decoded file size
	if (!ranges.empty())
	{
		ranges.back().Size += decodedFileSize - previousEnd;
	}

	return ranges;
}

std::vector<int> DupeStreamRepair::SelectPatchParts(const StreamRangeList& donorRanges,
	const StreamRangeList& holes, int marginParts)
{
	std::vector<bool> picked(donorRanges.size(), false);

	for (size_t i = 0; i < donorRanges.size(); i++)
	{
		if (IntersectsAny(donorRanges[i], holes))
		{
			int from = std::max(0, (int)i - marginParts);
			int to = std::min((int)donorRanges.size() - 1, (int)i + marginParts);
			for (int j = from; j <= to; j++)
			{
				picked[j] = true;
			}
		}
	}

	std::vector<int> parts;
	for (size_t i = 0; i < picked.size(); i++)
	{
		if (picked[i])
		{
			parts.push_back((int)i);
		}
	}
	return parts;
}

std::vector<int> DupeStreamRepair::SelectProbeParts(const StreamRangeList& donorRanges,
	const StreamRangeList& holes, int probeCount)
{
	// candidates: parts whose estimated range - and the neighbors', to absorb
	// estimation error - lies entirely inside already-downloaded regions
	std::vector<int> candidates;
	for (size_t i = 0; i < donorRanges.size(); i++)
	{
		bool clear = true;
		size_t from = i > 0 ? i - 1 : 0;
		size_t to = std::min(donorRanges.size() - 1, i + 1);
		for (size_t j = from; j <= to && clear; j++)
		{
			clear = !IntersectsAny(donorRanges[j], holes);
		}
		if (clear)
		{
			candidates.push_back((int)i);
		}
	}

	if ((int)candidates.size() <= probeCount)
	{
		return candidates;
	}

	// spread the probes evenly across the candidate list
	std::vector<int> probes;
	for (int k = 0; k < probeCount; k++)
	{
		probes.push_back(candidates[candidates.size() * (2 * k + 1) / (2 * probeCount)]);
	}
	return probes;
}

void DupeStreamRepair::SubtractCovered(StreamRangeList& ranges, const StreamRange& covered)
{
	StreamRangeList remaining;

	for (const StreamRange& range : ranges)
	{
		if (!RangesIntersect(range, covered))
		{
			remaining.push_back(range);
			continue;
		}
		if (range.Offset < covered.Offset)
		{
			remaining.push_back({range.Offset, covered.Offset - range.Offset});
		}
		if (covered.End() < range.End())
		{
			remaining.push_back({covered.End(), range.End() - covered.End()});
		}
	}

	ranges = std::move(remaining);
}

int64 DupeStreamRepair::RequiredCompareFloor(int64 decodedFileSize, const StreamRangeList& holes,
	const StreamRangeList& donorRanges, const std::vector<int>& probeParts)
{
	int64 base = BaseCompareFloor(decodedFileSize - TotalSize(holes));

	int64 achievable = 0;
	for (int partIndex : probeParts)
	{
		StreamRangeList presentPart = { donorRanges[partIndex] };
		for (const StreamRange& hole : holes)
		{
			SubtractCovered(presentPart, hole);
		}
		achievable += TotalSize(presentPart);
	}

	return achievable >= 64 ? std::min(base, achievable) : base;
}

bool DupeStreamRepair::BuildRepairJob(FileInfo* fileInfo, const char* diskBasename)
{
	if (g_Options->GetDupeArticleFallback() != Options::dafStream || g_Options->GetRawArticle())
	{
		return false;
	}

	NzbInfo* nzbInfo = fileInfo->GetNzbInfo();
	if (!nzbInfo || nzbInfo->GetDeleting() || nzbInfo->GetParking() ||
		nzbInfo->GetDeleteStatus() != NzbInfo::dsNone ||
		nzbInfo->GetPostInfo() != nullptr)
	{
		// the PostInfo check: files completing AFTER post-processing started
		// (par-check unpauses par2 volumes mid-repair) must not re-arm the
		// already-drained job list for a second stream-repair pass
		return false;
	}

	// any file type qualifies: identity is decided empirically by probe
	// byte-compares, so reposts of passworded or compressed archives (and
	// their par2 files) donate exactly like bare media does
	if (fileInfo->GetSuccessArticles() == 0 ||
		fileInfo->GetDecodedFileSize() <= 0 ||
		Util::EmptyStr(diskBasename))
	{
		return false;
	}

	StreamRangeList holes = ComputeHoles(fileInfo);
	if (holes.empty())
	{
		return false;
	}

	nzbInfo->GetStreamRepairJobs()->emplace_back(fileInfo->GetId(), diskBasename,
		fileInfo->GetDecodedFileSize(), std::move(holes));

	return true;
}

std::string DupeStreamRepair::SuffixKey(const char* filename)
{
	if (Util::EmptyStr(filename))
	{
		return "";
	}

	const char* lastDot = nullptr;
	const char* prevDot = nullptr;
	for (const char* p = filename; *p; p++)
	{
		if (*p == '.')
		{
			prevDot = lastDot;
			lastDot = p;
		}
	}
	if (!lastDot)
	{
		return "";
	}

	std::string key(prevDot ? prevDot + 1 : lastDot + 1);
	for (char& ch : key)
	{
		ch = (char)tolower((unsigned char)ch);
	}
	return key;
}

std::vector<FileInfo*> DupeStreamRepair::SelectDonorCandidates(const char* targetFilename,
	int64 targetDecodedFileSize, int positionalRank, int positionalWindow,
	NzbInfo* donorNzb, int maxCandidates)
{
	if (Util::EmptyStr(targetFilename))
	{
		return {};
	}

	// the window: donor files whose nzb-declared ENCODED size is near the
	// target's decoded size (yEnc overhead is ~1-3%; div 8 tolerates ~12.5%)
	// and which still carry an article list (EstimateDonorRanges needs it)
	std::vector<FileInfo*> window;
	for (FileInfo* donorFile : donorNzb->GetFileList())
	{
		if (!donorFile->GetArticles()->empty() &&
			DupeArticleFallback::SizesMatch(donorFile->GetSize(), targetDecodedFileSize, 8))
		{
			window.push_back(donorFile);
		}
	}

	std::vector<FileInfo*> candidates;
	auto add = [&candidates, maxCandidates](FileInfo* donorFile)
	{
		if ((int)candidates.size() < maxCandidates &&
			std::find(candidates.begin(), candidates.end(), donorFile) == candidates.end())
		{
			candidates.push_back(donorFile);
		}
	};

	// 1. a repost that kept its filenames
	for (FileInfo* donorFile : window)
	{
		if (!strcasecmp(donorFile->GetFilename(), targetFilename))
		{
			add(donorFile);
		}
	}

	// 2. same suffix key, but only when it identifies EXACTLY ONE donor
	// member: volume schemes ("part03.rar", "r00", "vol07+08.par2") are
	// unique per member, while shared keys (a same-extension episode pack,
	// digit-bearing or not: "mkv", "mp4") would flood the cap in file-list
	// order and evict the better-ranked tiers below
	std::string targetKey = SuffixKey(targetFilename);
	if (!targetKey.empty())
	{
		FileInfo* keyMatch = nullptr;
		bool ambiguous = false;
		for (FileInfo* donorFile : window)
		{
			if (SuffixKey(donorFile->GetFilename()) == targetKey)
			{
				ambiguous = keyMatch != nullptr;
				keyMatch = donorFile;
			}
		}
		if (keyMatch && !ambiguous)
		{
			add(keyMatch);
		}
	}

	// 3. fully obfuscated reposts keep file count and sizes: pair the
	// rank-th window member by donor filename order, but only when the
	// window cardinality matches the target side's (the set signature)
	if (positionalRank >= 0 && positionalWindow == (int)window.size() &&
		positionalRank < (int)window.size())
	{
		std::vector<FileInfo*> byName = window;
		std::sort(byName.begin(), byName.end(),
			[](FileInfo* file1, FileInfo* file2)
			{
				return strcasecmp(file1->GetFilename(), file2->GetFilename()) < 0;
			});
		add(byName[positionalRank]);
	}

	// 4. closest encoded size (the pre-M1 heuristic; catches renamed singles)
	std::vector<FileInfo*> bySize = window;
	std::sort(bySize.begin(), bySize.end(),
		[targetDecodedFileSize](FileInfo* file1, FileInfo* file2)
		{
			int64 diff1 = file1->GetSize() > targetDecodedFileSize ?
				file1->GetSize() - targetDecodedFileSize : targetDecodedFileSize - file1->GetSize();
			int64 diff2 = file2->GetSize() > targetDecodedFileSize ?
				file2->GetSize() - targetDecodedFileSize : targetDecodedFileSize - file2->GetSize();
			return diff1 < diff2;
		});
	for (FileInfo* donorFile : bySize)
	{
		add(donorFile);
	}

	return candidates;
}

std::string DupeStreamRepair::SelectExtractedInner(const char* dir, int64 innerSize, const char* innerName)
{
	if (Util::EmptyStr(dir) || innerSize < 0)
	{
		return "";
	}

	std::vector<std::string> matches;

	fs::error_code dirEc;
	for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, dirEc);
		!dirEc && it != fs::recursive_directory_iterator();
		it.increment(dirEc))
	{
		fs::error_code fileEc;
		if (!it->is_regular_file(fileEc) || fileEc)
		{
			continue;
		}

		std::string path = fs::u8string(it->path());
		if (FileSystem::FileSize(path.c_str()) == innerSize)
		{
			matches.push_back(std::move(path));
		}
	}

	if (matches.empty())
	{
		return "";
	}

	std::sort(matches.begin(), matches.end());

	if (!Util::EmptyStr(innerName))
	{
		for (const std::string& match : matches)
		{
			std::string basename = fs::u8string(fs::path(match).filename());
			if (!strcasecmp(basename.c_str(), innerName))
			{
				return match;
			}
		}
	}

	return matches.front();
}
