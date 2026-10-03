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
#include "DupeCoordinator.h"
#include "Options.h"

BOOST_AUTO_TEST_SUITE(QueueTest)

namespace
{

class StaticDownloadQueue final : public DownloadQueue
{
public:
	StaticDownloadQueue() { Init(this); }
	~StaticDownloadQueue() { Final(); }
	bool EditEntry(int, EEditAction, const char*) override { return false; }
	bool EditList(IdList*, NameList*, EMatchMode, EEditAction, const char*) override { return false; }
	void HistoryChanged() override {}
	void Save() override {}
	void SaveChanged() override {}
};

std::unique_ptr<NzbInfo> MakeNzb(const char* name, const char* dupeKey, int dupeScore)
{
	std::unique_ptr<NzbInfo> nzbInfo = std::make_unique<NzbInfo>();
	nzbInfo->SetKind(NzbInfo::nkNzb);
	nzbInfo->SetName(name);
	nzbInfo->SetDupeKey(dupeKey);
	nzbInfo->SetDupeScore(dupeScore);
	nzbInfo->SetDupeMode(dmScore);
	return nzbInfo;
}

// a history item deleted as duplicate backup: healthy (nothing failed) and
// not marked bad, so ReturnBestDupe may move it back to the queue
NzbInfo* AddBackup(DownloadQueue& queue, const char* name, const char* dupeKey, int dupeScore)
{
	std::unique_ptr<NzbInfo> nzbInfo = MakeNzb(name, dupeKey, dupeScore);
	nzbInfo->SetDeleteStatus(NzbInfo::dsDupe);
	NzbInfo* raw = nzbInfo.get();
	queue.GetHistory()->Add(std::make_unique<HistoryInfo>(std::move(nzbInfo)), true);
	return raw;
}

} // namespace

BOOST_AUTO_TEST_CASE(DupeFailoverWarrantedTest)
{
	// a posting at 30% health warrants 30% of the score it was queued with
	BOOST_CHECK(DupeCoordinator::DupeFailoverWarranted(90, 300, 27));
	BOOST_CHECK(!DupeCoordinator::DupeFailoverWarranted(90, 300, 26));
	// a dead posting warrants nothing: any backup not scored below zero is better
	BOOST_CHECK(DupeCoordinator::DupeFailoverWarranted(90, 0, 0));
	BOOST_CHECK(!DupeCoordinator::DupeFailoverWarranted(90, 0, -5));
	// at full health only an equal or better backup qualifies
	BOOST_CHECK(DupeCoordinator::DupeFailoverWarranted(90, 1000, 90));
	BOOST_CHECK(!DupeCoordinator::DupeFailoverWarranted(90, 1000, 89));
	// items queued without a score (0) warrant nothing, like ReturnBestDupe
	BOOST_CHECK(DupeCoordinator::DupeFailoverWarranted(0, 1000, 0));
	BOOST_CHECK(DupeCoordinator::DupeFailoverWarranted(-10, 500, 0));
	// health outside the permille range is clamped
	BOOST_CHECK(!DupeCoordinator::DupeFailoverWarranted(90, 1500, 89));
	BOOST_CHECK(DupeCoordinator::DupeFailoverWarranted(90, -7, 0));
}

BOOST_AUTO_TEST_CASE(DupeCoordinatorFindDupeBackupTest)
{
	Options::CmdOptList cmdOpts;
	Options options(&cmdOpts, nullptr);
	StaticDownloadQueue queue;
	DupeCoordinator coordinator;

	std::unique_ptr<NzbInfo> queued = MakeNzb("Rel.2026.1080p-A", "imdb:1", 10);
	NzbInfo* item = queued.get();
	queue.GetQueue()->Add(std::move(queued), false);

	// nothing in history
	BOOST_CHECK(coordinator.FindDupeBackup(&queue, item, item->GetName(), item->GetDupeKey()) == nullptr);

	// the highest-scored healthy backup of the same key wins; other keys are ignored
	AddBackup(queue, "Rel.2026.1080p-B", "imdb:1", 50);
	NzbInfo* best = AddBackup(queue, "Rel.2026.1080p-C", "imdb:1", 90);
	AddBackup(queue, "Other.2026-X", "imdb:2", 100);
	HistoryInfo* backup = coordinator.FindDupeBackup(&queue, item, item->GetName(), item->GetDupeKey());
	BOOST_REQUIRE(backup);
	BOOST_CHECK(backup->GetNzbInfo() == best);

	// a backup marked bad, or one that failed on its own, is no candidate
	best->SetMarkStatus(NzbInfo::ksBad);
	backup = coordinator.FindDupeBackup(&queue, item, item->GetName(), item->GetDupeKey());
	BOOST_REQUIRE(backup);
	BOOST_CHECK_EQUAL(backup->GetNzbInfo()->GetDupeScore(), 50);

	// a queued duplicate with a higher score than every backup blocks them
	std::unique_ptr<NzbInfo> rival = MakeNzb("Rel.2026.1080p-D", "imdb:1", 60);
	NzbInfo* rivalRaw = rival.get();
	queue.GetQueue()->Add(std::move(rival), false);
	BOOST_CHECK(coordinator.FindDupeBackup(&queue, item, item->GetName(), item->GetDupeKey()) == nullptr);

	// the item itself never counts as its own rival
	item->SetDupeScore(99);
	queue.GetQueue()->Remove(rivalRaw);
	backup = coordinator.FindDupeBackup(&queue, item, item->GetName(), item->GetDupeKey());
	BOOST_REQUIRE(backup);
	BOOST_CHECK_EQUAL(backup->GetNzbInfo()->GetDupeScore(), 50);

	// a good-duplicate in history ends the search
	std::unique_ptr<NzbInfo> good = MakeNzb("Rel.2026.1080p-G", "imdb:1", 1);
	good->SetMarkStatus(NzbInfo::ksGood);
	queue.GetHistory()->Add(std::make_unique<HistoryInfo>(std::move(good)), true);
	BOOST_CHECK(coordinator.FindDupeBackup(&queue, item, item->GetName(), item->GetDupeKey()) == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
