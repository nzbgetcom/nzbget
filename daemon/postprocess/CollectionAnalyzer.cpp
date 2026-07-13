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
#include "CollectionAnalyzer.h"
#include "FileTypes.h"

namespace PostDownloadRenamer
{

static const uintmax_t AMBIGUOUS_COLLECTION_RATIO = 3;

std::vector<CollectionAnalyzer::FileGroup>
CollectionAnalyzer::BuildGroups(const std::vector<Candidate>& candidates)
{
	std::vector<FileGroup> groups;
	for (const Candidate& candidate : candidates)
	{
		if (FileTypes::IsSubtitleExt(candidate.ext) || FileTypes::IsSampleStem(candidate.stem)) continue;

		FileGroup* targetGroup = nullptr;
		for (FileGroup& group : groups)
		{
			if (group.parentDir == candidate.parentDir && group.extKey == candidate.extLower)
			{
				targetGroup = &group;
				break;
			}
		}

		if (!targetGroup)
		{
			groups.push_back({candidate.parentDir, candidate.extLower, 0, 0, 0, false});
			targetGroup = &groups.back();
		}

		++targetGroup->count;
		uintmax_t size = candidate.size;
		if (size > targetGroup->largest)
		{
			targetGroup->second = targetGroup->largest;
			targetGroup->largest = size;
		}
		else if (size > targetGroup->second)
		{
			targetGroup->second = size;
		}
	}

	for (FileGroup& group : groups)
	{
		// Multiple same-type files in one directory (e.g. a season pack or album tracks) have no
		// dominant release file; renaming them all to "<metaname>(N)" produces arbitrary, misleading
		// names, so the whole group is left obfuscated unless one file clearly dominates the others.
		bool isAmbiguousCollection = (group.count >= 2 && group.largest <= group.second * AMBIGUOUS_COLLECTION_RATIO);
		bool isAudio = FileTypes::IsAudioExt(group.extKey);
		group.skip = isAmbiguousCollection && !isAudio;
	}

	return groups;
}

CollectionAnalyzer::CollectionAnalyzer(const std::vector<Candidate>& candidates)
	: m_groups(BuildGroups(candidates))
{
}

bool CollectionAnalyzer::ShouldSkip(const Candidate& candidate) const
{
	bool isSub = FileTypes::IsSubtitleExt(candidate.ext);
	bool isSample = FileTypes::IsSampleStem(candidate.stem);

	if (isSub || isSample)
	{
		bool hasNonAudioGroup = false;
		bool allNonAudioSkipped = true;

		for (const FileGroup& group : m_groups)
		{
			if (group.parentDir == candidate.parentDir && !FileTypes::IsAudioExt(group.extKey))
			{
				hasNonAudioGroup = true;
				if (!group.skip)
				{
					allNonAudioSkipped = false;
				}
			}
		}

		return hasNonAudioGroup && allNonAudioSkipped;
	}

	for (const FileGroup& group : m_groups)
	{
		if (group.parentDir == candidate.parentDir && group.extKey == candidate.extLower)
		{
			return group.skip;
		}
	}

	return false;
}

}
