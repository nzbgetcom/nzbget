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


#ifndef DONORHEALTH_H
#define DONORHEALTH_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/*
 * How much of a posting still exists on the news servers: a sample of its
 * articles is asked of ALL servers at once (STAT, and for a few articles also
 * BODY with the yEnc checksum verified, because a server sometimes says
 * "223" when the data is gone). The first server that really has an article
 * settles it; an article is missing only when every server said no, with at
 * least one definitive "430". Each server walks the sample at its own pace,
 * so a slow server never holds up the fast ones.
 *
 * Details that were real bugs on real servers:
 *  - A reply of 451 is an answer (some providers say it for a missing
 *    article): it keeps the connection and never pauses the server, but it
 *    isn't a definitive miss either.
 *  - A server with three transport errors in a row pauses and abstains
 *    meanwhile, so misses don't wait on it.
 *  - When the time runs out, an unsettled article counts as missing when at
 *    least half of the servers missed it and none had it; articles that exist
 *    settle at once, missing ones slowly, so dropping the unsettled ones
 *    made a 12% alive posting read 57%.
 *  - STATs are pipelined: 4 per round trip at first, doubling while a batch
 *    takes under a second (up to 16), halving above two.
 *  - Servers take turns downloading a body: the next server only if the
 *    previous one's data was bad.
 */
class DonorHealth
{
public:
	enum class Answer
	{
		None,
		Present,
		Missing,
		BodyBad,	// the server has the article but its data is bad or gone
		Error		// no usable answer: 451, an error reply, an outage
	};

	struct Health
	{
		int checked = 0;
		int present = 0;
		int missing = 0;
		int error = 0;
		int bodyChecked = 0;	// articles that also got a BODY check
		int bodyBad = 0;		// ... for which no server delivered valid data

		int Answered() const { return present + missing + error; }
		/* the present share of the answered articles; -1 when too few were
		 * answered to judge. Errors count as not present: a live article still
		 * gets a hit from another server, while a server that errors would
		 * otherwise hide every dead article. */
		double Alive() const;
		Health& operator+=(const Health& other);
	};

	// answered articles needed before a posting is judged
	static constexpr int MinKnown = 5;
	static constexpr int ServerGiveUp = 3;
	static constexpr int PipelineStart = 4;
	static constexpr int PipelineMax = 16;

	/* a probe proves a posting dead only with nothing found and enough
	 * definitive misses (an outage, where every answer is an error, never does) */
	static bool DeadProbe(const Health& health);

	struct Item
	{
		std::string messageId;
		bool body = false;
		std::shared_ptr<std::vector<std::string>> groups;
	};

	struct SampleOptions
	{
		int percent = 5;
		int minimum = 50;
		int maximum = 1000;
		int bodyPercent = 20;	// share of the sample that also gets a BODY check ...
		int maxBody = 20;		// ... at most this many
		unsigned seed = 0;
	};

	/* the articles to check: percent of the ids, at least minimum, at most
	 * maximum (deterministic), about bodyPercent of them marked for a body check */
	static std::vector<std::string> Sample(const std::vector<std::string>& ids, int percent, int minimum,
		int maximum, unsigned seed);
	static std::vector<Item> Plan(const std::vector<std::string>& ids,
		const std::shared_ptr<std::vector<std::string>>& groups, const SampleOptions& options);

	struct BodyGate
	{
		std::mutex lock;
		std::atomic<bool> ok{false};
		std::function<bool()> settled;
		bool Done() { return ok || settled(); }
	};

	struct Request
	{
		std::string messageId;
		BodyGate* gate = nullptr;	// set when the article also gets a BODY check
		std::shared_ptr<std::vector<std::string>> groups;
	};

	struct ExchangeError
	{
		std::vector<Answer> answers;	// the answers read before the connection broke (may be shorter)
		bool busy = false;	// no connection was free (nzbget's own downloads): not the server's fault
	};

	/*
	 * One news server as the check sees it: the pacing, pause and connection
	 * cap shared by every check that runs; the transport (the NNTP
	 * conversation) is a subclass's Exchange().
	 */
	class Server
	{
	public:
		explicit Server(int cap) : m_cap(cap < 1 ? 1 : cap) {}
		virtual ~Server() = default;

		int Cap() const { return m_cap; }
		int Depth() const { return m_depth; }
		bool Paused() const;
		int MaxActive() const { return m_maxActive; }

		/* the answers for a batch of articles; waits for a connection slot. Every
		 * wait ends early, with Error answers and nothing asked, once <cancelled>
		 * says so (the check is over) or DonorHealth::StopAll ran */
		std::vector<Answer> Ask(const std::vector<Request>& batch,
			const std::function<bool()>& cancelled = nullptr);

		void SetRetryAfterMs(int ms) { m_retryAfterMs = ms; }

	protected:
		/* all STATs of the batch in one write, then their replies in order, then
		 * BODY for the articles that have a gate (see BodyOnce); throws ExchangeError
		 * when the connection fails */
		virtual std::vector<Answer> Exchange(const std::vector<Request>& batch) = 0;

		/* one server at a time downloads a gated article's body, and only until one delivered */
		static Answer BodyOnce(BodyGate* gate, const std::function<Answer()>& download);

	private:
		int m_cap;
		int m_retryAfterMs = 30000;
		std::atomic<int> m_depth{PipelineStart};
		std::atomic<int> m_errors{0};
		std::atomic<long long> m_downUntilMs{0};
		std::mutex m_slotMutex;
		std::condition_variable m_slotCond;
		int m_active = 0;
		std::atomic<int> m_maxActive{0};
	};

	using ServerList = std::vector<std::shared_ptr<Server>>;

	/* one posting's articles on all servers, within the budget (the time starts now) */
	static Health CheckItems(const ServerList& servers, const std::vector<Item>& items, int budgetMs);

	struct Options
	{
		SampleOptions sample;
		int probe = 10;			// articles checked first; a posting with none found is dead
		int budgetMs = 120000;	// for each posting, counted from when its check starts
		long long deadlineMs = 0;	// CheckPostings: no posting checked past it (NowMs; 0: none)
	};

	/* a posting: a probe first; unless that proves it dead (and full is set)
	 * the rest of the sample */
	static Health CheckPosting(const ServerList& servers, const std::vector<std::string>& ids,
		const std::shared_ptr<std::vector<std::string>>& groups, const Options& options, bool full);

	/* postings checked concurrently, each reported as its check finishes */
	struct Posting
	{
		std::string key;
		std::vector<std::string> ids;
		std::shared_ptr<std::vector<std::string>> groups;
	};
	static void CheckPostings(const ServerList& servers, const std::vector<Posting>& postings,
		const Options& options, bool full, int concurrency,
		const std::function<void(const std::string& key, const Health&)>& emit);

	static long long NowMs();

	/* shutdown: the walkers of every check stop asking and end; WaitAll returns
	 * when they did (before the server pool goes away). Reset allows checks again */
	static void StopAll();
	static void WaitAll();
	static void Reset();
	static bool Stopping();
};

#endif
