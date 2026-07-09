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
#include "ContentMap.h"
#include "DupeStreamRepair.h"
#include "RarReader.h"
#include "Util.h"

bool DiskContentSource::Read(int64 offset, void* buffer, int64 size)
{
	return offset >= 0 && size >= 0 && offset + size <= m_size &&
		m_file.Seek(offset) && m_file.Read(buffer, size) == size;
}

CompositeSource::CompositeSource(ContentSourceSet& sources, std::vector<int> memberIndexes,
	std::vector<int64> memberSizes) :
	m_sources(sources), m_memberIndexes(std::move(memberIndexes)),
	m_memberSizes(std::move(memberSizes))
{
	for (int64 memberSize : m_memberSizes)
	{
		m_memberBases.push_back(m_totalSize);
		m_totalSize += memberSize;
	}
}

bool CompositeSource::Read(int64 offset, void* buffer, int64 size)
{
	if (offset < 0 || size < 0 || offset + size > m_totalSize)
	{
		return false;
	}

	for (const MemberRange& piece : ToMembers({offset, size}))
	{
		ContentSource* source = m_sources.GetSource(piece.MemberIndex);
		if (!source || !source->Read(piece.Range.Offset, buffer, piece.Range.Size))
		{
			return false;
		}
		buffer = (char*)buffer + piece.Range.Size;
	}
	return true;
}

std::vector<MemberRange> CompositeSource::ToMembers(const StreamRange& logicalRange) const
{
	std::vector<MemberRange> pieces;
	for (size_t i = 0; i < m_memberIndexes.size(); i++)
	{
		int64 from = std::max(logicalRange.Offset, m_memberBases[i]);
		int64 to = std::min(logicalRange.End(), m_memberBases[i] + m_memberSizes[i]);
		if (from < to)
		{
			pieces.push_back({m_memberIndexes[i], {from - m_memberBases[i], to - from}});
		}
	}
	return pieces;
}

StreamRangeList ContentMap::MapToInner(int memberIndex, const StreamRange& memberRange) const
{
	StreamRangeList innerRanges;
	for (const ContentRun& run : m_runs)
	{
		if (run.MemberIndex != memberIndex)
		{
			continue;
		}
		int64 from = std::max(memberRange.Offset, run.MemberOffset);
		int64 to = std::min(memberRange.End(), run.MemberOffset + run.Size);
		if (from < to)
		{
			innerRanges.push_back({run.InnerOffset + (from - run.MemberOffset), to - from});
		}
	}
	return innerRanges;
}

std::vector<MemberRange> ContentMap::MapFromInner(const StreamRange& innerRange) const
{
	std::vector<MemberRange> pieces;
	for (const ContentRun& run : m_runs)
	{
		int64 from = std::max(innerRange.Offset, run.InnerOffset);
		int64 to = std::min(innerRange.End(), run.InnerEnd());
		if (from < to)
		{
			pieces.push_back({run.MemberIndex,
				{run.MemberOffset + (from - run.InnerOffset), to - from}});
		}
	}
	return pieces;
}

void ContentMap::ExcludeMember(int memberIndex)
{
	m_runs.erase(std::remove_if(m_runs.begin(), m_runs.end(),
		[memberIndex](const ContentRun& run)
		{
			return run.MemberIndex == memberIndex;
		}),
		m_runs.end());
}

namespace
{

std::string Lowered(const std::string& name)
{
	std::string lowered = name;
	for (char& ch : lowered)
	{
		ch = (char)tolower((unsigned char)ch);
	}
	return lowered;
}

bool AllDigits(const std::string& text)
{
	if (text.empty())
	{
		return false;
	}
	for (char ch : text)
	{
		if (!isdigit((unsigned char)ch))
		{
			return false;
		}
	}
	return true;
}

bool EndsWith(const std::string& name, const char* suffix)
{
	size_t suffixLen = strlen(suffix);
	return name.size() >= suffixLen &&
		name.compare(name.size() - suffixLen, suffixLen, suffix) == 0;
}

// one member name parsed against the container naming schemes
struct ParsedName
{
	enum EScheme { psNone, psRarNew, psRarOld, psRarOldFirst, psZipSpan,
		psZipFinal, psSevenSplit, psSevenSingle, psSplit };
	EScheme Scheme = psNone;
	std::string Base;	// lowercased grouping key
	int Volume = 0;		// data-order position within the scheme
};

ParsedName ParseMemberName(const std::string& name)
{
	ParsedName parsed;
	std::string lowered = Lowered(name);

	// base.partNN.rar (new rar naming, 1-based)
	if (EndsWith(lowered, ".rar"))
	{
		std::string stem = lowered.substr(0, lowered.size() - 4);
		size_t partPos = stem.rfind(".part");
		if (partPos != std::string::npos && AllDigits(stem.substr(partPos + 5)))
		{
			parsed.Scheme = ParsedName::psRarNew;
			parsed.Base = stem.substr(0, partPos);
			parsed.Volume = atoi(stem.c_str() + partPos + 5);
			return parsed;
		}
		// base.rar (old naming, first volume)
		parsed.Scheme = ParsedName::psRarOldFirst;
		parsed.Base = stem;
		parsed.Volume = 0;
		return parsed;
	}

	size_t dotPos = lowered.rfind('.');
	if (dotPos == std::string::npos || dotPos == 0)
	{
		return parsed;
	}
	std::string extension = lowered.substr(dotPos + 1);
	std::string stem = lowered.substr(0, dotPos);

	// base.rNN / base.sNN (old rar naming continuations)
	if (extension.size() == 3 && (extension[0] == 'r' || extension[0] == 's') &&
		AllDigits(extension.substr(1)))
	{
		parsed.Scheme = ParsedName::psRarOld;
		parsed.Base = stem;
		parsed.Volume = 1 + atoi(extension.c_str() + 1) + (extension[0] == 's' ? 100 : 0);
		return parsed;
	}

	// base.zip / base.zNN (spanned zip; the .zip file is the LAST volume)
	if (extension == "zip")
	{
		parsed.Scheme = ParsedName::psZipFinal;
		parsed.Base = stem;
		return parsed;
	}
	if (extension.size() >= 3 && extension[0] == 'z' && AllDigits(extension.substr(1)))
	{
		parsed.Scheme = ParsedName::psZipSpan;
		parsed.Base = stem;
		parsed.Volume = atoi(extension.c_str() + 1);
		return parsed;
	}

	// base.7z / base.7z.NNN
	if (extension == "7z")
	{
		parsed.Scheme = ParsedName::psSevenSingle;
		parsed.Base = stem;
		return parsed;
	}
	if (AllDigits(extension) && extension.size() >= 2 && extension.size() <= 4)
	{
		if (EndsWith(stem, ".7z"))
		{
			parsed.Scheme = ParsedName::psSevenSplit;
			parsed.Base = stem.substr(0, stem.size() - 3);
			parsed.Volume = atoi(extension.c_str());
			return parsed;
		}
		// raw splits require the stem to still carry an extension
		if (stem.rfind('.') != std::string::npos)
		{
			parsed.Scheme = ParsedName::psSplit;
			parsed.Base = stem;
			parsed.Volume = atoi(extension.c_str());
			return parsed;
		}
	}

	return parsed;
}

struct SetBucket
{
	MemberSet::EFormat Format;
	int FirstMember;	// listing position of the earliest member (set order)
	std::vector<std::pair<int, int>> Volumes;	// (volume, memberIndex)
	bool HasFinal = false;	// old-naming .rar seen / spanned .zip seen
	int FinalMember = -1;
};

}

std::vector<MemberSet> ContentMapper::GroupSets(const std::vector<SetMember>& members)
{
	// bucket keys: scheme family + lowercased base name
	std::vector<std::pair<std::string, SetBucket>> buckets;
	auto bucketFor = [&buckets](const std::string& key, MemberSet::EFormat format,
		int memberIndex) -> SetBucket&
	{
		for (std::pair<std::string, SetBucket>& entry : buckets)
		{
			if (entry.first == key)
			{
				return entry.second;
			}
		}
		buckets.emplace_back(key, SetBucket{format, memberIndex, {}, false, -1});
		return buckets.back().second;
	};

	std::vector<bool> consumed(members.size(), false);

	for (size_t i = 0; i < members.size(); i++)
	{
		ParsedName parsed = ParseMemberName(members[i].Name);
		switch (parsed.Scheme)
		{
			case ParsedName::psRarNew:
				bucketFor("rarnew:" + parsed.Base, MemberSet::mfRar, (int)i)
					.Volumes.emplace_back(parsed.Volume, (int)i);
				consumed[i] = true;
				break;

			case ParsedName::psRarOldFirst:
			{
				SetBucket& bucket = bucketFor("rarold:" + parsed.Base, MemberSet::mfRar, (int)i);
				bucket.HasFinal = true;	// the .rar volume leads the old naming
				bucket.Volumes.emplace_back(0, (int)i);
				consumed[i] = true;
				break;
			}

			case ParsedName::psRarOld:
				bucketFor("rarold:" + parsed.Base, MemberSet::mfRar, (int)i)
					.Volumes.emplace_back(parsed.Volume, (int)i);
				consumed[i] = true;
				break;

			case ParsedName::psZipSpan:
				bucketFor("zip:" + parsed.Base, MemberSet::mfZip, (int)i)
					.Volumes.emplace_back(parsed.Volume, (int)i);
				consumed[i] = true;
				break;

			case ParsedName::psZipFinal:
			{
				SetBucket& bucket = bucketFor("zip:" + parsed.Base, MemberSet::mfZip, (int)i);
				bucket.HasFinal = true;
				bucket.FinalMember = (int)i;
				consumed[i] = true;
				break;
			}

			case ParsedName::psSevenSingle:
				bucketFor("7z:" + parsed.Base, MemberSet::mfSevenZip, (int)i)
					.Volumes.emplace_back(1, (int)i);
				consumed[i] = true;
				break;

			case ParsedName::psSevenSplit:
				bucketFor("7zsplit:" + parsed.Base, MemberSet::mfSevenZip, (int)i)
					.Volumes.emplace_back(parsed.Volume, (int)i);
				consumed[i] = true;
				break;

			case ParsedName::psSplit:
				bucketFor("split:" + parsed.Base, MemberSet::mfSplit, (int)i)
					.Volumes.emplace_back(parsed.Volume, (int)i);
				consumed[i] = true;
				break;

			case ParsedName::psNone:
				break;
		}
	}

	std::vector<MemberSet> sets;

	for (std::pair<std::string, SetBucket>& entry : buckets)
	{
		SetBucket& bucket = entry.second;
		std::sort(bucket.Volumes.begin(), bucket.Volumes.end());

		// contiguity: old-naming rar starts at 0 (the .rar), everything else
		// at its scheme's first number; gaps drop the whole set
		bool contiguous = true;
		for (size_t i = 1; i < bucket.Volumes.size(); i++)
		{
			contiguous &= bucket.Volumes[i].first == bucket.Volumes[i - 1].first + 1;
		}
		bool complete = contiguous && !bucket.Volumes.empty() &&
			(bucket.Format != MemberSet::mfRar ||
				(entry.first.compare(0, 7, "rarold:") == 0 ?
					bucket.HasFinal && bucket.Volumes[0].first == 0 :
					bucket.Volumes[0].first == 1)) &&
			(bucket.Format != MemberSet::mfSplit || bucket.Volumes[0].first == 1) &&
			(bucket.Format != MemberSet::mfSevenZip || bucket.Volumes[0].first == 1);

		// a spanned zip needs its final .zip; a lone .zip is a set of one
		if (bucket.Format == MemberSet::mfZip)
		{
			complete = bucket.HasFinal &&
				(bucket.Volumes.empty() ||
					(contiguous && bucket.Volumes[0].first == 1));
		}

		if (!complete)
		{
			continue;
		}

		MemberSet set;
		set.Format = bucket.Format;
		for (std::pair<int, int>& volume : bucket.Volumes)
		{
			set.Members.push_back(volume.second);
		}
		if (bucket.Format == MemberSet::mfZip)
		{
			set.Members.push_back(bucket.FinalMember);	// data order: zNN..., .zip last
		}
		sets.push_back(std::move(set));
	}

	// bare media singletons from whatever no scheme consumed
	for (size_t i = 0; i < members.size(); i++)
	{
		if (!consumed[i] && DupeStreamRepair::IsStreamEligible(members[i].Name.c_str()))
		{
			MemberSet set;
			set.Format = MemberSet::mfBare;
			set.Members.push_back((int)i);
			sets.push_back(std::move(set));
		}
	}

	// deterministic order: by the listing position of each set's first member
	std::sort(sets.begin(), sets.end(),
		[](const MemberSet& set1, const MemberSet& set2)
		{
			return *std::min_element(set1.Members.begin(), set1.Members.end()) <
				*std::min_element(set2.Members.begin(), set2.Members.end());
		});

	return sets;
}

std::unique_ptr<ContentMap> ContentMapper::BuildMap(const std::vector<SetMember>& members,
	const MemberSet& set, ContentSourceSet& sources, std::string& skipReason)
{
	skipReason.clear();
	switch (set.Format)
	{
		case MemberSet::mfBare:
			return BuildBareMap(members, set, sources, skipReason);
		case MemberSet::mfSplit:
			return BuildSplitMap(members, set, sources, skipReason);
		case MemberSet::mfRar:
			return BuildRarMap(members, set, sources, skipReason);
		default:
			skipReason = "format mapper not implemented";
			return nullptr;
	}
}

std::unique_ptr<ContentMap> ContentMapper::BuildBareMap(const std::vector<SetMember>& members,
	const MemberSet& set, ContentSourceSet& sources, std::string& skipReason)
{
	int memberIndex = set.Members[0];
	ContentSource* source = sources.GetSource(memberIndex);
	if (!source || source->Size() <= 0)
	{
		skipReason = "member unreadable";
		return nullptr;
	}

	std::unique_ptr<ContentMap> map = std::make_unique<ContentMap>();
	map->SetInnerName(members[memberIndex].Name.c_str());
	map->SetInnerSize(source->Size());
	map->GetRuns()->push_back({0, memberIndex, 0, source->Size()});
	return map;
}

std::unique_ptr<ContentMap> ContentMapper::BuildSplitMap(const std::vector<SetMember>& members,
	const MemberSet& set, ContentSourceSet& sources, std::string& skipReason)
{
	// inner name: the shared stem without the numeric suffix
	const std::string& firstName = members[set.Members[0]].Name;
	std::string innerName = firstName.substr(0, firstName.rfind('.'));
	if (!DupeStreamRepair::IsStreamEligible(innerName.c_str()))
	{
		skipReason = "inner file is not a media file";
		return nullptr;
	}

	std::unique_ptr<ContentMap> map = std::make_unique<ContentMap>();
	map->SetInnerName(innerName.c_str());

	int64 innerOffset = 0;
	for (int memberIndex : set.Members)
	{
		ContentSource* source = sources.GetSource(memberIndex);
		if (!source || source->Size() <= 0)
		{
			// every member's size shapes the offsets after it - unlike rar
			// volumes, a split set with an unreadable member cannot map at all
			skipReason = std::string("member ") + members[memberIndex].Name + " unreadable";
			return nullptr;
		}
		map->GetRuns()->push_back({innerOffset, memberIndex, 0, source->Size()});
		innerOffset += source->Size();
	}
	map->SetInnerSize(innerOffset);
	return map;
}

std::unique_ptr<ContentMap> ContentMapper::BuildRarMap(const std::vector<SetMember>& members,
	const MemberSet& set, ContentSourceSet& sources, std::string& skipReason)
{
	// parse every volume's headers; at most ONE unreadable volume is carried
	// as an unknown whose packed size is inferred from the inner size, so
	// the rest of the set still maps (its own holes stay for par2)
	std::vector<std::unique_ptr<RarVolume>> volumes(set.Members.size());
	int unknownVolume = -1;

	for (size_t i = 0; i < set.Members.size(); i++)
	{
		int memberIndex = set.Members[i];
		ContentSource* source = sources.GetSource(memberIndex);
		std::unique_ptr<RarVolume> volume =
			std::make_unique<RarVolume>(members[memberIndex].Name.c_str());
		if (source && volume->ReadFrom(*source))
		{
			volumes[i] = std::move(volume);
			continue;
		}
		if (source && volume->GetEncrypted())
		{
			skipReason = "encrypted archive headers";
			return nullptr;
		}
		if (unknownVolume >= 0)
		{
			skipReason = "unreadable headers in more than one volume";
			return nullptr;
		}
		unknownVolume = (int)i;
	}

	// the primary inner file: largest entry of the first readable volume
	RarFile* primary = nullptr;
	for (std::unique_ptr<RarVolume>& volume : volumes)
	{
		if (!volume)
		{
			continue;
		}
		for (RarFile& innerFile : *volume->GetFiles())
		{
			if (!primary || innerFile.GetSize() > primary->GetSize())
			{
				primary = &innerFile;
			}
		}
		break;
	}
	if (!primary)
	{
		skipReason = "no file entries in the first readable volume";
		return nullptr;
	}

	std::string innerName = FileSystem::BaseFileName(primary->GetFilename());
	int64 innerSize = primary->GetSize();
	if (!DupeStreamRepair::IsStreamEligible(innerName.c_str()))
	{
		skipReason = "inner file is not a media file";
		return nullptr;
	}

	struct VolumeRun
	{
		int64 PackedSize = 0;
		int64 DataOffset = -1;	// -1 = no mappable data in this volume
	};
	std::vector<VolumeRun> runs(set.Members.size());
	int64 knownPacked = 0;

	for (size_t i = 0; i < set.Members.size(); i++)
	{
		if (!volumes[i])
		{
			continue;
		}
		for (RarFile& innerFile : *volumes[i]->GetFiles())
		{
			if (strcasecmp(FileSystem::BaseFileName(innerFile.GetFilename()),
				innerName.c_str()))
			{
				continue;	// other inner files (nfo, srt) stay unmapped
			}
			if (!innerFile.GetStored())
			{
				skipReason = "compressed archive (only store mode is mappable)";
				return nullptr;
			}
			if (innerFile.GetEncryptedData())
			{
				skipReason = "encrypted archive data";
				return nullptr;
			}
			if (innerFile.GetSize() != innerSize)
			{
				skipReason = "inconsistent inner file size across volumes";
				return nullptr;
			}
			ContentSource* source = sources.GetSource(set.Members[i]);
			if (innerFile.GetDataOffset() < 0 || innerFile.GetPackedSize() < 0 ||
				!source || innerFile.GetDataOffset() + innerFile.GetPackedSize() > source->Size())
			{
				skipReason = "implausible data run geometry";
				return nullptr;
			}
			runs[i] = {innerFile.GetPackedSize(), innerFile.GetDataOffset()};
			knownPacked += innerFile.GetPackedSize();
		}
	}

	// store mode means the packed bytes ARE the inner bytes: exact sum
	// required (this is the strong gate even if a method byte lied)
	if (unknownVolume >= 0)
	{
		int64 inferred = innerSize - knownPacked;
		if (inferred < 0)
		{
			skipReason = "packed sizes exceed the inner file size";
			return nullptr;
		}
		runs[unknownVolume] = {inferred, -1};
	}
	else if (knownPacked != innerSize)
	{
		skipReason = "packed sizes do not sum to the inner file size";
		return nullptr;
	}

	std::unique_ptr<ContentMap> map = std::make_unique<ContentMap>();
	map->SetInnerName(innerName.c_str());
	map->SetInnerSize(innerSize);

	int64 innerOffset = 0;
	for (size_t i = 0; i < set.Members.size(); i++)
	{
		if (runs[i].DataOffset >= 0 && runs[i].PackedSize > 0)
		{
			map->GetRuns()->push_back({innerOffset, set.Members[i],
				runs[i].DataOffset, runs[i].PackedSize});
		}
		innerOffset += runs[i].PackedSize;
	}

	return map;
}
