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
#include "DupeStreamRepair.h"
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
