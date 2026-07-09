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

uint16 GetLe16(const char* buffer)
{
	return (uint16)((uint8)buffer[0] | ((uint16)(uint8)buffer[1] << 8));
}

uint32 GetLe32(const char* buffer)
{
	return (uint32)(uint8)buffer[0] | ((uint32)(uint8)buffer[1] << 8) |
		((uint32)(uint8)buffer[2] << 16) | ((uint32)(uint8)buffer[3] << 24);
}

uint64 GetLe64(const char* buffer)
{
	return (uint64)GetLe32(buffer) | ((uint64)GetLe32(buffer + 4) << 32);
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
		case MemberSet::mfZip:
			return BuildZipMap(members, set, sources, skipReason);
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

std::unique_ptr<ContentMap> ContentMapper::BuildZipMap(const std::vector<SetMember>& members,
	const MemberSet& set, ContentSourceSet& sources, std::string& skipReason)
{
	// the logical zip stream: z01..zNN then the final .zip (set order);
	// only directory and header regions are read - data holes don't block
	std::vector<int64> memberSizes;
	std::vector<int64> diskBases;
	int64 totalSize = 0;
	for (int memberIndex : set.Members)
	{
		ContentSource* source = sources.GetSource(memberIndex);
		if (!source || source->Size() <= 0)
		{
			skipReason = "member unreadable";
			return nullptr;
		}
		memberSizes.push_back(source->Size());
		diskBases.push_back(totalSize);
		totalSize += source->Size();
	}
	CompositeSource logical(sources, set.Members, memberSizes);

	// find the end-of-central-directory record in the tail window
	int64 windowSize = std::min<int64>(totalSize, 66000);
	std::vector<char> window(windowSize);
	if (windowSize < 22 ||
		!logical.Read(totalSize - windowSize, window.data(), windowSize))
	{
		skipReason = "archive directory unreadable";
		return nullptr;
	}
	int64 eocdPos = -1;
	for (int64 i = windowSize - 22; i >= 0; i--)
	{
		if (GetLe32(window.data() + i) == 0x06054b50)
		{
			eocdPos = i;
			break;
		}
	}
	if (eocdPos < 0)
	{
		skipReason = "no zip end-of-directory record";
		return nullptr;
	}
	const char* eocd = window.data() + eocdPos;
	uint64 entryCount = GetLe16(eocd + 10);
	uint32 cdDisk = GetLe16(eocd + 6);
	uint64 cdOffset = GetLe32(eocd + 16);

	// zip64: the locator sits immediately before the EOCD
	if ((cdOffset == 0xffffffff || entryCount == 0xffff) && eocdPos >= 20 &&
		GetLe32(window.data() + eocdPos - 20) == 0x07064b50)
	{
		uint32 eocd64Disk = GetLe32(window.data() + eocdPos - 20 + 4);
		uint64 eocd64Offset = GetLe64(window.data() + eocdPos - 20 + 8);
		char eocd64[56];
		if (eocd64Disk >= diskBases.size() ||
			!logical.Read(diskBases[eocd64Disk] + (int64)eocd64Offset, eocd64, sizeof(eocd64)) ||
			GetLe32(eocd64) != 0x06064b50)
		{
			skipReason = "corrupt zip64 directory";
			return nullptr;
		}
		cdDisk = GetLe32(eocd64 + 16);
		entryCount = GetLe64(eocd64 + 32);
		cdOffset = GetLe64(eocd64 + 48);
	}
	if (cdDisk >= diskBases.size())
	{
		skipReason = "corrupt zip directory";
		return nullptr;
	}
	int64 cdPos = diskBases[cdDisk] + (int64)cdOffset;

	// walk the central directory keeping the largest (primary) entry;
	// the CD is authoritative for sizes even under data-descriptor flag 3
	struct ZipEntry
	{
		std::string Name;
		uint64 CompSize = 0;
		uint64 UncompSize = 0;
		uint64 LocalOffset = 0;
		uint32 Disk = 0;
		uint16 Method = 0;
		uint16 Flags = 0;
	};
	ZipEntry primary;
	bool found = false;

	for (uint64 entryIndex = 0; entryIndex < entryCount; entryIndex++)
	{
		char fixed[46];
		if (!logical.Read(cdPos, fixed, sizeof(fixed)) || GetLe32(fixed) != 0x02014b50)
		{
			skipReason = "corrupt zip directory";
			return nullptr;
		}
		ZipEntry entry;
		entry.Flags = GetLe16(fixed + 8);
		entry.Method = GetLe16(fixed + 10);
		entry.CompSize = GetLe32(fixed + 20);
		entry.UncompSize = GetLe32(fixed + 24);
		uint16 nameLen = GetLe16(fixed + 28);
		uint16 extraLen = GetLe16(fixed + 30);
		uint16 commentLen = GetLe16(fixed + 32);
		entry.Disk = GetLe16(fixed + 34);
		entry.LocalOffset = GetLe32(fixed + 42);

		std::vector<char> nameBuffer(nameLen);
		if (nameLen > 0 &&
			!logical.Read(cdPos + 46, nameBuffer.data(), nameLen))
		{
			skipReason = "corrupt zip directory";
			return nullptr;
		}
		entry.Name.assign(nameBuffer.data(), nameLen);

		if (extraLen > 0)
		{
			std::vector<char> extra(extraLen);
			if (!logical.Read(cdPos + 46 + nameLen, extra.data(), extraLen))
			{
				skipReason = "corrupt zip directory";
				return nullptr;
			}
			// zip64 field 0x0001: 64-bit values for the maxed markers, in order
			for (int64 fieldPos = 0; fieldPos + 4 <= extraLen; )
			{
				uint16 fieldId = GetLe16(&extra[fieldPos]);
				uint16 fieldSize = GetLe16(&extra[fieldPos + 2]);
				if (fieldId == 0x0001)
				{
					int64 valuePos = fieldPos + 4;
					int64 fieldEnd = fieldPos + 4 + fieldSize;
					if (entry.UncompSize == 0xffffffff && valuePos + 8 <= fieldEnd)
					{
						entry.UncompSize = GetLe64(&extra[valuePos]);
						valuePos += 8;
					}
					if (entry.CompSize == 0xffffffff && valuePos + 8 <= fieldEnd)
					{
						entry.CompSize = GetLe64(&extra[valuePos]);
						valuePos += 8;
					}
					if (entry.LocalOffset == 0xffffffff && valuePos + 8 <= fieldEnd)
					{
						entry.LocalOffset = GetLe64(&extra[valuePos]);
						valuePos += 8;
					}
					if (entry.Disk == 0xffff && valuePos + 4 <= fieldEnd)
					{
						entry.Disk = GetLe32(&extra[valuePos]);
					}
				}
				fieldPos += 4 + fieldSize;
			}
		}

		cdPos += 46 + nameLen + extraLen + commentLen;

		if (!found || entry.UncompSize > primary.UncompSize)
		{
			primary = std::move(entry);
			found = true;
		}
	}
	if (!found)
	{
		skipReason = "empty zip directory";
		return nullptr;
	}

	size_t slashPos = primary.Name.rfind('/');
	std::string innerName = slashPos == std::string::npos ?
		primary.Name : primary.Name.substr(slashPos + 1);

	if (primary.Method != 0)
	{
		skipReason = "compressed zip entry (only stored maps)";
		return nullptr;
	}
	if (primary.Flags & 0x0001)
	{
		skipReason = "encrypted zip entry";
		return nullptr;
	}
	if (primary.CompSize != primary.UncompSize)
	{
		skipReason = "stored entry sizes disagree";
		return nullptr;
	}
	if (!DupeStreamRepair::IsStreamEligible(innerName.c_str()))
	{
		skipReason = "inner file is not a media file";
		return nullptr;
	}
	if (primary.Disk >= diskBases.size())
	{
		skipReason = "corrupt zip directory";
		return nullptr;
	}

	// the local header names the exact data start
	int64 localPos = diskBases[primary.Disk] + (int64)primary.LocalOffset;
	char local[30];
	if (!logical.Read(localPos, local, sizeof(local)) || GetLe32(local) != 0x04034b50)
	{
		skipReason = "corrupt zip local header";
		return nullptr;
	}
	int64 dataPos = localPos + 30 + GetLe16(local + 26) + GetLe16(local + 28);
	if (dataPos + (int64)primary.UncompSize > totalSize)
	{
		skipReason = "implausible data run geometry";
		return nullptr;
	}

	std::unique_ptr<ContentMap> map = std::make_unique<ContentMap>();
	map->SetInnerName(innerName.c_str());
	map->SetInnerSize((int64)primary.UncompSize);

	int64 innerOffset = 0;
	for (const MemberRange& piece : logical.ToMembers({dataPos, (int64)primary.UncompSize}))
	{
		map->GetRuns()->push_back({innerOffset, piece.MemberIndex,
			piece.Range.Offset, piece.Range.Size});
		innerOffset += piece.Range.Size;
	}

	return map;
}
