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


#ifndef DUPEPROBE_H
#define DUPEPROBE_H

#include "NString.h"
#include "Log.h"
#include "Thread.h"
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

class NntpConnection;
class FileInfo;

/*
 * Early dead-posting check for option <HealthCheck> value "dupe".
 *
 * A posting whose articles exist on no server is only noticed by failing
 * them, and a large one needs thousands of failures before its health drops
 * below critical. When a duplicate waits in history, the download is probed
 * instead as soon as it starts: a handful of articles spread across the whole
 * posting are checked (STAT) on every active server. Only when none of them
 * exists anywhere, and enough servers answered definitively, is the
 * download abandoned for the duplicate. A posting with any article alive is
 * never abandoned here: the regular health check handles partial damage.
 */
class DupeProbe : public Thread
{
public:
	struct Sample
	{
		CString MessageId;
		std::shared_ptr<std::vector<CString>> Groups;
	};

	enum EAnswer
	{
		daExists,
		daMissing,
		daUnknown	// no answer, an error, an authentication problem: no evidence
	};

	// articles checked per server
	static constexpr int SampleCount = 10;
	// postings of fewer articles than this are not probed
	static constexpr int MinArticles = 4;
	// servers that must answer definitively for a verdict, if there are that many
	static constexpr int MinMissingServers = 5;
	// samples that must exist somewhere for a download to count as alive: a single
	// stray article doesn't keep a dead posting going (B45)
	static constexpr int MinAliveSamples = 2;

	/* indexes of <count> samples spread evenly over <total> articles (at the
	 * middle of each of <count> equal parts) */
	static std::vector<size_t> SampleIndexes(size_t total, int count);

	/* maps a reply to STAT: 2xx exists; "no such article" replies (41x, 42x,
	 * 43x - 430 - and 451, which some providers use) are definitive; anything
	 * else is no evidence */
	static EAnswer Classify(const char* response);

	/* the verdict: fewer than MinAliveSamples sampled articles exist anywhere,
	 * and at least min(MinMissingServers, activeServers) servers answered every
	 * sample they were asked definitively (<definitiveServers>) */
	static bool IsDead(int existing, int definitiveServers, int activeServers);

	/* samples spread over the articles of <files>, treated as one run;
	 * <loadArticles> loads the article list of a file not started yet */
	static std::vector<Sample> SamplesOf(const std::vector<FileInfo*>& files,
		const std::function<void(FileInfo*)>& loadArticles = nullptr);

	/* starts a probe of <samples> for download <nzbId>; the thread destroys itself.
	 * <recoveredAtStart>: the download's count of articles borrowed from duplicates
	 * when it started - those prove nothing about its own posting */
	static void Start(int nzbId, std::vector<Sample> samples, int recoveredAtStart = 0);

	struct Verdict
	{
		int Existing = 0;			// samples found on some server
		int MissingServers = 0;		// servers that answered every sample asked definitively
		std::string FoundOn;		// where the first sample found was, for the log
		int ActiveServers = 0;
		int ReachedServers = 0;	// servers asked at all (a free connection was had)
		bool Finished = true;
		bool Dead() const { return Finished && IsDead(Existing, MissingServers, ActiveServers); }
	};

	/* the same check in the calling thread, within <limitSec> (a duplicate
	 * is checked this way before stream repair reads from it) */
	static Verdict Check(std::vector<Sample> samples, int limitSec);

	/*
	 * A download that failed with articles missing: <samples> of its failed
	 * articles are asked of every server. When most of them exist after all (a
	 * timeout or a dropped connection lost them, as ArticleRetries=0 makes
	 * likely), its failed articles are retried once ("Retry failed articles").
	 * Runs in its own thread; the download is in history by id <nzbId>.
	 */
	static void StartRecheck(int nzbId, std::vector<Sample> samples);
	// the most failed articles asked of the servers, and the time for it
	static constexpr int RecheckSampleCount = 10;
	static constexpr int RecheckLimitSec = 60;

	/* shutdown: cancels running probes and refuses new ones; WaitAll() returns
	 * when they ended. Reset() allows probes again (a reload's new coordinator) */
	static void StopAll();
	/* seconds a dead-pick probe of download <nzbId> has been running (its verdict
	 * comes first), or -1 when none runs */
	static int ProbingFor(int nzbId);
	static void WaitAll();
	static void Reset();

protected:
	void Run() override;

private:
	DupeProbe(int nzbId, std::vector<Sample> samples) :
		m_nzbId(nzbId), m_samples(std::move(samples)), m_found(m_samples.size(), 0) {}

	struct ServerResult
	{
		int Exists = 0;
		int Missing = 0;
		int Unknown = 0;
	};

	int m_nzbId;
	time_t m_started = 0;
	int m_recoveredAtStart = 0;
	std::vector<Sample> m_samples;
	// recheck mode: every sample is asked of every server until found somewhere
	bool m_countAll = false;
	// samples found on some server: a later server isn't asked them again
	std::vector<char> m_found;
	std::string m_foundOn;
	NntpConnection* m_connection = nullptr;

	bool ProbeServer(int serverId, std::set<int>& probed, ServerResult& result);
	/* writes to the download's own log (seen in the web interface even when
	 * detail messages go nowhere); detail() if the download is gone */
	void Note(Message::EKind kind, const char* format, ...);
	Verdict Measure(int limitSec);
	void Recheck();
	/* false once StopAll ran: the probe must not run */
	bool Register();
	void Unregister();
	void Cancel();
	void Abandon(int missingServers, int activeServers);
};

#endif
