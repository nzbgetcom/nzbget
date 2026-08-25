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

class RarCryptoContext;	// StreamCrypto.h - held by pointer only here

/* crypto annotation for a run whose member bytes are RAR-encrypted ciphertext
 * (password-assisted store-rar mapping, M3). The run lives in PLAINTEXT inner
 * space; its member bytes are the ciphertext. For an encrypted RAR the whole
 * inner file is ONE continuous AES-CBC stream that RAR splits across volumes
 * at ARBITRARY byte offsets (verified: per-volume cipher chunks are unpadded
 * and commonly non-16-aligned, only their total is padded to ceil16 of the
 * plaintext size; no per-volume re-keying), so every encrypted run of a map
 * shares ONE context. The member data areas concatenate to one contiguous
 * CIPHER SPACE in which position p carries the ciphertext byte of plaintext
 * position p: this run's slab covers cipher positions [run.InnerOffset,
 * run.InnerOffset + CipherSize) at member offsets [CipherDataOffset,
 * CipherDataOffset + CipherSize). CipherSize can exceed run.Size only in the
 * final slab (the CBC padding tail past the plaintext end). Cipher block 0
 * chains from the context's header IV, every later block from its predecessor
 * ciphertext block - which may live in the PREVIOUS member. */
struct RunCrypto
{
	std::shared_ptr<RarCryptoContext> Crypto;
	int64 CipherDataOffset = 0;
	int64 CipherSize = 0;
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

	/* crypto annotations, kept parallel to GetRuns() (index-for-index) when the
	 * map is encrypted, empty otherwise. The builder pushes one entry per run. */
	std::vector<RunCrypto>* GetRunCryptos() { return &m_runCryptos; }
	/* the crypto binding for a run, or nullptr for a plaintext run: the surface
	 * Task 4 decrypts holes through. Encrypted maps annotate every run. */
	const RunCrypto* GetRunCrypto(size_t runIndex) const
	{
		return runIndex < m_runCryptos.size() && m_runCryptos[runIndex].Crypto ?
			&m_runCryptos[runIndex] : nullptr;
	}
	/* true when any run carries ciphertext (a password-assisted store-rar map) */
	bool GetEncrypted() const { return !m_runCryptos.empty(); }
	/* total bytes of the contiguous cipher space (== ceil16(InnerSize) for a
	 * complete encrypted map), 0 for plaintext maps */
	int64 GetCipherStreamSize() const;

	/* where cipherRange (positions in the contiguous cipher space spanning all
	 * member data areas) lives in member coordinates. Fail closed: the result is
	 * empty unless every byte is covered exactly once, in order - a range
	 * touching an excluded member or the padding of a foreshortened map cannot
	 * be trusted for crypto work. Empty for plaintext maps. */
	std::vector<MemberRange> MapCipherRange(const StreamRange& cipherRange) const;

	/* assembles the raw ciphertext of cipherRange from the member sources
	 * (cross-member 16-byte blocks are the norm: RAR cuts volumes mid-block);
	 * false when any byte is unmappable or unreadable */
	bool ReadCipherRange(ContentSourceSet& sources, const StreamRange& cipherRange,
		char* buffer) const;

	/* reads innerRange of the PLAINTEXT inner stream by fetching the covering
	 * whole cipher blocks (plus the predecessor block as chain input; the
	 * header IV seeds block 0) and CBC-decrypting them. false when any needed
	 * cipher byte is unavailable - the caller must treat the range as missing */
	bool ReadInnerDecrypted(ContentSourceSet& sources, const StreamRange& innerRange,
		char* buffer) const;

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
	// parallel to m_runs when encrypted (one entry per run), empty when plain;
	// ExcludeMember keeps the two in lockstep
	std::vector<RunCrypto> m_runCryptos;
};

/* one target-side set the repair stage can work on; Map is null exactly
 * when SkipReason names why the set cannot cross-map (par2 still applies) */
struct RepairSetData
{
	MemberSet Set;
	std::unique_ptr<ContentMap> Map;
	StreamRangeList InnerHoles;
	// InnerHoles as captured at build time: InnerHoles shrinks as donors
	// patch, but identity evidence must anchor only to bytes that were no
	// longer holes when the set was built - primary-downloaded or already
	// probe-verified M1 writes - so probe placement excludes these
	StreamRangeList OriginalInnerHoles;
	std::string SkipReason;
};

class ContentMapper
{
public:
	/* M3-eligible skip reasons: a set that skipped for one of these was blocked
	 * ONLY by encryption, so the controller may retry BuildMap with a password
	 * (the M3 ladder). Defined here - where BuildRarMap SETS them - and matched
	 * by StreamRepair, so a future reword cannot silently disable donor-side M3. */
	static constexpr const char* SkipEncryptedData = "encrypted archive data";
	static constexpr const char* SkipEncryptedHeaders = "encrypted archive headers";

	/* M4 (option <DupeStreamDecompress>): true when set's format is one an
	 * extractor (unrar/7z) can decompress (mfRar, mfZip, mfSevenZip) - a
	 * candidate for the decompression-donor path. False for mfBare/mfSplit:
	 * there is no compression to undo, the store-mode paths above already
	 * handle them. */
	static bool IsCompressibleArchive(const MemberSet& set);

	/* groups member files into container sets by their naming schemes;
	 * incomplete sets (numbering gaps) are dropped - their members stay
	 * un-mapped and par2 owns them. Bare singletons require a media
	 * extension (DupeStreamRepair::IsStreamEligible). */
	static std::vector<MemberSet> GroupSets(const std::vector<SetMember>& members);

	/* builds the inner-content map for one set; nullptr + skipReason when
	 * the set is not store/copy-mappable (M1 and par2 still apply). A non-null
	 * password enables password-assisted mapping of encrypted store-rar archives
	 * (threaded to BuildRarMap only); without it encrypted archives skip as in
	 * M2. Donor call sites supply the donor's password; the target side threads
	 * its own through BuildRepairSets. */
	static std::unique_ptr<ContentMap> BuildMap(const std::vector<SetMember>& members,
		const MemberSet& set, ContentSourceSet& sources, std::string& skipReason,
		const char* password = nullptr);

	/* target side: group members into sets, keep those with holed members,
	 * build maps through (hole-aware) sources and translate member holes to
	 * inner coordinates. Framing holes and holes of excluded members drop
	 * out here - par2 owns them. A non-null password (the TARGET's own) lets
	 * encrypted store-rar target sets map; their runs carry RunCrypto. */
	static std::vector<RepairSetData> BuildRepairSets(const std::vector<SetMember>& members,
		const std::vector<StreamRangeList>& memberHoles, ContentSourceSet& sources,
		const char* password = nullptr);

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
		const MemberSet& set, ContentSourceSet& sources, std::string& skipReason,
		const char* password = nullptr);
	static std::unique_ptr<ContentMap> BuildZipMap(const std::vector<SetMember>& members,
		const MemberSet& set, ContentSourceSet& sources, std::string& skipReason);
	static std::unique_ptr<ContentMap> BuildSevenZipMap(const std::vector<SetMember>& members,
		const MemberSet& set, ContentSourceSet& sources, std::string& skipReason);
};

#endif
