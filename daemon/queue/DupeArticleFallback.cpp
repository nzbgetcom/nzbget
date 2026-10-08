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
#include <map>
#include <set>
#include "DupeArticleFallback.h"
#include "DupeCoordinator.h"
#include "NzbFile.h"
#include "Options.h"
#include "DiskState.h"
#include "Log.h"
#include "Util.h"
#include "FileSystem.h"
#include <fstream>
#ifndef DISABLE_PARCHECK
#include <par2/libpar2.h>
#include <par2/md5.h>
#endif

bool DupeArticleFallback::IsParFile(FileInfo* fileInfo)
{
	return fileInfo->GetParFile() ||
		(fileInfo->GetFilenameConfirmed() &&
		 Util::EndsWith(fileInfo->GetFilename(), ".par2", false));
}

bool DupeArticleFallback::HasPar2(NzbInfo* nzbInfo)
{
	if (nzbInfo->GetParSize() > 0)
	{
		return true;
	}
	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		if (IsParFile(fileInfo))
		{
			return true;
		}
	}
	for (CompletedFile& completedFile : nzbInfo->GetCompletedFiles())
	{
		if (completedFile.GetParFile() || Util::EndsWith(completedFile.GetFilename(), ".par2", false))
		{
			return true;
		}
	}
	return false;
}

bool DupeArticleFallback::ShouldDeferToPar(NzbInfo* nzbInfo)
{
	if (!nzbInfo)
	{
		return false;
	}
	if (nzbInfo->GetDirectRenameStatus() == NzbInfo::tsRunning && !nzbInfo->GetAllFirst())
	{
		// Obfuscated parity is not recognizable until the first article of
		// every file was tried (that is when its par2 header is detected).
		// The rename status itself stays "running" for the whole download
		// whenever any first article is missing, so it cannot end the wait.
		return true;
	}
	if (nzbInfo->GetParStatus() == NzbInfo::psFailure)
	{
		return false;
	}
	if (nzbInfo->GetParSize() > 0)
	{
		return true;
	}
	for (FileInfo* fileInfo : nzbInfo->GetFileList())
	{
		if (!fileInfo->GetDeleted() && IsParFile(fileInfo))
		{
			return true;
		}
	}
	for (CompletedFile& fileInfo : nzbInfo->GetCompletedFiles())
	{
		if (fileInfo.GetParFile() || Util::EndsWith(fileInfo.GetFilename(), ".par2", false))
		{
			return true;
		}
	}
	return false;
}

bool DupeArticleFallback::ParCannotCover(NzbInfo* nzbInfo, bool withMargin)
{
	// Damaged data bytes beyond every recovery byte the collection has (minus
	// its own failed parity) can never be repaired by its par2 set; waiting
	// for par-check would only forfeit the download-time recovery. Block
	// granularity makes the real need larger still, so this is conservative.
	int64 parAvailable = nzbInfo->GetParSize() - nzbInfo->GetParCurrentFailedSize();
	int64 dataFailed = nzbInfo->GetCurrentFailedSize() - nzbInfo->GetParCurrentFailedSize();
	if (dataFailed <= 0)
	{
		return false;
	}
	if (dataFailed > parAvailable)
	{
		return true;
	}

	// what the data tried so far projects (B59: Dark Matter S02E05 lost 21% of its
	// data, its par2 covered 20%; the failures outgrew the par2 data only at the end)
	int tried = nzbInfo->GetCurrentSuccessArticles() + nzbInfo->GetCurrentFailedArticles();
	int64 dataTotal = nzbInfo->GetSize() - nzbInfo->GetParSize();
	int64 dataTried = nzbInfo->GetCurrentSuccessSize() - nzbInfo->GetParCurrentSuccessSize() + dataFailed;
	if (tried < ProjectionSample || dataTried <= 0 || dataTotal <= 0)
	{
		return false;
	}
	double projectedFailed = (double)dataFailed * dataTotal / dataTried;
	return projectedFailed + (withMargin ? (double)dataTotal * ParEdgeMargin / 1000 : 0) > parAvailable;
}

bool DupeArticleFallback::TryFallback(DownloadQueue* downloadQueue, FileInfo* fileInfo, ArticleInfo* articleInfo)
{
	if (g_Options->GetDupeArticleFallback() == Options::dafNone || g_Options->GetRawArticle() ||
		IsParFile(fileInfo))
	{
		return false;
	}

	NzbInfo* nzbInfo = fileInfo->GetNzbInfo();
	if (fileInfo->GetDeleted() || nzbInfo->GetDeleting() || nzbInfo->GetParking() ||
		nzbInfo->GetDeleteStatus() != NzbInfo::dsNone || nzbInfo->GetDupeMode() == dmForce)
	{
		return false;
	}

	// A borrowed article proves only that it arrived intact (its yEnc crc), not that
	// it is this file's: a duplicate of the same size and article layout but other
	// bytes (another encode) filled the holes, and the download ended SUCCESS with
	// the wrong bytes. Its par2 files are what checks borrowed bytes (BorrowedPar2-
	// Mismatches, par-check): without any, holes are left to stream repair, which
	// proves a duplicate's bytes before it copies them
	if (!HasPar2(nzbInfo))
	{
		if (!nzbInfo->GetDupeNoParNoted())
		{
			nzbInfo->SetDupeNoParNoted(true);
			nzbInfo->PrintMessage(Message::mkInfo,
				"Not borrowing articles of %s from duplicates: it has no par2 files to check them",
				nzbInfo->GetName());
		}
		return false;
	}

	if (g_Options->GetDupeArticleFallback() >= Options::dafStream)
	{
		// The first article identifies its file (the 16k hash direct-rename
		// and par-rename go by, the archive headers) - losing it costs far
		// more than the single donor fetch that can recover it.
		// Once the damage outgrows every recovery byte the collection has,
		// single articles are borrowed again (whole-file stream recovery
		// keeps waiting for par-check, which knows which files it protects).
		// Once lifted, the wait stays lifted: a projection can fall back below the
		// par2 data (articles fetched from a lead duplicate count as arrived).
		bool parDefer = ShouldDeferToPar(nzbInfo);
		bool defer = parDefer && articleInfo->GetPartNumber() != 1 &&
			nzbInfo->GetDupeParDeferState() != NzbInfo::dpLifted && !ParCannotCover(nzbInfo);
		// say once per collection which way the par-first rule went, so a
		// download that borrowed nothing can be told apart from one that tried
		if (defer && nzbInfo->GetDupeParDeferState() == NzbInfo::dpNone &&
			downloadQueue && !CollectDonors(downloadQueue, nzbInfo).empty())
		{
			nzbInfo->SetDupeParDeferState(NzbInfo::dpDeferred);
			nzbInfo->PrintMessage(Message::mkInfo,
				"Deferring duplicate recovery for %s to par-check (its par2 files may cover the damage)",
				nzbInfo->GetName());
		}
		else if (parDefer && !defer && articleInfo->GetPartNumber() != 1 &&
			nzbInfo->GetDupeParDeferState() == NzbInfo::dpDeferred)
		{
			nzbInfo->SetDupeParDeferState(NzbInfo::dpLifted);
			nzbInfo->PrintMessage(Message::mkInfo,
				"Damage of %s exceeds its par2 recovery data (or will, judged by the articles tried so far), "
				"recovering missing articles from duplicates",
				nzbInfo->GetName());
		}
		if (defer)
		{
			return false;
		}
	}

	// preserve the article's own (primary) message-id before the first
	// substitution, so it can be used as a revert source when cut over
	if (Util::EmptyStr(articleInfo->GetDupeOriginalMessageId()))
	{
		articleInfo->SetDupeOriginalMessageId(articleInfo->GetMessageId());
	}

	int round = articleInfo->GetDupeFallbackRound();
	if (round == 0)
	{
		// the article's source order is decided once, at its first fallback:
		// the donors known now, rotated to the file's current lead, pinned on
		// the article so no later queue/history change or lead rotation can
		// shift the round->source mapping under it (which would skip or
		// repeat sources). The trade-off is deliberate: a duplicate appearing
		// later serves fresh articles, not articles already mid-fallback.
		PinSources(downloadQueue, fileInfo, articleInfo);
	}
	else if (round == 1 && RegisterLeadFailure(fileInfo, articleInfo))
	{
		nzbInfo->PrintMessage(Message::mkInfo,
			"Switching lead duplicate collection for %s (the lead duplicate is missing many articles)",
			fileInfo->GetFilename());
	}

	std::vector<CString>& sources = *articleInfo->GetDupeSources();
	// sources of a duplicate whose articles failed the par2 checksums are passed over (B40)
	std::vector<int>& sourceDonors = *articleInfo->GetDupeSourceDonors();
	int pinnedRound = round;
	while (round < (int)sources.size() && round < (int)sourceDonors.size() &&
		nzbInfo->GetDupeBlockedDonors()->count(sourceDonors[round]))
	{
		round++;
	}
	if (round >= (int)sources.size())
	{
		if (pinnedRound == 0)
		{
			// no duplicate carries this article's file at all
			nzbInfo->SetDupeUnsourcedArticles(nzbInfo->GetDupeUnsourcedArticles() + 1);
		}
		else if (!Util::EmptyStr(articleInfo->GetDupeOriginalMessageId()))
		{
			// every source was tried: the article is the release's own again
			articleInfo->SetMessageId(articleInfo->GetDupeOriginalMessageId());
		}
		return false;
	}

	// count this article once, the first time a duplicate source is tried for it
	// (the denominator of the per-file recovered/attempted completion summary).
	// No per-attempt log line: it would flood large files and, worse, read as a
	// success when it is only an attempt - the recovery is counted on success.
	// A duplicate asked first after the cutover isn't an attempt on a missing article:
	// counted, it read as "recovered 11 of 2853 missing" for a file missing ~20
	if (pinnedRound == 0 && !articleInfo->GetDupeProactive())
	{
		fileInfo->SetDupeAttemptedArticles(fileInfo->GetDupeAttemptedArticles() + 1);
		nzbInfo->SetDupeAttemptedArticles(nzbInfo->GetDupeAttemptedArticles() + 1);
	}

	// pin the decoded byte range this article must occupy, as far as it is
	// already known from finished neighbour articles, so a donor article with
	// drifted decoded boundaries is rejected before its bytes are written
	articleInfo->SetDupeExpectedOffset(ExpectedSegmentOffset(fileInfo, articleInfo));
	articleInfo->SetDupeExpectedEnd(ExpectedSegmentEnd(fileInfo, articleInfo));

	articleInfo->SetMessageId(sources[round]);
	articleInfo->SetDupeFallbackRound(round + 1);

	return true;
}

std::vector<CString> DupeArticleFallback::OrderSources(const std::vector<CString>& donorCandidates,
	bool cutover, const char* primaryMessageId)
{
	std::vector<CString> sources;

	if (!cutover || donorCandidates.empty())
	{
		// reactive: the primary was already tried via the normal path; offer
		// the donor candidates only (empty if none, so the article just fails)
		for (const CString& candidate : donorCandidates)
		{
			sources.emplace_back((const char*)candidate);
		}
		return sources;
	}

	// cut over: lead with the top donor, then the remaining donors, then the
	// primary as the final fallback. The file cut over precisely because the
	// primary is missing many articles, so on a lead-donor miss the other
	// duplicates are better bets than paying a (likely failing) full server
	// sweep on the primary; the primary still backstops parts every duplicate
	// misses. NOTE: proactive fetches must never count as "recovered"
	// regardless of round (see QueueCoordinator::ArticleCompleted) - with the
	// primary last, no donor round proves the primary was missing the part
	sources.reserve(donorCandidates.size() + 1);
	for (const CString& candidate : donorCandidates)
	{
		sources.emplace_back((const char*)candidate);
	}
	sources.emplace_back(primaryMessageId);
	return sources;
}

void DupeArticleFallback::RotateToLead(RawNzbList& donors, int leadNzbId)
{
	if (leadNzbId == 0)
	{
		return;
	}

	RawNzbList::iterator lead = std::find_if(donors.begin(), donors.end(),
		[leadNzbId](NzbInfo* donor) { return donor->GetId() == leadNzbId; });
	if (lead != donors.end())
	{
		std::rotate(donors.begin(), lead, donors.end());
	}
}

namespace
{

// the block checksums par2 records for one file (packets Main, FileDesc, IFSC)
struct Par2FileSums
{
	std::string hash16k;	// hex
	std::string name;
	uint64 length = 0;
	std::vector<uint32> crcs;
	std::string setId;		// hex: the recovery set the file belongs to
	uint64 blockSize = 0;	// that set's block size
};

uint64 ReadLe64(const uchar* p)
{
	uint64 value = 0;
	for (int i = 7; i >= 0; i--)
	{
		value = (value << 8) | p[i];
	}
	return value;
}

uint32 ReadLe32(const uchar* p)
{
	return (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24);
}

std::string Hex(const uchar* p, int len)
{
	static const char* digits = "0123456789abcdef";
	std::string hex;
	for (int i = 0; i < len; i++)
	{
		hex += digits[p[i] >> 4];
		hex += digits[p[i] & 15];
	}
	return hex;
}

// the par2 data of every par2 file in <dir> (recognized by its packet magic, so
// obfuscated names count too); recovery packets are skipped, not read
std::map<std::string, Par2FileSums> LoadPar2Sums(const char* dir, uint64& blockSize)
{
	std::map<std::string, Par2FileSums> files;
	// every set's own block size: a folder can hold several par2 sets (a season
	// pack, one per episode), and one block size for all of them judged a file
	// with another set's blocks (B40 rejected right borrowed articles, or let
	// wrong ones through)
	std::map<std::string, uint64> setBlockSizes;
	blockSize = 0;
	DirBrowser browser(dir);
	while (const char* filename = browser.Next())
	{
		BString<1024> path("%s%c%s", dir, PATH_SEPARATOR, filename);
		std::ifstream in(fs::u8path(*path), std::ios::binary);
		uchar header[64];
		uint64 pos = 0;
		while (in.seekg((std::streamoff)pos) && in.read((char*)header, 64))
		{
			if (memcmp(header, "PAR2\0PKT", 8))
			{
				break;
			}
			uint64 length = ReadLe64(header + 8);
			if (length < 64 || length % 4)
			{
				break;
			}
			const uchar* type = header + 48;
			std::string setId = Hex(header + 32, 16);
			uint64 bodyLength = length - 64;
			bool main = !memcmp(type, "PAR 2.0\0Main\0\0\0\0", 16);
			bool desc = !memcmp(type, "PAR 2.0\0FileDesc", 16);
			bool ifsc = !memcmp(type, "PAR 2.0\0IFSC\0\0\0\0", 16);
			if ((main || desc || ifsc) && bodyLength <= 64 * 1024 * 1024)
			{
				std::vector<uchar> body((size_t)bodyLength);
				if (!in.read((char*)body.data(), (std::streamsize)bodyLength))
				{
					break;
				}
				if (main && bodyLength >= 8)
				{
					blockSize = ReadLe64(body.data());
					setBlockSizes[setId] = blockSize;
				}
				else if (desc && bodyLength >= 56)
				{
					Par2FileSums& sums = files[Hex(body.data(), 16)];
					sums.setId = setId;
					sums.hash16k = Hex(body.data() + 32, 16);
					sums.length = ReadLe64(body.data() + 48);
					sums.name.assign((const char*)body.data() + 56, (size_t)bodyLength - 56);
					sums.name = sums.name.substr(0, sums.name.find('\0'));
				}
				else if (ifsc && bodyLength >= 16)
				{
					Par2FileSums& sums = files[Hex(body.data(), 16)];
					sums.setId = setId;
					sums.crcs.clear();
					for (uint64 at = 16; at + 20 <= bodyLength; at += 20)
					{
						sums.crcs.push_back(ReadLe32(body.data() + at + 16));
					}
				}
			}
			pos += length;
		}
	}
	for (auto& entry : files)
	{
		auto it = setBlockSizes.find(entry.second.setId);
		entry.second.blockSize = it != setBlockSizes.end() ? it->second : 0;
	}
	return files;
}

std::string Hash16kOf(const char* path)
{
#ifndef DISABLE_PARCHECK
	std::ifstream in(fs::u8path(path), std::ios::binary);
	std::vector<char> buffer(16 * 1024);
	in.read(buffer.data(), (std::streamsize)buffer.size());
	Par2::MD5Context context;
	context.Update(buffer.data(), (size_t)in.gcount());
	Par2::MD5Hash hash;
	context.Final(hash);
	return hash.print();
#else
	return "";
#endif
}

}

std::vector<ArticleInfo*> DupeArticleFallback::BorrowedPar2Mismatches(FileInfo* fileInfo, const char* path)
{
	std::vector<ArticleInfo*> mismatches;
	ArticleList* articles = fileInfo->GetArticles();
	if (std::none_of(articles->begin(), articles->end(), [](std::unique_ptr<ArticleInfo>& article)
		{ return article->GetStatus() == ArticleInfo::aiFinished && article->GetDupeDonorId() > 0; }))
	{
		return mismatches;
	}

	uint64 blockSize = 0;
	std::map<std::string, Par2FileSums> files = LoadPar2Sums(fileInfo->GetNzbInfo()->GetDestDir(), blockSize);
	if (files.empty())
	{
		return mismatches;
	}
	int64 fileSize = FileSystem::FileSize(path);
	std::string hash16k = Hash16kOf(path);
	const Par2FileSums* sums = nullptr;
	for (auto& entry : files)
	{
		if ((int64)entry.second.length == fileSize && !entry.second.crcs.empty() &&
			(!strcasecmp(entry.second.hash16k.c_str(), hash16k.c_str()) ||
			 !strcmp(entry.second.name.c_str(), fileInfo->GetFilename())))
		{
			sums = &entry.second;
			break;
		}
	}
	// the block size of the file's own set
	blockSize = sums ? sums->blockSize : 0;
	// a par2 block size no real set uses (a damaged packet) isn't trusted: the buffer
	// for it would be too large to allocate
	if (!sums || blockSize == 0 || blockSize % 4 != 0 || blockSize > (uint64)MaxPar2BlockSize ||
		sums->crcs.size() < (size_t)((fileSize + blockSize - 1) / blockSize))
	{
		return mismatches;
	}

	// the byte ranges of the file that arrived: a block is only judged when every
	// byte of it did (a missing article's zeros would fail any block)
	std::vector<std::pair<int64, int64>> arrived;
	for (ArticleInfo* article : articles)
	{
		if (article->GetStatus() == ArticleInfo::aiFinished && article->GetSegmentSize() > 0)
		{
			arrived.emplace_back(article->GetSegmentOffset(), article->GetSegmentOffset() + article->GetSegmentSize());
		}
	}
	std::sort(arrived.begin(), arrived.end());
	auto covered = [&arrived](int64 from, int64 to)
		{
			for (const auto& range : arrived)
			{
				if (range.first > from)
				{
					return false;
				}
				from = std::max(from, range.second);
				if (from >= to)
				{
					return true;
				}
			}
			return from >= to;
		};

	std::ifstream in(fs::u8path(path), std::ios::binary);
	std::vector<char> buffer((size_t)blockSize);
	std::map<int64, bool> verdicts;	// block -> its bytes match
	for (ArticleInfo* article : articles)
	{
		if (article->GetStatus() != ArticleInfo::aiFinished || article->GetDupeDonorId() <= 0 ||
			article->GetSegmentSize() <= 0)
		{
			continue;
		}
		int64 first = article->GetSegmentOffset() / (int64)blockSize;
		int64 last = (article->GetSegmentOffset() + article->GetSegmentSize() - 1) / (int64)blockSize;
		bool bad = false;
		for (int64 block = first; block <= last && !bad; block++)
		{
			int64 from = block * (int64)blockSize;
			int64 to = std::min(from + (int64)blockSize, fileSize);
			// a segment past the end of the file (a misplaced article) has no block
			if (from >= fileSize || (size_t)block >= sums->crcs.size() || !covered(from, to))
			{
				continue;
			}
			auto known = verdicts.find(block);
			if (known == verdicts.end())
			{
				std::fill(buffer.begin(), buffer.end(), 0);
				in.clear();
				in.seekg((std::streamoff)from);
				in.read(buffer.data(), (std::streamsize)(to - from));
				// par2 pads the last block with zeros
				Crc32 crc;
				crc.Append((uchar*)buffer.data(), (uint32)blockSize);
				known = verdicts.emplace(block, crc.Finish() == sums->crcs[(size_t)block]).first;
			}
			bad = !known->second;
		}
		if (bad)
		{
			mismatches.push_back(article);
		}
	}
	return mismatches;
}

void DupeArticleFallback::FinishPin(FileInfo* fileInfo, ArticleInfo* articleInfo,
	const std::vector<CString>& candidates, const std::vector<int>& contributors,
	bool cutover, const char* primaryMessageId)
{
	// the distinct donors behind the candidates, in candidate order: [0] is
	// the donor whose article the pinned slot 0 fetches, [1] the donor a
	// demotion would rotate the lead to
	std::vector<int> donorIds;
	for (int donorId : contributors)
	{
		if (std::find(donorIds.begin(), donorIds.end(), donorId) == donorIds.end())
		{
			donorIds.push_back(donorId);
		}
	}

	articleInfo->SetDupeLeadSnapshot(donorIds.empty() ? 0 : donorIds[0]);
	articleInfo->SetDupeNextLead(donorIds.size() > 1 ? donorIds[1] : 0);
	articleInfo->SetDupeDonorCount((int)donorIds.size());

	// the file's lead is decided by its first pinned article (the donors were
	// iterated in score order then, so this is the top-scored donor offering
	// that part); until rotated it stays this donor by nzb-id
	if (fileInfo->GetDupeLeadDonorId() == 0 && !donorIds.empty())
	{
		fileInfo->SetDupeLeadDonorId(donorIds[0]);
	}

	*articleInfo->GetDupeSources() = OrderSources(candidates, cutover, primaryMessageId);
	// OrderSources keeps the candidates' order (the primary goes last)
	*articleInfo->GetDupeSourceDonors() = contributors;
}

bool DupeArticleFallback::RegisterLeadFailure(FileInfo* fileInfo, ArticleInfo* articleInfo)
{
	// only a failed fetch of the pinned lead donor counts, and only while that
	// donor is still the file's lead: late failures of an already demoted lead
	// reported by in-flight articles must not cascade-demote donors that were
	// never tried, and a part whose slot 0 belongs to ANOTHER donor (the lead
	// does not carry that part) must not charge the lead's streak
	if (articleInfo->GetDupeFallbackRound() != 1 ||
		articleInfo->GetDupeLeadSnapshot() == 0 ||
		articleInfo->GetDupeLeadSnapshot() != fileInfo->GetDupeLeadDonorId())
	{
		return false;
	}

	int failures = fileInfo->GetDupeLeadFailures() + 1;
	fileInfo->SetDupeLeadFailures(failures);

	// rotate only while another donor exists and the rotation budget lasts:
	// once every duplicate has led without a single lead success, the
	// duplicates are equally holed and further rotation (and its log line,
	// once per switch) would be pure noise
	if (failures >= LeadDemoteThreshold && articleInfo->GetDupeNextLead() != 0 &&
		fileInfo->GetDupeLeadSwitches() < articleInfo->GetDupeDonorCount())
	{
		fileInfo->SetDupeLeadDonorId(articleInfo->GetDupeNextLead());
		fileInfo->SetDupeLeadFailures(0);
		fileInfo->SetDupeLeadSwitches(fileInfo->GetDupeLeadSwitches() + 1);
		return true;
	}
	return false;
}

void DupeArticleFallback::RegisterLeadSuccess(FileInfo* fileInfo, ArticleInfo* articleInfo)
{
	if (articleInfo->GetDupeFallbackRound() != 1 ||
		articleInfo->GetDupeLeadSnapshot() == 0 ||
		articleInfo->GetDupeLeadSnapshot() != fileInfo->GetDupeLeadDonorId())
	{
		return;
	}

	fileInfo->SetDupeLeadFailures(0);

	// re-arm the rotation budget only when the success is verifiable: with
	// unfinished neighbours the alignment check passes vacuously, the article
	// may be demoted again later (DemoteMisalignedDupeNeighbors), and its
	// bogus success - although the streak reset is recharged at demotion -
	// must not bypass the rotation bound
	if (ExpectedSegmentOffset(fileInfo, articleInfo) != -1 &&
		ExpectedSegmentEnd(fileInfo, articleInfo) != -1)
	{
		fileInfo->SetDupeLeadSwitches(0);
	}
}

void DupeArticleFallback::VacateGhostLead(FileInfo* fileInfo, const RawNzbList& donors)
{
	int leadNzbId = fileInfo->GetDupeLeadDonorId();
	if (leadNzbId == 0 ||
		std::find_if(donors.begin(), donors.end(),
			[leadNzbId](NzbInfo* donor) { return donor->GetId() == leadNzbId; }) != donors.end())
	{
		return;
	}

	fileInfo->SetDupeLeadDonorId(0);
	fileInfo->SetDupeLeadFailures(0);
	fileInfo->SetDupeLeadSwitches(0);
}

CString DupeArticleFallback::NzbFilenameOf(FileInfo* fileInfo)
{
	ArticleList* articles = fileInfo->GetArticles();
	if (articles->empty())
	{
		return CString();
	}

	// the collection's own nzb-file is parsed (and cached) like a donor's
	NzbInfo* ownNzb = GetParsedDonor(fileInfo->GetNzbInfo());
	if (!ownNzb)
	{
		return CString();
	}

	// The own entry is found by an article message-id. Substituted articles
	// carry a duplicate's id (their original id is not kept across an
	// unload/reload of the article list), so any article whose id is still
	// the posting's own identifies the file.
	for (FileInfo* ownFile : ownNzb->GetFileList())
	{
		ArticleList* ownArticles = ownFile->GetArticles();
		if (ownArticles->size() != articles->size())
		{
			continue;
		}
		for (size_t i = 0; i < articles->size(); i++)
		{
			ArticleInfo* article = (*articles)[i].get();
			const char* messageId = !Util::EmptyStr(article->GetDupeOriginalMessageId()) ?
				article->GetDupeOriginalMessageId() : article->GetMessageId();
			ArticleInfo* ownArticle = (*ownArticles)[i].get();
			if (ownArticle->GetPartNumber() == article->GetPartNumber() &&
				!Util::EmptyStr(messageId) && !strcmp(ownArticle->GetMessageId(), messageId))
			{
				return CString(ownFile->GetFilename());
			}
		}
	}
	return CString();
}

RawNzbList DupeArticleFallback::CollectDonors(DownloadQueue* downloadQueue, NzbInfo* nzbInfo)
{
	RawNzbList donors;

	for (NzbInfo* queuedNzbInfo : downloadQueue->GetQueue())
	{
		if (queuedNzbInfo != nzbInfo &&
			queuedNzbInfo->GetKind() == NzbInfo::nkNzb &&
			queuedNzbInfo->GetDupeMode() != dmForce &&
			DupeCoordinator::SameNameOrKey(queuedNzbInfo->GetName(), queuedNzbInfo->GetDupeKey(),
				nzbInfo->GetName(), nzbInfo->GetDupeKey()))
		{
			donors.push_back(queuedNzbInfo);
		}
	}

	for (HistoryInfo* historyInfo : downloadQueue->GetHistory())
	{
		if (historyInfo->GetKind() == HistoryInfo::hkNzb &&
			historyInfo->GetNzbInfo()->GetDupeMode() != dmForce &&
			DupeCoordinator::SameNameOrKey(historyInfo->GetNzbInfo()->GetName(),
				historyInfo->GetNzbInfo()->GetDupeKey(), nzbInfo->GetName(), nzbInfo->GetDupeKey()))
		{
			donors.push_back(historyInfo->GetNzbInfo());
		}
	}

	std::sort(donors.begin(), donors.end(),
		[](NzbInfo* donor1, NzbInfo* donor2)
		{
			return donor1->GetDupeScore() > donor2->GetDupeScore() ||
				(donor1->GetDupeScore() == donor2->GetDupeScore() &&
				 donor1->GetId() < donor2->GetId());
		});

	return donors;
}

void DupeArticleFallback::PinSources(DownloadQueue* downloadQueue, FileInfo* fileInfo,
	ArticleInfo* articleInfo)
{
	NzbInfo* nzbInfo = fileInfo->GetNzbInfo();

	RawNzbList donors = CollectDonors(downloadQueue, nzbInfo);
	VacateGhostLead(fileInfo, donors);
	RotateToLead(donors, fileInfo->GetDupeLeadDonorId());

	// the name the target's own nzb-file gives this file: the downloaded name
	// may have been replaced by an (obfuscated) name from the article itself,
	// while duplicate nzb-files still carry the subject names
	CString targetNzbFilename = donors.empty() ? CString() : NzbFilenameOf(fileInfo);

	std::vector<CString> candidates;
	std::vector<int> contributors;
	int samePosting = 0;
	int unparsed = 0;

	for (NzbInfo* donorNzbInfo : donors)
	{
		// one whose articles failed the par2 checksums of this download is out (B40)
		if (nzbInfo->GetDupeBlockedDonors()->count(donorNzbInfo->GetId()))
		{
			continue;
		}

		// an exact copy of the same posting shares the message-ids and cannot help
		if (nzbInfo->GetFullContentHash() > 0 &&
			nzbInfo->GetFullContentHash() == donorNzbInfo->GetFullContentHash())
		{
			samePosting++;
			continue;
		}

		// Extract this donor's candidate immediately: GetParsedDonor may evict a
		// previously-parsed donor from the bounded cache, so a parsed-donor pointer
		// must never be held across another GetParsedDonor call (use-after-free).
		NzbInfo* parsedDonor = GetParsedDonor(donorNzbInfo);
		if (parsedDonor)
		{
			AppendDonorCandidate(candidates, contributors, donorNzbInfo->GetId(),
				parsedDonor, fileInfo, articleInfo->GetPartNumber(), targetNzbFilename);
		}
		else
		{
			unparsed++;
		}
	}

	if (candidates.empty() && !donors.empty())
	{
		// the silent "did not even try" case: say why no duplicate could help
		nzbInfo->PrintMessage(Message::mkDetail,
			"No duplicate source for %s [%i]: %i duplicate(s), %i same posting, "
			"%i without readable nzb-file, none with a matching file (nzb name: %s)",
			fileInfo->GetFilename(), articleInfo->GetPartNumber(), (int)donors.size(),
			samePosting, unparsed, targetNzbFilename.Empty() ? "unknown" : *targetNzbFilename);
	}

	FinishPin(fileInfo, articleInfo, candidates, contributors,
		fileInfo->GetDupeCutover(), articleInfo->GetDupeOriginalMessageId());
}

void DupeArticleFallback::AppendDonorCandidate(std::vector<CString>& candidates,
	std::vector<int>& contributors, int donorNzbId,
	NzbInfo* parsedDonor, FileInfo* targetFile, int partNumber, const char* targetNzbFilename)
{
	FileInfo* donorFile = MatchDonorFile(targetFile, parsedDonor, targetNzbFilename);
	if (!donorFile)
	{
		return;
	}

	const char* messageId = FindDonorMessageId(donorFile, partNumber);
	if (!messageId)
	{
		return;
	}

	bool duplicate = std::find_if(candidates.begin(), candidates.end(),
		[messageId](const CString& candidate) { return !strcmp(candidate, messageId); }) != candidates.end();
	if (!duplicate)
	{
		candidates.emplace_back(messageId);
		contributors.push_back(donorNzbId);
	}
}

/*
 * Pure candidate builder over already-live donors, used by unit tests. The
	 * production path (PinSources) processes donors one at a time for
	 * cache-safety; both share AppendDonorCandidate so the logic stays in sync.
	 *
 * The candidate list must not depend on the current state of the article:
 * it is frozen on first fallback, so later donor insertion, removal, or lead
 * rotation cannot shift the round-to-source mapping.
 */
std::vector<CString> DupeArticleFallback::BuildCandidateMessageIds(
	const std::vector<NzbInfo*>& parsedDonors, FileInfo* targetFile, int partNumber)
{
	std::vector<CString> candidates;
	std::vector<int> contributors;

	for (NzbInfo* parsedDonor : parsedDonors)
	{
		AppendDonorCandidate(candidates, contributors, parsedDonor->GetId(),
			parsedDonor, targetFile, partNumber);
	}

	return candidates;
}

std::unique_ptr<NzbInfo> DupeArticleFallback::ParseDonorNzb(const char* queuedFilename)
{
	if (Util::EmptyStr(queuedFilename) || !FileSystem::FileExists(queuedFilename))
	{
		return nullptr;
	}

	NzbFile nzbFile(queuedFilename, "");
	if (!nzbFile.Parse())
	{
		detail("Could not parse duplicate nzb-file %s", queuedFilename);
		return nullptr;
	}

	std::unique_ptr<NzbInfo> parsedNzb = nzbFile.DetachNzbInfo();

	if (g_Options->GetServerMode())
	{
		// in server mode the parser offloads article lists to disk-state and
		// clears them from memory; load them back (the donor is used in memory
		// only) and remove the disk-state files written as parse side effect
		for (FileInfo* fileInfo : parsedNzb->GetFileList())
		{
			g_DiskState->LoadArticles(fileInfo);
		}
		g_DiskState->DiscardFiles(parsedNzb.get(), false);
	}

	return parsedNzb;
}

NzbInfo* DupeArticleFallback::GetParsedDonor(NzbInfo* donorNzbInfo)
{
	int donorId = donorNzbInfo->GetId();

	// a duplicate whose nzb-file couldn't be read is asked again after a while: the
	// failure may have been passing (review item 9)
	auto bad = m_badDonors.find(donorId);
	if (bad != m_badDonors.end() && Util::CurrentTime() - bad->second < BadDonorRetrySec)
	{
		return nullptr;
	}

	auto it = m_parsedDonors.find(donorId);
	if (it != m_parsedDonors.end())
	{
		return it->second.get();
	}

	// duplicates in history don't keep their article lists in memory; the
	// retained source nzb-file is parsed again instead (like "Download again")
	std::unique_ptr<NzbInfo> parsedNzb = ParseDonorNzb(donorNzbInfo->GetQueuedFilename());
	if (!parsedNzb)
	{
		m_badDonors[donorId] = Util::CurrentTime();
		return nullptr;
	}

	if ((int)m_parsedDonors.size() >= MaxCachedDonors)
	{
		m_parsedDonors.erase(m_parsedDonors.begin());
	}

	// store the parsed collection in the cache and return a pointer to the
	// owned object (the cache keeps it alive for the daemon's lifetime)
	std::unique_ptr<NzbInfo>& cachedDonor = m_parsedDonors[donorId];
	cachedDonor = std::move(parsedNzb);

	return cachedDonor.get();
}

FileInfo* DupeArticleFallback::MatchDonorFile(FileInfo* targetFile, NzbInfo* donorNzb,
	const char* targetNzbFilename)
{
	if (IsParFile(targetFile))
	{
		return nullptr;
	}

	std::vector<FileInfo*> structuralMatches;
	FileInfo* filenameMatch = nullptr;
	bool filenameAmbiguous = false;

	for (FileInfo* donorFile : donorNzb->GetFileList())
	{
		if (IsParFile(donorFile) || !StructureMatches(targetFile, donorFile))
		{
			continue;
		}

		if (!strcasecmp(targetFile->GetFilename(), donorFile->GetFilename()) ||
			(!Util::EmptyStr(targetNzbFilename) &&
			 !strcasecmp(targetNzbFilename, donorFile->GetFilename())))
		{
			if (filenameMatch)
			{
				filenameAmbiguous = true;
			}
			else
			{
				filenameMatch = donorFile;
			}
			continue;
		}

		structuralMatches.push_back(donorFile);
	}

	// An exact filename is preferred only when it is unique.  Choosing the
	// first exact match is nondeterministic for multi-file duplicate NZBs.
	if (filenameAmbiguous)
	{
		return nullptr;
	}
	if (filenameMatch)
	{
		return filenameMatch;
	}
	if (structuralMatches.size() == 1)
	{
		return structuralMatches[0];
	}

	// Equal-size volumes of an obfuscated repost all match structurally;
	// the identical member is the only one whose article sizes step exactly
	// like the target's.
	FileInfo* stepMatch = nullptr;
	for (FileInfo* donorFile : structuralMatches)
	{
		if (ArticleSizeStepsMatch(targetFile, donorFile))
		{
			if (stepMatch)
			{
				return nullptr;
			}
			stepMatch = donorFile;
		}
	}
	return stepMatch;
}

uint64 DupeArticleFallback::ArticleSizeStepsHash(FileInfo* fileInfo)
{
	ArticleList* articles = fileInfo->GetArticles();
	if ((int)articles->size() < MinStepFingerprintArticles)
	{
		return 0;
	}

	// FNV-1a over the article count and the steps between consecutive sizes
	uint64 hash = 14695981039346656037ULL;
	auto mix = [&hash](int64 value)
	{
		for (int i = 0; i < 8; i++)
		{
			hash ^= (uint64)((value >> (i * 8)) & 0xff);
			hash *= 1099511628211ULL;
		}
	};

	mix((int64)articles->size());
	bool varying = false;
	for (size_t i = 1; i < articles->size(); i++)
	{
		int step = (*articles)[i]->GetSize() - (*articles)[i - 1]->GetSize();
		mix(step);
		varying |= step != 0 && i < articles->size() - 1;
	}
	return varying && hash != 0 ? hash : 0;
}

bool DupeArticleFallback::ArticleSizeStepsMatch(FileInfo* targetFile, FileInfo* donorFile)
{
	// An NZB lists the encoded size of each article including its headers.
	// Two postings of the same bytes with the same segmentation differ there
	// only by a per-file constant (their subject lines differ), so the steps
	// between consecutive article sizes are equal. Encoded sizes vary with
	// the content (yEnc escaping), which makes the step sequence a strong
	// identity fingerprint once a file has enough articles.
	ArticleList* targetArticles = targetFile->GetArticles();
	ArticleList* donorArticles = donorFile->GetArticles();
	if ((int)targetArticles->size() < MinStepFingerprintArticles ||
		targetArticles->size() != donorArticles->size())
	{
		return false;
	}

	bool varying = false;
	for (size_t i = 1; i < targetArticles->size(); i++)
	{
		int targetStep = (*targetArticles)[i]->GetSize() - (*targetArticles)[i - 1]->GetSize();
		int donorStep = (*donorArticles)[i]->GetSize() - (*donorArticles)[i - 1]->GetSize();
		if (targetStep != donorStep)
		{
			return false;
		}
		// a posting tool that lists one fixed size per article proves nothing
		varying |= targetStep != 0 && i < targetArticles->size() - 1;
	}
	return varying;
}

// The part count a yEnc subject declares, its last "(part/count)"; 0 if it has none
// (an obfuscated subject). An nzb-file that lacks segments keeps the poster's count.
static int DeclaredParts(FileInfo* fileInfo)
{
	const char* subject = fileInfo->GetSubject();
	int declared = 0;
	for (const char* p = subject ? strchr(subject, '(') : nullptr; p; p = strchr(p + 1, '('))
	{
		int part = 0;
		int count = 0;
		char close = 0;
		if (sscanf(p, "(%d/%d%c", &part, &count, &close) == 3 && close == ')' && part >= 0 && count > 0)
		{
			declared = count;
		}
	}
	return declared;
}

bool DupeArticleFallback::StructureMatches(FileInfo* targetFile, FileInfo* donorFile)
{
	ArticleList* targetArticles = targetFile->GetArticles();
	ArticleList* donorArticles = donorFile->GetArticles();

	if (targetArticles->empty())
	{
		return false;
	}

	// twins are posted in as many parts: a file whose subject counts other parts, or
	// that has a part beyond the count the release's subject declares, is another file
	// (B39: dead volumes of 29 parts paired with a healthy 30-part file)
	int targetDeclared = DeclaredParts(targetFile);
	int donorDeclared = DeclaredParts(donorFile);
	if (targetDeclared > 0 && donorDeclared > 0 && targetDeclared != donorDeclared)
	{
		return false;
	}
	if (targetDeclared > 0)
	{
		for (ArticleInfo* donorArticle : donorFile->GetArticles())
		{
			if (donorArticle->GetPartNumber() > targetDeclared)
			{
				return false;
			}
		}
	}

	// The release's nzb-file may lack a few segments (an indexer that didn't
	// capture them all): it still pairs with a twin that lists them, part by part.
	// Every part the release lists must be in the duplicate at a matching size;
	// the duplicate's other parts must be gaps in the release's list (at most one
	// in 16), and at most one may follow the release's last part - more would be a
	// longer encode with the same article size, not a twin.
	if (targetArticles->size() < donorArticles->size())
	{
		size_t extra = donorArticles->size() - targetArticles->size();
		if (extra > std::max<size_t>(1, donorArticles->size() / 16))
		{
			return false;
		}
		std::map<int, ArticleInfo*> donorParts;
		for (ArticleInfo* donorArticle : donorFile->GetArticles())
		{
			donorParts[donorArticle->GetPartNumber()] = donorArticle;
		}
		int lastTargetPart = 0;
		std::set<int> targetParts;
		for (ArticleInfo* targetArticle : targetFile->GetArticles())
		{
			auto it = donorParts.find(targetArticle->GetPartNumber());
			if (it == donorParts.end() ||
				!SizesMatch(targetArticle->GetSize(), it->second->GetSize(), PartSizeToleranceDiv))
			{
				return false;
			}
			targetParts.insert(targetArticle->GetPartNumber());
			lastTargetPart = std::max(lastTargetPart, targetArticle->GetPartNumber());
		}
		int trailing = 0;
		int64 trailingSize = 0;
		for (ArticleInfo* donorArticle : donorFile->GetArticles())
		{
			if (!targetParts.count(donorArticle->GetPartNumber()) && donorArticle->GetPartNumber() > lastTargetPart)
			{
				trailing++;
				trailingSize += donorArticle->GetSize();
			}
		}
		// nzbget counts a segment the nzb-file lacks between listed parts as missed,
		// so the release's size includes an estimate for it, close to the twin's size
		return trailing <= 1 &&
			SizesMatch(targetFile->GetSize() + trailingSize, donorFile->GetSize(), TotalSizeToleranceDiv);
	}

	if (targetArticles->size() != donorArticles->size() ||
		targetFile->GetTotalArticles() != donorFile->GetTotalArticles() ||
		!SizesMatch(targetFile->GetSize(), donorFile->GetSize(), TotalSizeToleranceDiv))
	{
		return false;
	}

	for (size_t i = 0; i < targetArticles->size(); i++)
	{
		ArticleInfo* targetArticle = (*targetArticles)[i].get();
		ArticleInfo* donorArticle = (*donorArticles)[i].get();
		if (targetArticle->GetPartNumber() != donorArticle->GetPartNumber() ||
			!SizesMatch(targetArticle->GetSize(), donorArticle->GetSize(), PartSizeToleranceDiv))
		{
			return false;
		}
	}

	return true;
}

const char* DupeArticleFallback::FindDonorMessageId(FileInfo* donorFile, int partNumber)
{
	for (ArticleInfo* article : donorFile->GetArticles())
	{
		if (article->GetPartNumber() == partNumber && !Util::EmptyStr(article->GetMessageId()))
		{
			return article->GetMessageId();
		}
	}

	return nullptr;
}

bool DupeArticleFallback::SizesMatch(int64 size1, int64 size2, int div)
{
	int64 diff = size1 > size2 ? size1 - size2 : size2 - size1;
	return diff <= std::max(size1, size2) / div;
}

static int FindArticleIndex(ArticleList* articles, ArticleInfo* articleInfo)
{
	for (size_t i = 0; i < articles->size(); i++)
	{
		if ((*articles)[i].get() == articleInfo)
		{
			return (int)i;
		}
	}
	return -1;
}

int64 DupeArticleFallback::ExpectedSegmentOffset(FileInfo* fileInfo, ArticleInfo* articleInfo)
{
	ArticleList* articles = fileInfo->GetArticles();
	int index = FindArticleIndex(articles, articleInfo);
	if (index < 0)
	{
		return -1;
	}

	if (articleInfo->GetPartNumber() == 1)
	{
		// the first part of a file always decodes to offset 0
		return 0;
	}
	if (index == 0)
	{
		// earlier parts are missing from the nzb-file: the offset is unknown
		return -1;
	}

	// a list neighbour is the previous part only when the nzb-file lists every part
	ArticleInfo* prev = (*articles)[index - 1].get();
	if (prev->GetPartNumber() == articleInfo->GetPartNumber() - 1 &&
		prev->GetStatus() == ArticleInfo::aiFinished && prev->GetSegmentSize() > 0)
	{
		return prev->GetSegmentOffset() + prev->GetSegmentSize();
	}

	return -1;
}

int64 DupeArticleFallback::ExpectedSegmentEnd(FileInfo* fileInfo, ArticleInfo* articleInfo)
{
	ArticleList* articles = fileInfo->GetArticles();
	int index = FindArticleIndex(articles, articleInfo);
	if (index < 0)
	{
		return -1;
	}

	if (index == (int)articles->size() - 1)
	{
		// the last article of a file must decode up to the decoded file size
		// (known once any article of the file was decoded)
		return fileInfo->GetDecodedFileSize() > 0 ? fileInfo->GetDecodedFileSize() : -1;
	}

	ArticleInfo* next = (*articles)[index + 1].get();
	if (next->GetPartNumber() == articleInfo->GetPartNumber() + 1 &&
		next->GetStatus() == ArticleInfo::aiFinished && next->GetSegmentSize() > 0)
	{
		return next->GetSegmentOffset();
	}

	return -1;
}

bool DupeArticleFallback::SegmentAligned(FileInfo* fileInfo, ArticleInfo* articleInfo)
{
	int64 begin = articleInfo->GetSegmentOffset();
	int size = articleInfo->GetSegmentSize();
	if (size <= 0)
	{
		// no decoded placement recorded - nothing to validate against
		return true;
	}

	int64 expectedOffset = ExpectedSegmentOffset(fileInfo, articleInfo);
	if (expectedOffset >= 0 && begin != expectedOffset)
	{
		return false;
	}

	int64 expectedEnd = ExpectedSegmentEnd(fileInfo, articleInfo);
	if (expectedEnd >= 0 && begin + size != expectedEnd)
	{
		return false;
	}

	return true;
}

ArticleInfo* DupeArticleFallback::FirstUntiledArticle(FileInfo* fileInfo)
{
	// DecodedFileSize is only set from a decoded yEnc article; when it is 0 the
	// file is non-yEnc (e.g. uuencode, whose articles all record offset 0) or
	// its geometry is not yet known - either way the tiling cannot be judged.
	// After a restart it is repopulated from the first yEnc article that
	// completes, so it is available again by the time a reloaded file finishes.
	int64 decodedSize = fileInfo->GetDecodedFileSize();
	if (decodedSize <= 0)
	{
		return nullptr;
	}

	int64 expected = 0;
	ArticleList* articles = fileInfo->GetArticles();
	for (size_t index = 0; index < articles->size(); index++)
	{
		ArticleInfo* article = (*articles)[index].get();
		if (article->GetStatus() != ArticleInfo::aiFinished || article->GetSegmentSize() <= 0)
		{
			// an unfinished article or one without a recorded decoded placement
			// leaves the geometry incomplete: do not judge (avoids false positives
			// on partial or non-decoded states)
			return nullptr;
		}
		if (article->GetSegmentOffset() != expected)
		{
			// a gap (offset > expected) or overlap (offset < expected) at this seam.
			// A duplicate's article is the likelier culprit than the release's own.
			// Otherwise, at a gap, an article that ends exactly where the next one
			// begins (or at the file's end) is placed right: the one before it
			// decoded short. Anything else is this article's fault.
			ArticleInfo* prev = index > 0 ? (*articles)[index - 1].get() : nullptr;
			if (prev)
			{
				bool prevBorrowed = prev->GetDupeFallbackRound() > 0;
				bool thisBorrowed = article->GetDupeFallbackRound() > 0;
				if (prevBorrowed != thisBorrowed)
				{
					return prevBorrowed ? prev : article;
				}
				int64 end = article->GetSegmentOffset() + article->GetSegmentSize();
				ArticleInfo* next = index + 1 < articles->size() ? (*articles)[index + 1].get() : nullptr;
				bool placedRight = next ? next->GetStatus() == ArticleInfo::aiFinished &&
					next->GetSegmentSize() > 0 && next->GetSegmentOffset() == end : end == decodedSize;
				if (article->GetSegmentOffset() > expected && placedRight)
				{
					return prev;
				}
			}
			return article;
		}
		expected = article->GetSegmentOffset() + article->GetSegmentSize();
	}

	if (expected != decodedSize)
	{
		// the assembled decoded bytes fall short of / exceed the file size
		return articles->empty() ? nullptr : (*articles)[articles->size() - 1].get();
	}

	return nullptr;
}

bool DupeArticleFallback::HasDonorArticles(FileInfo* fileInfo)
{
	for (ArticleInfo* article : fileInfo->GetArticles())
	{
		if (article->GetDupeFallbackRound() > 0)
		{
			return true;
		}
	}
	return false;
}

bool DupeArticleFallback::MergeDecodedFileSize(FileInfo* fileInfo, int64 articleFileSize)
{
	int64 decodedFileSize = fileInfo->GetDecodedFileSize();
	if (articleFileSize <= 0 || decodedFileSize < 0 || decodedFileSize == articleFileSize)
	{
		return false;
	}

	if (decodedFileSize == 0)
	{
		fileInfo->SetDecodedFileSize(articleFileSize);
		return false;
	}

	fileInfo->SetDecodedFileSize(-1);
	return true;
}
