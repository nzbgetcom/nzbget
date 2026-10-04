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
#include "Thread.h"
#include <functional>
#include <memory>
#include <set>
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

	/* indexes of <count> samples spread evenly over <total> articles (at the
	 * middle of each of <count> equal parts) */
	static std::vector<size_t> SampleIndexes(size_t total, int count);

	/* maps a reply to STAT: 2xx exists; "no such article" replies (41x, 42x,
	 * 43x - 430 - and 451, which some providers use) are definitive; anything
	 * else is no evidence */
	static EAnswer Classify(const char* response);

	/* the verdict: no sampled article exists anywhere and at least
	 * min(MinMissingServers, activeServers) servers found every sample
	 * missing */
	static bool IsDead(int existing, int missingServers, int activeServers);

	/* samples spread over the articles of <files>, treated as one run;
	 * <loadArticles> loads the article list of a file not started yet */
	static std::vector<Sample> SamplesOf(const std::vector<FileInfo*>& files,
		const std::function<void(FileInfo*)>& loadArticles = nullptr);

	/* starts a probe of <samples> for download <nzbId>; the thread destroys itself */
	static void Start(int nzbId, std::vector<Sample> samples);

	struct Verdict
	{
		int Existing = 0;
		int MissingServers = 0;
		int ActiveServers = 0;
		bool Finished = true;
		bool Dead() const { return Finished && IsDead(Existing, MissingServers, ActiveServers); }
	};

	/* the same check in the calling thread, within <limitSec> (a duplicate
	 * is checked this way before stream repair reads from it) */
	static Verdict Check(std::vector<Sample> samples, int limitSec);

	/* shutdown: cancels running probes, WaitAll() returns when they ended */
	static void StopAll();
	static void WaitAll();

protected:
	void Run() override;

private:
	DupeProbe(int nzbId, std::vector<Sample> samples) :
		m_nzbId(nzbId), m_samples(std::move(samples)) {}

	struct ServerResult
	{
		int Exists = 0;
		int Missing = 0;
		int Unknown = 0;
	};

	int m_nzbId;
	std::vector<Sample> m_samples;
	NntpConnection* m_connection = nullptr;

	bool ProbeServer(int serverId, std::set<int>& probed, ServerResult& result);
	Verdict Measure(int limitSec);
	void Register();
	void Unregister();
	void Cancel();
	void Abandon(int missingServers, int activeServers);
};

#endif
