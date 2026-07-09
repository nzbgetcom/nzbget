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

#include <boost/test/unit_test.hpp>
#include <cstring>
#include "ContentMap.h"

BOOST_AUTO_TEST_SUITE(PostprocessTest)

namespace
{

// an in-memory member: reads always succeed inside [0, Size)
class MemoryContentSource : public ContentSource
{
public:
	MemoryContentSource(std::vector<char> data) : m_data(std::move(data)) {}
	virtual int64 Size() { return (int64)m_data.size(); }
	virtual bool Read(int64 offset, void* buffer, int64 size)
	{
		if (offset < 0 || size < 0 || offset + size > (int64)m_data.size())
		{
			return false;
		}
		memcpy(buffer, m_data.data() + offset, size);
		return true;
	}
private:
	std::vector<char> m_data;
};

class MemorySourceSet : public ContentSourceSet
{
public:
	virtual ContentSource* GetSource(int memberIndex)
	{
		return memberIndex >= 0 && memberIndex < (int)Sources.size() ?
			Sources[memberIndex].get() : nullptr;
	}
	std::vector<std::unique_ptr<MemoryContentSource>> Sources;
};

std::vector<char> Pattern(int size, char seed)
{
	std::vector<char> data(size);
	for (int i = 0; i < size; i++)
	{
		data[i] = (char)(seed + i * 7);
	}
	return data;
}

}

BOOST_AUTO_TEST_CASE(ContentMapModelTest)
{
	// inner file of 100 bytes inside two members, 20 bytes of framing before
	// each data run: member 0 carries inner [0,60), member 1 carries [60,100)
	ContentMap map;
	map.SetInnerName("movie.mkv");
	map.SetInnerSize(100);
	map.GetRuns()->push_back({0, 0, 20, 60});
	map.GetRuns()->push_back({60, 1, 20, 40});

	// member range straddling framing and data: only the data part maps
	StreamRangeList inner = map.MapToInner(0, {0, 30});
	BOOST_REQUIRE_EQUAL(inner.size(), 1u);
	BOOST_CHECK_EQUAL(inner[0].Offset, 0);
	BOOST_CHECK_EQUAL(inner[0].Size, 10);

	// inner range straddling the member boundary splits into two pieces
	std::vector<MemberRange> pieces = map.MapFromInner({50, 20});
	BOOST_REQUIRE_EQUAL(pieces.size(), 2u);
	BOOST_CHECK_EQUAL(pieces[0].MemberIndex, 0);
	BOOST_CHECK_EQUAL(pieces[0].Range.Offset, 70);	// 20 framing + (50-0)
	BOOST_CHECK_EQUAL(pieces[0].Range.Size, 10);
	BOOST_CHECK_EQUAL(pieces[1].MemberIndex, 1);
	BOOST_CHECK_EQUAL(pieces[1].Range.Offset, 20);
	BOOST_CHECK_EQUAL(pieces[1].Range.Size, 10);

	// a hole entirely inside framing maps to nothing
	BOOST_CHECK(map.MapToInner(0, {0, 20}).empty());

	// excluding a member drops its runs in both directions
	map.ExcludeMember(0);
	BOOST_CHECK(map.MapToInner(0, {20, 60}).empty());
	std::vector<MemberRange> after = map.MapFromInner({0, 100});
	BOOST_REQUIRE_EQUAL(after.size(), 1u);
	BOOST_CHECK_EQUAL(after[0].MemberIndex, 1);
}

BOOST_AUTO_TEST_CASE(CompositeSourceTest)
{
	MemorySourceSet sources;
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(Pattern(10, 1)));
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(Pattern(20, 2)));
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(Pattern(5, 3)));

	CompositeSource composite(sources, {0, 1, 2}, {10, 20, 5});
	BOOST_CHECK_EQUAL(composite.Size(), 35);

	// a read across the 0/1 boundary stitches both members
	char buffer[15];
	BOOST_REQUIRE(composite.Read(5, buffer, 15));
	std::vector<char> member0 = Pattern(10, 1), member1 = Pattern(20, 2);
	BOOST_CHECK(!memcmp(buffer, member0.data() + 5, 5));
	BOOST_CHECK(!memcmp(buffer + 5, member1.data(), 10));

	// past-end reads fail all-or-nothing
	BOOST_CHECK(!composite.Read(30, buffer, 10));

	// logical->member translation splits across boundaries
	std::vector<MemberRange> pieces = composite.ToMembers({8, 24});
	BOOST_REQUIRE_EQUAL(pieces.size(), 3u);
	BOOST_CHECK_EQUAL(pieces[0].MemberIndex, 0);
	BOOST_CHECK_EQUAL(pieces[0].Range.Offset, 8);
	BOOST_CHECK_EQUAL(pieces[0].Range.Size, 2);
	BOOST_CHECK_EQUAL(pieces[1].MemberIndex, 1);
	BOOST_CHECK_EQUAL(pieces[1].Range.Offset, 0);
	BOOST_CHECK_EQUAL(pieces[1].Range.Size, 20);
	BOOST_CHECK_EQUAL(pieces[2].MemberIndex, 2);
	BOOST_CHECK_EQUAL(pieces[2].Range.Offset, 0);
	BOOST_CHECK_EQUAL(pieces[2].Range.Size, 2);
}

BOOST_AUTO_TEST_CASE(ContentMapperGroupSetsTest)
{
	std::vector<SetMember> members = {
		{"Rel.part02.rar", 100}, {"Rel.part01.rar", 100},		// 0,1: new naming, out of order
		{"old.r00", 100}, {"old.rar", 100}, {"old.r01", 100},	// 2,3,4: old naming
		{"span.z01", 100}, {"span.zip", 100}, {"span.z02", 100},	// 5,6,7: spanned zip
		{"seven.7z.002", 100}, {"seven.7z.001", 100},			// 8,9: 7z splits
		{"movie.mkv.001", 100}, {"movie.mkv.002", 100},			// 10,11: raw splits
		{"bare.mkv", 100},										// 12: bare media
		{"Rel.vol00+01.par2", 100},								// 13: no set
		{"gap.part01.rar", 100}, {"gap.part03.rar", 100},		// 14,15: incomplete -> dropped
		{"readme.txt", 100},									// 16: no set
	};

	std::vector<MemberSet> sets = ContentMapper::GroupSets(members);
	BOOST_REQUIRE_EQUAL(sets.size(), 6u);

	// new-naming rar, ordered by part number regardless of listing order
	BOOST_CHECK_EQUAL((int)sets[0].Format, (int)MemberSet::mfRar);
	BOOST_REQUIRE_EQUAL(sets[0].Members.size(), 2u);
	BOOST_CHECK_EQUAL(sets[0].Members[0], 1);
	BOOST_CHECK_EQUAL(sets[0].Members[1], 0);

	// old-naming rar: .rar first, then .r00, .r01
	BOOST_CHECK_EQUAL((int)sets[1].Format, (int)MemberSet::mfRar);
	BOOST_REQUIRE_EQUAL(sets[1].Members.size(), 3u);
	BOOST_CHECK_EQUAL(sets[1].Members[0], 3);
	BOOST_CHECK_EQUAL(sets[1].Members[1], 2);
	BOOST_CHECK_EQUAL(sets[1].Members[2], 4);

	// spanned zip: z01, z02, then .zip LAST (data order)
	BOOST_CHECK_EQUAL((int)sets[2].Format, (int)MemberSet::mfZip);
	BOOST_REQUIRE_EQUAL(sets[2].Members.size(), 3u);
	BOOST_CHECK_EQUAL(sets[2].Members[0], 5);
	BOOST_CHECK_EQUAL(sets[2].Members[1], 7);
	BOOST_CHECK_EQUAL(sets[2].Members[2], 6);

	// 7z splits ordered by suffix number
	BOOST_CHECK_EQUAL((int)sets[3].Format, (int)MemberSet::mfSevenZip);
	BOOST_REQUIRE_EQUAL(sets[3].Members.size(), 2u);
	BOOST_CHECK_EQUAL(sets[3].Members[0], 9);
	BOOST_CHECK_EQUAL(sets[3].Members[1], 8);

	// raw splits of a media file
	BOOST_CHECK_EQUAL((int)sets[4].Format, (int)MemberSet::mfSplit);
	BOOST_REQUIRE_EQUAL(sets[4].Members.size(), 2u);
	BOOST_CHECK_EQUAL(sets[4].Members[0], 10);
	BOOST_CHECK_EQUAL(sets[4].Members[1], 11);

	// bare media singleton; par2/txt and the gapped rar set are absent
	BOOST_CHECK_EQUAL((int)sets[5].Format, (int)MemberSet::mfBare);
	BOOST_REQUIRE_EQUAL(sets[5].Members.size(), 1u);
	BOOST_CHECK_EQUAL(sets[5].Members[0], 12);
}

BOOST_AUTO_TEST_CASE(ContentMapperSplitAndBareMapTest)
{
	std::vector<SetMember> members = {
		{"movie.mkv.001", 0}, {"movie.mkv.002", 0}, {"bare.mkv", 0}, {"clip.avi.001", 0}};
	MemorySourceSet sources;
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(Pattern(30, 1)));
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(Pattern(12, 2)));
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(Pattern(50, 3)));
	sources.Sources.push_back(std::make_unique<MemoryContentSource>(Pattern(7, 4)));

	std::string skipReason;

	// raw splits concatenate: inner name loses the numeric suffix
	MemberSet splitSet{MemberSet::mfSplit, {0, 1}};
	std::unique_ptr<ContentMap> splitMap =
		ContentMapper::BuildMap(members, splitSet, sources, skipReason);
	BOOST_REQUIRE(splitMap);
	BOOST_CHECK_EQUAL(splitMap->GetInnerName(), "movie.mkv");
	BOOST_CHECK_EQUAL(splitMap->GetInnerSize(), 42);
	BOOST_REQUIRE_EQUAL(splitMap->GetRuns()->size(), 2u);
	BOOST_CHECK_EQUAL((*splitMap->GetRuns())[1].InnerOffset, 30);
	BOOST_CHECK_EQUAL((*splitMap->GetRuns())[1].MemberOffset, 0);

	// bare media is the identity map
	MemberSet bareSet{MemberSet::mfBare, {2}};
	std::unique_ptr<ContentMap> bareMap =
		ContentMapper::BuildMap(members, bareSet, sources, skipReason);
	BOOST_REQUIRE(bareMap);
	BOOST_CHECK_EQUAL(bareMap->GetInnerName(), "bare.mkv");
	BOOST_CHECK_EQUAL(bareMap->GetInnerSize(), 50);
	BOOST_REQUIRE_EQUAL(bareMap->GetRuns()->size(), 1u);
	BOOST_CHECK_EQUAL((*bareMap->GetRuns())[0].MemberOffset, 0);

	// an unservable member kills the split map with a reason
	MemorySourceSet holed;
	holed.Sources.push_back(std::make_unique<MemoryContentSource>(Pattern(30, 1)));
	// member 1 missing entirely
	MemberSet badSet{MemberSet::mfSplit, {0, 1}};
	BOOST_CHECK(!ContentMapper::BuildMap(members, badSet, holed, skipReason));
	BOOST_CHECK(!skipReason.empty());

	// rar mapping is not implemented until Task 4 - and once it is, these
	// non-rar bytes still fail with a reason, so this assertion outlives it
	MemberSet rarSet{MemberSet::mfRar, {0}};
	BOOST_CHECK(!ContentMapper::BuildMap(members, rarSet, sources, skipReason));
	BOOST_CHECK(!skipReason.empty());
}

BOOST_AUTO_TEST_SUITE_END()
