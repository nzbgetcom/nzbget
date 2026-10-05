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
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#ifndef DUPESEARCH_H
#define DUPESEARCH_H

#include "Observer.h"
#include "Thread.h"
#include "DownloadInfo.h"
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include "DeadPostings.h"
#include "NzbFetcher.h"

/*
 * Option <DupeSearch>: looks for other postings of a new download on a
 * Newznab indexer and queues them as duplicates (see DupeSearchJob).
 *
 * This part decides WHICH downloads are searched. A download added to the
 * queue is noted, and after <DupeSearchDelay> seconds - a client that
 * submits backups of its own has added them by then - it is searched if it
 * is still the top-scored item of its duplicate key, hasn't passed the
 * download stage, isn't a duplicate this search queued itself, and its key
 * wasn't searched lately (at most one search per key within SearchWindowSec
 * unless a pick with a higher score arrives). What was searched is kept
 * across restarts, and a restart looks at the queue once for downloads whose
 * add event it missed.
 */
class DupeSearch : public Thread
{
public:
	DupeSearch();
	~DupeSearch() override;
	void Run() override;
	void Stop() override;

	// seconds a key is not searched again, unless a higher-scored pick arrives
	static constexpr int SearchWindowSec = 6 * 3600;
	// keys are forgotten after this long
	static constexpr int StateTtlSec = 30 * 24 * 3600;
	// the score of a managed pick, and the room below it for donors
	static constexpr int BasePickScore = 1000000;
	// search and fetch of one pick are meant to fit in this long
	static constexpr int SearchDeadlineSec = 60;

	// parameters of the duplicates this search queued: a marker, and the
	// share of the posting found alive (percent)
	static constexpr const char* DonorParam = "DupeSearch";
	static constexpr const char* AliveParam = "DupeAlive";

	/* "dupes:" + the normalized title: the duplicate key of a download that has none */
	static std::string MakeDupeKey(const std::string& name);

	/* has this search queued the nzb itself, or has the nzbget-dupe-proxy
	 * (it marks its duplicates with DupeAlive)? */
	static bool IsDonor(NzbInfo* nzbInfo);

private:
	class DownloadQueueObserver final : public Observer
	{
	public:
		DupeSearch* m_owner;
		void Update(Subject* caller, void* aspect) override { m_owner->DownloadQueueUpdate(aspect); }
	};

	struct Searched
	{
		int score = 0;
		time_t time = 0;
	};

	struct Job
	{
		int nzbId = 0;
		std::string name;
		std::string dupeKey;
		std::string category;
		std::string imdb;	// from the nzb-file's meta data
		std::string tvdb;
		std::string queuedFile;	// the nzb-file of the pick
		std::vector<std::string> knownFiles;	// the nzb-files nzbget keeps for this release (queue and history)
		// the duplicates of the key nzbget keeps in history (DELETED/DUPE),
		// whoever sent them: id and the item's own nzb-file
		std::vector<std::pair<int, std::string>> members;
		int score = 0;
	};

	// postings fetched at the same time
	static constexpr int FetchParallel = 4;

	DownloadQueueObserver m_observer;
	NzbFetcher m_fetcher;
	DeadPostings m_dead;
	std::mutex m_mutex;
	std::condition_variable m_cond;
	std::map<int, time_t> m_pending;			// nzb id -> when to look at it
	std::map<std::string, Searched> m_searched;	// lowercase dupe key -> last search
	std::map<std::string, std::set<std::string>> m_sent;	// lowercase dupe key -> fingerprints queued

	void DownloadQueueUpdate(void* aspect);
	void Schedule(int nzbId, time_t due);
	void ScanQueue();
	bool Prepare(DownloadQueue* downloadQueue, int nzbId, Job& job);
	void Search(const Job& job);
	/* health checks, adds and ranks the verified postings and the key's duplicates in history */
	void Place(const Job& job, const NzbSummary& pick, std::vector<NzbFetcher::Fetched>& verified,
		std::map<std::string, int>& rejected, int results, int candidates, int postings);
	void Collect(DownloadQueue* downloadQueue, NzbInfo* nzbInfo, Job& job);

	/*
	 * A search in progress survives a restart: QueueDir/dupesearch-pending/<nzb id>/
	 * holds a phase file ("searching", then "fetched" once the fetched nzb-files
	 * are saved beside it as <n>.nzb and <n>.meta). At startup a fetched search
	 * resumes from the saved files (no search, no fetch: grabs are scarce); one
	 * still searching is searched again; one whose pick is gone is dropped.
	 */
	/* a line of the search of pick <nzbId>: in the pick's own log and the global log */
	void Note(int nzbId, Message::EKind kind, const char* format, ...) PRINTF_SYNTAX(4);
	std::string PendingDir(int nzbId);
	void MarkSearching(int nzbId);
	void SavePending(const Job& job, const std::vector<NzbFetcher::Fetched>& verified);
	void RemovePending(int nzbId);
	void ResumePending();
	/* why the search may no longer add duplicates for the pick (deleted by the user,
	 * another duplicate key), empty while it may */
	std::string PickGone(const Job& job);
	/* queues a posting as a duplicate of the pick; the download id, 0 when not added */
	int AddDonor(const Job& job, const NzbFetcher::Fetched& posting, int score, double alive);
	/* the DupeScore (and a parameter "Name=Value") of a duplicate that sits in history or in the queue */
	bool SetScore(int id, int score, const std::string& param);
	void LoadState();
	void SaveState();
	std::string StatePath();
};

extern DupeSearch* g_DupeSearch;

#endif
