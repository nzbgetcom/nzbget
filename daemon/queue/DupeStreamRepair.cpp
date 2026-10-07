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
#include <functional>
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
	if (g_Options->GetDupeArticleFallback() < Options::dafStream || g_Options->GetRawArticle() ||
		DupeArticleFallback::IsParFile(fileInfo) || Util::EndsWith(diskBasename, ".par2", false))
	{
		return false;
	}

	NzbInfo* nzbInfo = fileInfo->GetNzbInfo();
	// a health cancel keeps the files for "Download remaining files": the file
	// whose last article set it off still takes its job
	bool healthCancel = nzbInfo && nzbInfo->GetDeleteStatus() == NzbInfo::dsHealth;
	if (!nzbInfo || (!healthCancel && (nzbInfo->GetDeleting() || nzbInfo->GetParking() ||
		nzbInfo->GetDeleteStatus() != NzbInfo::dsNone)) ||
		nzbInfo->GetPostInfo() != nullptr)
	{
		// the PostInfo check: files completing AFTER post-processing started
		// (par-check unpauses par2 volumes mid-repair) must not re-arm the
		// already-drained job list for a second stream-repair pass
		return false;
	}

	if (Util::EmptyStr(diskBasename) || fileInfo->GetArticles()->empty())
	{
		return false;
	}

	// A file none of whose articles arrived has no decoded size and nothing
	// on disk: the whole file is its hole. It can only be recreated from a
	// donor member identified structurally and proven on the collection's
	// other files (see StreamRepairController::RepairWholeFile).
	if (fileInfo->GetSuccessArticles() == 0)
	{
		if (fileInfo->GetSize() <= 0)
		{
			return false;
		}
		nzbInfo->GetStreamRepairJobs()->emplace_back(fileInfo->GetId(), diskBasename,
			0, fileInfo->GetFailedSize(), fileInfo->GetMissedSize(),
			fileInfo->GetFailedArticles() + fileInfo->GetMissedArticles(), fileInfo->GetParFile(),
			StreamRangeList(), DupeArticleFallback::ArticleSizeStepsHash(fileInfo));
		return true;
	}

	// Data files qualify regardless of container: identity is decided by
	// probe byte-compares. Parity is excluded above because partial byte
	// matches cannot establish that another posting belongs to the PAR set.
	if (fileInfo->GetDecodedFileSize() <= 0)
	{
		return false;
	}

	StreamRangeList holes = ComputeHoles(fileInfo);
	if (holes.empty())
	{
		return false;
	}

	nzbInfo->GetStreamRepairJobs()->emplace_back(fileInfo->GetId(), diskBasename,
		fileInfo->GetDecodedFileSize(), fileInfo->GetFailedSize(), fileInfo->GetMissedSize(),
		fileInfo->GetFailedArticles() + fileInfo->GetMissedArticles(), fileInfo->GetParFile(),
		std::move(holes), DupeArticleFallback::ArticleSizeStepsHash(fileInfo));

	return true;
}

FileInfo* DupeStreamRepair::SelectWholeFileDonor(const char* targetFilename, int64 targetEncodedSize,
	int targetArticleCount, uint64 targetStepsHash, NzbInfo* donorNzb,
	const std::set<FileInfo*>* claimed, const std::string& donorSet)
{
	if (Util::EmptyStr(targetFilename) || Util::EndsWith(targetFilename, ".par2", false) ||
		targetEncodedSize <= 0 || targetArticleCount <= 0)
	{
		return nullptr;
	}

	// both sides are nzb-declared encoded sizes here, so the tight tolerance
	// of the download-time article pairing applies; the article count may
	// differ (a repost cut into other article sizes still names its volumes)
	std::vector<FileInfo*> pool;
	for (FileInfo* donorFile : donorNzb->GetFileList())
	{
		if (!DupeArticleFallback::IsParFile(donorFile) &&
			!donorFile->GetArticles()->empty() &&
			(!claimed || !claimed->count(donorFile)) &&
			(donorSet.empty() || VolumeSetKey(donorFile->GetFilename()) == donorSet) &&
			DupeArticleFallback::SizesMatch(donorFile->GetSize(), targetEncodedSize, WholeFileSizeToleranceDiv))
		{
			pool.push_back(donorFile);
		}
	}

	auto unique = [&pool](std::function<bool(FileInfo*)> matches) -> FileInfo*
	{
		FileInfo* match = nullptr;
		for (FileInfo* donorFile : pool)
		{
			if (matches(donorFile))
			{
				if (match)
				{
					return nullptr;
				}
				match = donorFile;
			}
		}
		return match;
	};

	if (targetStepsHash != 0)
	{
		if (FileInfo* match = unique([targetStepsHash](FileInfo* donorFile)
			{ return DupeArticleFallback::ArticleSizeStepsHash(donorFile) == targetStepsHash; }))
		{
			return match;
		}
	}
	if (FileInfo* match = unique([targetFilename](FileInfo* donorFile)
		{ return !strcasecmp(donorFile->GetFilename(), targetFilename); }))
	{
		return match;
	}
	std::string targetKey = VolumeKey(targetFilename);
	if (!targetKey.empty())
	{
		return unique([&targetKey](FileInfo* donorFile)
			{ return VolumeKey(donorFile->GetFilename()) == targetKey; });
	}
	return nullptr;
}

std::string DupeStreamRepair::VolumeSetKey(const char* filename)
{
	if (Util::EmptyStr(filename))
	{
		return "";
	}
	std::string name(FileSystem::BaseFileName(filename));
	for (char& ch : name)
	{
		ch = (char)tolower((unsigned char)ch);
	}

	auto digitsBefore = [&name](size_t end) -> size_t
	{
		size_t begin = end;
		while (begin > 0 && isdigit((unsigned char)name[begin - 1]))
		{
			begin--;
		}
		return begin;
	};
	auto endsWith = [&name](const char* tail) -> bool
	{
		size_t len = strlen(tail);
		return name.size() > len && !name.compare(name.size() - len, len, tail);
	};

	// "x.partNN.rar"
	if (endsWith(".rar"))
	{
		size_t end = name.size() - 4;
		size_t begin = digitsBefore(end);
		if (begin < end && begin >= 5 && !name.compare(begin - 5, 5, ".part"))
		{
			return name.substr(0, begin) + "#.rar";
		}
		// old-style first volume "x.rar" of "x.r00", "x.r01", ...
		return name.substr(0, name.size() - 4) + ".r#";
	}
	if (endsWith(".zip"))
	{
		return name.substr(0, name.size() - 4) + ".z#";
	}
	// "x.rNN", "x.zNN", "x.NNN" (7z/raw splits)
	size_t begin = digitsBefore(name.size());
	if (begin < name.size() && begin >= 2 && name[begin - 1] != '.' &&
		name[begin - 2] == '.' && (name[begin - 1] == 'r' || name[begin - 1] == 'z'))
	{
		return name.substr(0, begin) + "#";
	}
	// an old-style set of more than 101 volumes continues "x.r99" with
	// "x.s00", "x.s01", ..., "x.t00", ...
	if (name.size() - begin == 2 && begin >= 2 && name[begin - 2] == '.' &&
		name[begin - 1] >= 's' && name[begin - 1] <= 'y')
	{
		return name.substr(0, begin - 1) + "r#";
	}
	if (begin < name.size() && name.size() - begin >= 3 && begin >= 1 && name[begin - 1] == '.')
	{
		return name.substr(0, begin) + "#";
	}
	return "";
}

bool DupeStreamRepair::DecodedSizePlausible(int64 decodedFileSize, int64 encodedSize)
{
	return decodedFileSize > 0 && encodedSize > 0 && decodedFileSize <= encodedSize &&
		decodedFileSize >= encodedSize - encodedSize / 8;
}

std::string DupeStreamRepair::VolumeKey(const char* filename)
{
	std::string key = SuffixKey(filename);
	std::string out;
	out.reserve(key.size());
	size_t i = 0;
	while (i < key.size())
	{
		if (!isdigit((unsigned char)key[i]))
		{
			out += key[i++];
			continue;
		}
		// a run of digits: keep its value, not its padding
		size_t end = i;
		while (end < key.size() && isdigit((unsigned char)key[end]))
		{
			end++;
		}
		size_t first = i;
		while (first + 1 < end && key[first] == '0')
		{
			first++;
		}
		out.append(key, first, end - first);
		i = end;
	}
	return out;
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
	NzbInfo* donorNzb, int maxCandidates, uint64 targetStepsHash,
	const std::set<FileInfo*>* claimed)
{
	if (Util::EmptyStr(targetFilename) || Util::EndsWith(targetFilename, ".par2", false))
	{
		return {};
	}

	// the window: donor files whose nzb-declared ENCODED size is near the
	// target's decoded size (yEnc overhead is ~1-3%; div 8 tolerates ~12.5%)
	// and which still carry an article list (EstimateDonorRanges needs it)
	std::vector<FileInfo*> window;
	for (FileInfo* donorFile : donorNzb->GetFileList())
	{
		if (!DupeArticleFallback::IsParFile(donorFile) && !donorFile->GetArticles()->empty() &&
			DupeArticleFallback::SizesMatch(donorFile->GetSize(), targetDecodedFileSize, 8))
		{
			window.push_back(donorFile);
		}
	}

	std::vector<FileInfo*> candidates;
	auto add = [&candidates, maxCandidates, claimed](FileInfo* donorFile)
	{
		if ((int)candidates.size() < maxCandidates &&
			(!claimed || !claimed->count(donorFile)) &&
			std::find(candidates.begin(), candidates.end(), donorFile) == candidates.end())
		{
			candidates.push_back(donorFile);
		}
	};

	// 0. a byte-identical repost, whatever its names: the only donor member
	// whose article sizes step exactly like the target's posting did
	if (targetStepsHash != 0)
	{
		FileInfo* stepMatch = nullptr;
		bool ambiguous = false;
		for (FileInfo* donorFile : donorNzb->GetFileList())
		{
			if (!DupeArticleFallback::IsParFile(donorFile) &&
				DupeArticleFallback::ArticleSizeStepsHash(donorFile) == targetStepsHash)
			{
				ambiguous = stepMatch != nullptr;
				stepMatch = donorFile;
			}
		}
		if (stepMatch && !ambiguous)
		{
			add(stepMatch);
		}
	}

	// 1. a repost that kept its filenames
	for (FileInfo* donorFile : window)
	{
		if (!strcasecmp(donorFile->GetFilename(), targetFilename))
		{
			add(donorFile);
		}
	}

	// 2. same suffix key, but only when it identifies EXACTLY ONE donor
	// member: volume schemes ("part03.rar", "r00") are
	// unique per member, while shared keys (a same-extension episode pack,
	// digit-bearing or not: "mkv", "mp4") would flood the cap in file-list
	// order and evict the better-ranked tiers below
	std::string targetKey = VolumeKey(targetFilename);
	if (!targetKey.empty())
	{
		FileInfo* keyMatch = nullptr;
		bool ambiguous = false;
		for (FileInfo* donorFile : window)
		{
			if (VolumeKey(donorFile->GetFilename()) == targetKey)
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
	constexpr size_t MaxExtractedEntries = 10000;
	constexpr int64 MaxExtractedBytes = MaxDecompressBytes;
	size_t entryCount = 0;
	int64 extractedBytes = 0;

	fs::error_code dirEc;
	for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, dirEc);
		!dirEc && it != fs::recursive_directory_iterator();
		it.increment(dirEc))
	{
		if (++entryCount > MaxExtractedEntries)
		{
			return "";
		}
		// skip symlinks WITHOUT following them: a hostile donor archive could
		// contain a link escaping the scratch dir (e.g. to the target's own
		// file), which would trivially "verify" and defeat real donors
		fs::error_code linkEc;
		if (fs::is_symlink(it->symlink_status(linkEc)) || linkEc)
		{
			info("Stream repair: archive contains link");
			return "";
		}

		fs::error_code fileEc;
		if (!it->is_regular_file(fileEc) || fileEc)
		{
			continue;
		}

		std::string path = fs::u8string(it->path());
		int64 size = FileSystem::FileSize(path.c_str());
		if (size < 0 || size > MaxExtractedBytes - extractedBytes)
		{
			return "";
		}
		extractedBytes += size;
		if (size == innerSize)
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

bool DupeStreamRepair::ExceedsDecompressCap(int64 totalBytes, int64 addBytes,
	int64 totalExtent, int64 addExtent)
{
	return totalBytes + addBytes > MaxDecompressBytes ||
		totalExtent + addExtent > MaxDecompressBytes;
}

std::string DupeStreamRepair::BuildDonorKey(NzbInfo* donorNzb, const char* password)
{
	std::string key;
	// Length-prefix every field: neither separator characters in names nor a
	// hash collision may cause a different donor to be discarded. Preserve
	// parser order because candidate and newsgroup selection can depend on it.
	auto append = [&key](const std::string& value)
	{
		key += std::to_string(value.size());
		key += ':';
		key += value;
	};
	append(password ? password : "");
	append(std::to_string(donorNzb->GetFileList()->size()));
	for (FileInfo* file : donorNzb->GetFileList())
	{
		append(file->GetFilename());
		append(std::to_string(file->GetSize()));
		append(file->GetParFile() ? "par" : "data");
		append(std::to_string(file->GetGroups()->size()));
		for (const CString& group : *file->GetGroups())
		{
			append(*group);
		}
		append(std::to_string(file->GetArticles()->size()));
		for (const std::unique_ptr<ArticleInfo>& article : *file->GetArticles())
		{
			append(std::to_string(article->GetPartNumber()));
			append(std::to_string(article->GetSize()));
			append(article->GetMessageId());
		}
	}
	return key;
}
