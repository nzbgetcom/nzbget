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


#ifndef STREAMREPAIR_H
#define STREAMREPAIR_H

#include <deque>
#include <map>
#include <set>
#include <vector>
#include "NString.h"
#include "Thread.h"
#include "DownloadInfo.h"
#include "ScriptController.h"
#include "ArticleFetcher.h"
#include "ContentMap.h"

class DiskFile;

/* target-side member files in DestDir, opened read-only on demand */
class DiskSourceSet : public ContentSourceSet
{
public:
	DiskSourceSet(const char* destDir, const std::vector<SetMember>& members) :
		m_destDir(destDir), m_members(members), m_entries(members.size()) {}
	virtual ContentSource* GetSource(int memberIndex);

private:
	struct Entry
	{
		DiskFile File;
		std::unique_ptr<DiskContentSource> Source;
		bool Tried = false;
	};

	CString m_destDir;
	const std::vector<SetMember>& m_members;
	std::deque<Entry> m_entries;
};

/* donor-side byte access to one member of a duplicate posting: articles are
 * fetched and decoded on demand, offsets resolve through the estimated
 * ranges with measured-drift correction, recent articles stay cached */
class DonorMemberSource : public ContentSource
{
public:
	DonorMemberSource(ArticleFetcher& fetcher, FileInfo* donorFile) :
		m_fetcher(fetcher), m_donorFile(donorFile) {}
	virtual int64 Size() { return EnsureInit() ? m_size : 0; }
	virtual bool Read(int64 offset, void* buffer, int64 size);
	void SetFetchBudget(int* budget) { m_fetchBudget = budget; }
	int TakeServedParts();

private:
	static constexpr int MaxCachedParts = 8;
	static constexpr int MaxResolveSteps = 6;

	ArticleFetcher& m_fetcher;
	FileInfo* m_donorFile;
	StreamRangeList m_ranges;
	std::map<int, ArticleFetcher::FetchedArticle> m_cache;
	std::deque<int> m_cacheOrder;
	std::set<int> m_servedParts;
	int64 m_size = -1;
	int64 m_drift = 0;
	int* m_fetchBudget = nullptr;
	bool m_bad = false;

	bool EnsureInit();
	const ArticleFetcher::FetchedArticle* FetchPart(int partIndex);
	const ArticleFetcher::FetchedArticle* PartForOffset(int64 offset, int& partIndex);
};

/* all members of one donor posting, lazily sourced */
class DonorSetSources : public ContentSourceSet
{
public:
	DonorSetSources(ArticleFetcher& fetcher, NzbInfo* donorNzb);
	virtual ContentSource* GetSource(int memberIndex) { return GetDonorSource(memberIndex); }
	DonorMemberSource* GetDonorSource(int memberIndex);
	std::vector<SetMember> BuildMembers();
	void SetFetchBudget(int* budget);
	int TakeServedParts();

private:
	ArticleFetcher& m_fetcher;
	std::vector<FileInfo*> m_files;
	std::vector<std::unique_ptr<DonorMemberSource>> m_sources;
	int* m_fetchBudget = nullptr;
};

/* target member files opened read-write on demand for verify and patch */
class TargetSetFiles
{
public:
	TargetSetFiles(const char* destDir, const std::vector<SetMember>& members) :
		m_destDir(destDir), m_members(members) {}
	DiskFile* GetFile(int memberIndex);

private:
	CString m_destDir;
	const std::vector<SetMember>& m_members;
	std::map<int, std::unique_ptr<DiskFile>> m_files;
};

/*
 * Post-processing stage for option <DupeArticleFallback> value "stream":
 * fills the missing byte ranges of completed media files (captured as
 * StreamRepairJobs at download completion) with bytes fetched from duplicate
 * postings of the same content, regardless of how the duplicate was split
 * into articles. Content identity is verified by byte-comparing fetched
 * donor articles against already-downloaded regions before anything is
 * written; par-check remains the backstop for whatever stays missing.
 */
class StreamRepairController : public Thread, public ScriptController
{
public:
	virtual void Run();
	virtual void Stop();
	static void StartJob(PostInfo* postInfo);

protected:
	virtual void AddMessage(Message::EKind kind, const char* text);

private:
	struct RepairTarget
	{
		int FileId;
		CString Filename;		// current on-disk base name (in DestDir)
		int64 DecodedFileSize;
		StreamRangeList Holes;
		// rank among same-size members (by name) and that window's size,
		// for positional donor pairing of obfuscated reposts; -1/0 = none
		int PositionalRank = -1;
		int PositionalWindow = 0;
	};

	struct DonorSource
	{
		CString QueuedFilename;
		CString InfoName;
		// the donor's own archive password (its *Unpack:Password parameter,
		// captured from the LIVE donor NzbInfo: re-parsing the .nzb would lose
		// passwords set at intake or via the API). A secret - never logged.
		CString Password;
	};

	PostInfo* m_postInfo;
	ArticleFetcher m_fetcher;
	// the target's own archive password (*Unpack:Password); never logged
	CString m_targetPassword;
	int m_recoveredArticles = 0;
	int64 m_recoveredBytes = 0;
	bool m_holesRemain = false;

	// a donor whose files keep failing identity verification is almost
	// certainly not a byte-identical repost: stop trying it after this many
	// consecutive unproductive files instead of burning fetches on the rest
	static constexpr int DonorFailureBail = 5;

	void CollectTargets(NzbInfo* nzbInfo, std::vector<RepairTarget>& targets,
		std::vector<CString>& memberNames);
	void ComputePositionalRanks(const char* destDir, std::vector<RepairTarget>& targets,
		const std::vector<CString>& memberNames);
	void CollectDonors(DownloadQueue* downloadQueue, NzbInfo* nzbInfo,
		std::vector<DonorSource>& donors);
	// how a donor's attempt at one file ended: only attempts that actually
	// spent fetches may advance the donor circuit breaker
	enum ERepairOutcome
	{
		roNoCost,		// nothing fetched (file unopenable, no candidates)
		roUnproductive,	// fetches spent, nothing written
		roProductive	// bytes written
	};

	void ExecRepair(const char* destDir, std::vector<RepairTarget>& targets,
		std::vector<DonorSource>& donors);
	ERepairOutcome RepairFile(const char* destDir, RepairTarget& target, NzbInfo* donorNzb,
		const char* donorName);
	static std::vector<FileInfo*> FindDonorFiles(const RepairTarget& target, NzbInfo* donorNzb);
	bool VerifyDonor(DiskFile& file, const RepairTarget& target, FileInfo* donorFile,
		const StreamRangeList& donorRanges);
	int PatchFromDonor(DiskFile& file, RepairTarget& target, FileInfo* donorFile,
		const StreamRangeList& donorRanges, const char* donorName);
	static bool CompareToFile(DiskFile& file, int64 offset, const char* data, int64 size);

	// cross-packing (M2): repairs holes through inner-content maps after
	// the per-member same-bytes pass (M1) exhausted all donors
	void ExecCrossPackRepair(const char* destDir, std::vector<RepairTarget>& targets,
		std::vector<DonorSource>& donors, const std::vector<CString>& memberNames);
	bool ReadDonorInner(ContentMap& donorMap, DonorSetSources& donorSources,
		const StreamRange& innerRange, std::vector<char>& buffer);
	static std::vector<StreamRange> BuildProbeWindows(const RepairSetData& repairSet);
	static int64 PlainCompareFloor(RepairSetData& repairSet,
		const std::vector<StreamRange>& windows);
	bool VerifyDonorSet(RepairSetData& repairSet, ContentMap& donorMap,
		DonorSetSources& donorSources, TargetSetFiles& targetFiles);
	int64 PatchFromDonorSet(RepairSetData& repairSet, ContentMap& donorMap,
		DonorSetSources& donorSources, TargetSetFiles& targetFiles,
		std::vector<RepairTarget>& targets, const std::vector<int>& memberTargets,
		const std::vector<SetMember>& setMembers, const char* donorName);

	// encrypted-target variants (M3): identity and patching in CIPHERTEXT
	// space - donor plaintext is re-encrypted under the target's stream
	// context and byte-compared against the target's disk before any write
	static bool ReadTargetCipher(ContentMap& targetMap, TargetSetFiles& targetFiles,
		const StreamRange& cipherRange, char* buffer);
	bool VerifyDonorSetEncrypted(RepairSetData& repairSet, ContentMap& donorMap,
		DonorSetSources& donorSources, TargetSetFiles& targetFiles,
		const std::vector<StreamRange>& windows);
	int64 PatchFromDonorSetEncrypted(RepairSetData& repairSet, ContentMap& donorMap,
		DonorSetSources& donorSources, TargetSetFiles& targetFiles,
		std::vector<RepairTarget>& targets, const std::vector<int>& memberTargets,
		const std::vector<SetMember>& setMembers, const char* donorName);
	int64 WriteInnerRange(ContentMap& targetMap, TargetSetFiles& targetFiles,
		std::vector<RepairTarget>& targets, const std::vector<int>& memberTargets,
		const std::vector<SetMember>& setMembers, const StreamRange& innerRange,
		const char* data);

	// decompression-assisted donor extraction (M4, option <DupeStreamDecompress>):
	// a compressed donor that cannot map for byte-copy is materialized to a
	// temp dir, extracted with the configured unrar/7z tool, and the extracted
	// inner file donates through the plain inner-space verify/patch path.
	// v1 scope: PLAINTEXT targets only; the temp tree is removed on every exit
	void ExecDecompressRepair(const char* destDir, RepairSetData& repairSet,
		NzbInfo* donorNzb, const std::vector<SetMember>& donorMembers,
		const MemberSet& donorSet, const DonorSource& donor,
		TargetSetFiles& targetFiles, std::vector<RepairTarget>& targets,
		const std::vector<int>& memberTargets, const std::vector<SetMember>& setMembers);
	bool MaterializeDonorSet(NzbInfo* donorNzb, const std::vector<SetMember>& donorMembers,
		const MemberSet& set, const char* tempDir, int64& totalBytes);
	bool VerifyDonorInnerFile(RepairSetData& repairSet, DiskFile& donorInner,
		int64 donorInnerSize, TargetSetFiles& targetFiles);
	int64 PatchFromDonorInnerFile(RepairSetData& repairSet, DiskFile& donorInner,
		TargetSetFiles& targetFiles, std::vector<RepairTarget>& targets,
		const std::vector<int>& memberTargets, const std::vector<SetMember>& setMembers,
		const char* donorName);

	void ReportRemainingHoles(std::vector<RepairTarget>& targets);

	void RepairCompleted();
};

#endif
