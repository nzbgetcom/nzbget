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
