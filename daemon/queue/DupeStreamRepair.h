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


#ifndef DUPESTREAMREPAIR_H
#define DUPESTREAMREPAIR_H

#include <string>
#include <vector>
#include "DownloadInfo.h"

/*
 * Byte-range ("stream level") recovery of directly posted media files from
 * duplicate postings of the same content that were segmented differently
 * (see option <DupeArticleFallback> value "stream").
 *
 * This class holds the pure geometry and eligibility logic: computing the
 * missing byte ranges of a file from its downloaded articles, estimating the
 * decoded byte ranges of a donor's articles from its nzb-declared encoded
 * sizes, and selecting donor articles for content probes and hole patches.
 * The post-processing stage that fetches and writes bytes is
 * StreamRepairController (daemon/postprocess/StreamRepair.h).
 */
class DupeStreamRepair
{
public:
	// donor articles picked around holes are expanded by this many parts on
	// each side to absorb the encoded-to-decoded estimation error
	static constexpr int PatchMarginParts = 2;
	// number of donor articles fetched to verify content identity against
	// already-downloaded regions before anything is written
	static constexpr int ProbeCount = 2;
	// a probe must byte-compare at least this much overlap to count as proof
	static constexpr int64 MinProbeCompareBytes = 16 * 1024;
	// how many donor members are probed per target member before giving up
	// (bounds wasted fetches when pairing heuristics fail on obfuscated sets)
	static constexpr int MaxDonorCandidates = 4;

	static bool IsStreamEligible(const char* filename);
	static StreamRangeList ComputeHoles(FileInfo* fileInfo);
	static int64 TotalSize(const StreamRangeList& ranges);
	static StreamRangeList EstimateDonorRanges(FileInfo* donorFile, int64 decodedFileSize);
	static std::vector<int> SelectPatchParts(const StreamRangeList& donorRanges,
		const StreamRangeList& holes, int marginParts);
	static std::vector<int> SelectProbeParts(const StreamRangeList& donorRanges,
		const StreamRangeList& holes, int probeCount);
	static void SubtractCovered(StreamRangeList& ranges, const StreamRange& covered);

	/* Captures a stream-repair job on the owning NzbInfo for a media file
	 * that completed with missing byte ranges. diskBasename is the file's
	 * on-disk name at completion. Must be called within DownloadQueue-lock. */
	static bool BuildRepairJob(FileInfo* fileInfo, const char* diskBasename);

	/* The last two dot-separated segments of a filename, lowercased
	 * ("Rel.part03.rar" -> "part03.rar", "X.R00" -> "r00"): equal-size
	 * members of a repost pair by this key when names differ. */
	static std::string SuffixKey(const char* filename);

	/* Donor files of one duplicate collection, ordered most-likely-identical
	 * first for the given target member: exact name match, same suffix key
	 * when it identifies exactly one donor member (volume schemes; shared
	 * extension keys are skipped), the positionalRank-th size-window member
	 * by donor filename order when positionalWindow matches the donor window size
	 * (rank < 0 skips), then ascending encoded-size distance; deduplicated
	 * and capped. Every candidate still has to pass probe verification. */
	static std::vector<FileInfo*> SelectDonorCandidates(const char* targetFilename,
		int64 targetDecodedFileSize, int positionalRank, int positionalWindow,
		NzbInfo* donorNzb, int maxCandidates);

private:
	static bool RangesIntersect(const StreamRange& range1, const StreamRange& range2)
	{
		return range1.Offset < range2.End() && range2.Offset < range1.End();
	}
	static bool IntersectsAny(const StreamRange& range, const StreamRangeList& ranges);
};

#endif
