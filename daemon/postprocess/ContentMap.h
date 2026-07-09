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


#ifndef CONTENTMAP_H
#define CONTENTMAP_H

#include <memory>
#include <string>
#include <vector>
#include "DownloadInfo.h"
#include "FileSystem.h"

/*
 * Cross-packing ("container-aware") stream repair for option
 * <DupeArticleFallback> value "stream": a ContentMap locates the canonical
 * inner content stream (the media file's bytes) inside a posting's member
 * files, so two postings that pack the SAME inner file differently (bare
 * file, store-mode rar volumes, stored zip, 7z-copy, raw splits) can donate
 * to each other. Maps are built by parsing container headers through the
 * ContentSource byte-reader interface - the donor/target symmetry point:
 * the target reads assembled files on disk, the donor fetches articles on
 * demand. Recovery still never unpacks, decompresses or decrypts anything:
 * only store/copy-mode packings are mappable, identity is proven by probe
 * byte-compares before any write, and everything unmappable stays for par2.
 */

/* all-or-nothing absolute-offset byte access to one member file;
 * false = the bytes are unavailable (target: inside a captured hole;
 * donor: article not fetchable) */
class ContentSource
{
public:
	virtual ~ContentSource() {}
	virtual int64 Size() = 0;
	virtual bool Read(int64 offset, void* buffer, int64 size) = 0;
};

/* the member files of one posting, by index into its SetMember list */
class ContentSourceSet
{
public:
	virtual ~ContentSourceSet() {}
	virtual ContentSource* GetSource(int memberIndex) = 0;
};

/* reads an already-open disk file (the target side) */
class DiskContentSource : public ContentSource
{
public:
	DiskContentSource(DiskFile& file, int64 size) : m_file(file), m_size(size) {}
	virtual int64 Size() { return m_size; }
	virtual bool Read(int64 offset, void* buffer, int64 size);

private:
	DiskFile& m_file;
	int64 m_size;
};

/* a member file of a posting; Size 0 = not known yet (donor side before
 * the first article of that member was fetched) */
struct SetMember
{
	std::string Name;
	int64 Size = 0;
};

/* member files grouped into one container set, in data order */
struct MemberSet
{
	enum EFormat
	{
		mfBare,		// a directly posted media file (identity map)
		mfSplit,	// raw splits: name.ext.001, .002, ...
		mfRar,		// rar volumes: .partNN.rar or .rar/.rNN
		mfZip,		// zip, incl. spanned .z01... + final .zip
		mfSevenZip	// 7z, incl. .7z.001 splits
	};

	EFormat Format;
	std::vector<int> Members;
};

/* one contiguous piece of the inner stream inside a member file */
struct ContentRun
{
	int64 InnerOffset;
	int MemberIndex;
	int64 MemberOffset;
	int64 Size;
	int64 InnerEnd() const { return InnerOffset + Size; }
};

struct MemberRange
{
	int MemberIndex;
	StreamRange Range;
};

/* presents ordered members as one logical byte stream (spanned zip volumes,
 * .7z.001 splits) and translates logical ranges back to member coordinates */
class CompositeSource : public ContentSource
{
public:
	CompositeSource(ContentSourceSet& sources, std::vector<int> memberIndexes,
		std::vector<int64> memberSizes);
	virtual int64 Size() { return m_totalSize; }
	virtual bool Read(int64 offset, void* buffer, int64 size);
	std::vector<MemberRange> ToMembers(const StreamRange& logicalRange) const;

private:
	ContentSourceSet& m_sources;
	std::vector<int> m_memberIndexes;
	std::vector<int64> m_memberSizes;
	std::vector<int64> m_memberBases;
	int64 m_totalSize = 0;
};

/* wraps another source, refusing reads that intersect captured holes: a
 * hole's bytes exist on disk (preallocated) but are garbage, and parsing
 * them would build maps out of noise */
class HoledSource : public ContentSource
{
public:
	HoledSource(ContentSource& inner, StreamRangeList holes) :
		m_inner(inner), m_holes(std::move(holes)) {}
	virtual int64 Size() { return m_inner.Size(); }
	virtual bool Read(int64 offset, void* buffer, int64 size);

private:
	ContentSource& m_inner;
	StreamRangeList m_holes;
};

/* per-member hole-aware view over another source set */
class HoledSourceSet : public ContentSourceSet
{
public:
	HoledSourceSet(ContentSourceSet& inner, const std::vector<StreamRangeList>& memberHoles) :
		m_inner(inner), m_memberHoles(memberHoles), m_wrapped(memberHoles.size()) {}
	virtual ContentSource* GetSource(int memberIndex);

private:
	ContentSourceSet& m_inner;
	const std::vector<StreamRangeList>& m_memberHoles;
	std::vector<std::unique_ptr<HoledSource>> m_wrapped;
};

/* where the inner content stream lives inside a member set */
class ContentMap
{
public:
	const char* GetInnerName() const { return m_innerName.c_str(); }
	void SetInnerName(const char* innerName) { m_innerName = innerName ? innerName : ""; }
	int64 GetInnerSize() const { return m_innerSize; }
	void SetInnerSize(int64 innerSize) { m_innerSize = innerSize; }
	std::vector<ContentRun>* GetRuns() { return &m_runs; }

	/* the parts of memberRange that carry inner bytes, in inner coordinates
	 * (framing inside memberRange drops out - donor-irreparable by design) */
	StreamRangeList MapToInner(int memberIndex, const StreamRange& memberRange) const;

	/* where innerRange lives in member coordinates, split across runs */
	std::vector<MemberRange> MapFromInner(const StreamRange& innerRange) const;

	/* drop all runs of one member (unreadable headers): its bytes become
	 * unmappable in both directions while the rest of the set still maps */
	void ExcludeMember(int memberIndex);

private:
	std::string m_innerName;
	int64 m_innerSize = 0;
	std::vector<ContentRun> m_runs;
};

/* one target-side set the repair stage can work on; Map is null exactly
 * when SkipReason names why the set cannot cross-map (par2 still applies) */
struct RepairSetData
{
	MemberSet Set;
	std::unique_ptr<ContentMap> Map;
	StreamRangeList InnerHoles;
	// InnerHoles as captured at build time: InnerHoles shrinks as donors
	// patch, but identity evidence must only ever anchor to bytes that were
	// NEVER a hole (primary-downloaded), so probe placement excludes these
	StreamRangeList OriginalInnerHoles;
	std::string SkipReason;
};

class ContentMapper
{
public:
	/* groups member files into container sets by their naming schemes;
	 * incomplete sets (numbering gaps) are dropped - their members stay
	 * un-mapped and par2 owns them. Bare singletons require a media
	 * extension (DupeStreamRepair::IsStreamEligible). */
	static std::vector<MemberSet> GroupSets(const std::vector<SetMember>& members);

	/* builds the inner-content map for one set; nullptr + skipReason when
	 * the set is not store/copy-mappable (M1 and par2 still apply) */
	static std::unique_ptr<ContentMap> BuildMap(const std::vector<SetMember>& members,
		const MemberSet& set, ContentSourceSet& sources, std::string& skipReason);

	/* target side: group members into sets, keep those with holed members,
	 * build maps through (hole-aware) sources and translate member holes to
	 * inner coordinates. Framing holes and holes of excluded members drop
	 * out here - par2 owns them. */
	static std::vector<RepairSetData> BuildRepairSets(const std::vector<SetMember>& members,
		const std::vector<StreamRangeList>& memberHoles, ContentSourceSet& sources);

	/* sorts by offset and merges overlapping/adjacent ranges into a disjoint
	 * ascending list: probe windows hugging neighboring holes can land on
	 * the same present island, and shared bytes must only count once */
	static StreamRangeList CoalesceRanges(std::vector<StreamRange> ranges);

private:
	static std::unique_ptr<ContentMap> BuildBareMap(const std::vector<SetMember>& members,
		const MemberSet& set, ContentSourceSet& sources, std::string& skipReason);
	static std::unique_ptr<ContentMap> BuildSplitMap(const std::vector<SetMember>& members,
		const MemberSet& set, ContentSourceSet& sources, std::string& skipReason);
	static std::unique_ptr<ContentMap> BuildRarMap(const std::vector<SetMember>& members,
		const MemberSet& set, ContentSourceSet& sources, std::string& skipReason);
	static std::unique_ptr<ContentMap> BuildZipMap(const std::vector<SetMember>& members,
		const MemberSet& set, ContentSourceSet& sources, std::string& skipReason);
	static std::unique_ptr<ContentMap> BuildSevenZipMap(const std::vector<SetMember>& members,
		const MemberSet& set, ContentSourceSet& sources, std::string& skipReason);
};

#endif
